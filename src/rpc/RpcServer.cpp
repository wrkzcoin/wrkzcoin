// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <rpc/RpcServer.h>
//////////////////////////

#include <algorithm>
#include <iostream>
#include <ctime>
#include <thread>

#include "version.h"

#include <config/Constants.h>
#include <common/CryptoNoteTools.h>
#include <errors/ValidateParameters.h>
#include <logger/Logger.h>
#include <serialization/SerializationTools.h>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/ParseExtra.h>

namespace
{
    /* httplib dedicates a pool thread to a connection for as long as that
     * connection stays alive, so enabling keep-alive on the clients means idle
     * connections now hold threads that used to be released immediately. The
     * default pool is only max(8, cores - 1), which a handful of syncing
     * wallets can occupy on their own. Give the RPC server a wider floor and a
     * shorter idle window so a slow client cannot starve the others. */
    void applyServerTuning(httplib::Server &srv)
    {
        const unsigned int cores = std::thread::hardware_concurrency();

        const size_t baseThreads = std::max<size_t>(32, cores > 0 ? cores * 2 : 0);
        const size_t maxThreads = baseThreads * 8;

        srv.new_task_queue = [baseThreads, maxThreads] {
            return new httplib::ThreadPool(baseThreads, maxThreads);
        };

        /* Nagle turns a request/response protocol into a per-request stall. */
        srv.set_tcp_nodelay(true);

        /* Release an idle keep-alive connection's thread quickly, while still
         * covering the gap between a wallet's back to back sync requests. */
        srv.set_keep_alive_timeout(3);
        srv.set_keep_alive_max_count(1000);
    }
} // namespace

RpcServer::RpcServer(
    const uint16_t bindPort,
    const std::string rpcBindIp,
    const std::string rpcBindIpv6Address,
    const bool rpcUseIpv6,
    const std::string corsHeader,
    const std::string rpcAccessToken,
    const uint32_t rpcReadTimeout,
    const uint32_t rpcWriteTimeout,
    const uint64_t rpcMaxRequestBodyBytes,
    const uint32_t rpcMaxRequestsPerMinute,
    const uint32_t rpcMaxGlobalIndexesRange,
    const uint32_t rpcMaxBlockCount,
    const bool rpcTrustProxy,
    const RpcMode rpcMode,
    const std::shared_ptr<CryptoNote::Core> core,
    const std::shared_ptr<CryptoNote::NodeServer> p2p,
    const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> syncManager):
    m_port(bindPort),
    m_host(rpcBindIp),
    m_ipv6Host(rpcUseIpv6 && !rpcBindIpv6Address.empty() ? rpcBindIpv6Address : ""),
    m_corsHeader(corsHeader),
    m_rpcAccessToken(rpcAccessToken),
    m_rpcReadTimeout(std::max<uint32_t>(1, rpcReadTimeout)),
    m_rpcWriteTimeout(std::max<uint32_t>(1, rpcWriteTimeout)),
    m_rpcMaxRequestBodyBytes(std::max<uint64_t>(1024, rpcMaxRequestBodyBytes)),
    m_rpcMaxRequestsPerMinute(rpcMaxRequestsPerMinute),
    m_rpcMaxGlobalIndexesRange(std::max<uint32_t>(100, rpcMaxGlobalIndexesRange)),
    m_rpcMaxBlockCount(std::max<uint32_t>(1, rpcMaxBlockCount)),
    m_rpcTrustProxy(rpcTrustProxy),
    m_rpcMode(rpcMode),
    m_core(core),
    m_p2p(p2p),
    m_syncManager(syncManager)
{
    applyServerTuning(m_server);
    m_server.set_address_family(AF_INET);
    m_server.set_read_timeout(std::chrono::seconds(m_rpcReadTimeout));
    m_server.set_write_timeout(std::chrono::seconds(m_rpcWriteTimeout));
    m_server.set_payload_max_length(static_cast<size_t>(m_rpcMaxRequestBodyBytes));
    m_server.set_error_logger([](const httplib::Error &error, const httplib::Request *) {
        Logger::logger.log(
            "RPC server startup error: " + httplib::to_string(error),
            Logger::WARNING,
            { Logger::DAEMON_RPC }
        );
    });

    setupRoutes(m_server);

    if (!m_ipv6Host.empty())
    {
        applyServerTuning(m_ipv6Server);
        m_ipv6Server.set_address_family(AF_INET6);
        m_ipv6Server.set_ipv6_v6only(true);
        m_ipv6Server.set_read_timeout(std::chrono::seconds(m_rpcReadTimeout));
        m_ipv6Server.set_write_timeout(std::chrono::seconds(m_rpcWriteTimeout));
        m_ipv6Server.set_payload_max_length(static_cast<size_t>(m_rpcMaxRequestBodyBytes));
        m_ipv6Server.set_error_logger([](const httplib::Error &error, const httplib::Request *) {
            Logger::logger.log(
                "RPC IPv6 server startup error: " + httplib::to_string(error),
                Logger::WARNING,
                { Logger::DAEMON_RPC }
            );
        });
        setupRoutes(m_ipv6Server);
    }
}

void RpcServer::setupRoutes(httplib::Server &srv)
{
    const bool bodyRequired = true;
    const bool bodyNotRequired = false;

    const bool syncRequired = true;
    const bool syncNotRequired = false;

    /* Route the request through our middleware function, before forwarding
       to the specified function */
    const auto router = [this](const auto function, const RpcMode routePermissions, const bool isBodyRequired, const bool syncRequired) {
        return [=](const httplib::Request &req, httplib::Response &res) {
            /* Pass the inputted function with the arguments passed through
               to middleware */
            middleware(
                req,
                res,
                routePermissions,
                isBodyRequired,
                syncRequired,
                std::bind(function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            );
        };
    };

    const auto jsonRpc = [this, router, bodyRequired, bodyNotRequired, syncRequired, syncNotRequired](const auto &req, auto &res) {
        const auto body = getJsonBody(req, res, true);

        if (!body)
        {
            return;
        }

        if (!hasMember(*body, "method"))
        {
            failRequest(400, "Missing JSON parameter: 'method'", res);
            return;
        }

        const auto method = getStringFromJSON(*body, "method");

        if (method == "getblocktemplate")
        {
            router(&RpcServer::getBlockTemplate, RpcMode::Standard, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "submitblock")
        {
            router(&RpcServer::submitBlock, RpcMode::Standard, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "getblockcount")
        {
            router(&RpcServer::getBlockCount, RpcMode::Standard, bodyNotRequired, syncNotRequired)(req, res);
        }
        else if (method == "getlastblockheader")
        {
            router(&RpcServer::getLastBlockHeader, RpcMode::Standard, bodyNotRequired, syncNotRequired)(req, res);
        }
        else if (method == "getblockheaderbyhash")
        {
            router(&RpcServer::getBlockHeaderByHash, RpcMode::Standard, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "getblockheaderbyheight")
        {
            router(&RpcServer::getBlockHeaderByHeight, RpcMode::Standard, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_blocks_list_json")
        {
            router(&RpcServer::getBlocksByHeight, RpcMode::Explorer, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_block_json")
        {
            router(&RpcServer::getBlockDetailsByHash, RpcMode::Explorer, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_transaction_json")
        {
            router(&RpcServer::getTransactionDetailsByHash, RpcMode::Explorer, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_on_transactions_pool_json")
        {
            router(&RpcServer::getTransactionsInPool, RpcMode::Explorer, bodyNotRequired, syncNotRequired)(req, res);
        }
        else
        {
            res.status = 404;
        }
    };

    /* Note: /json_rpc is exposed on both GET and POST */
    srv.Get("/json_rpc", jsonRpc)
       .Get("/info", router(&RpcServer::info, RpcMode::Standard, bodyNotRequired, syncNotRequired))
       .Get("/height", router(&RpcServer::height, RpcMode::Standard, bodyNotRequired, syncNotRequired))
       .Get("/peers", router(&RpcServer::peers, RpcMode::Standard, bodyNotRequired, syncNotRequired))

       .Post("/json_rpc", jsonRpc)
       .Post("/sendrawtransaction", router(&RpcServer::sendTransaction, RpcMode::Standard, bodyRequired, syncRequired))
       .Post("/getrandom_outs", router(&RpcServer::getRandomOuts, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/getwalletsyncdata", router(&RpcServer::getWalletSyncData, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/get_global_indexes_for_range", router(&RpcServer::getGlobalIndexes, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/queryblockslite", router(&RpcServer::queryBlocksLite, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/get_transactions_status", router(&RpcServer::getTransactionsStatus, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/get_pool_changes_lite", router(&RpcServer::getPoolChanges, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/queryblocksdetailed", router(&RpcServer::queryBlocksDetailed, RpcMode::Explorer, bodyRequired, syncNotRequired))
       .Post("/get_o_indexes", router(&RpcServer::getGlobalIndexesDeprecated, RpcMode::Standard, bodyRequired, syncNotRequired))
       .Post("/getrawblocks", router(&RpcServer::getRawBlocks, RpcMode::Standard, bodyRequired, syncNotRequired))

       /* Matches everything */
       /* NOTE: Not passing through middleware */
       .Options(".*", [this](auto &req, auto &res) { handleOptions(req, res); });
}

RpcServer::~RpcServer()
{
    stop();
}

void RpcServer::start()
{
    m_serverThread = std::thread(&RpcServer::listen, this);

    if (!m_ipv6Host.empty())
    {
        m_ipv6Thread = std::thread(&RpcServer::listenIpv6, this);
    }
}

void RpcServer::listen()
{
    const auto isListening = m_server.listen(m_host, m_port);

    if (!isListening)
    {
        std::cout << WarningMsg("Failed to start RPC server.") << std::endl;
        exit(1);
    }
}

void RpcServer::listenIpv6()
{
    const auto isListening = m_ipv6Server.listen(m_ipv6Host, m_port);

    if (!isListening)
    {
        std::cout << WarningMsg("Failed to start IPv6 RPC server on [")
                  << WarningMsg(m_ipv6Host)
                  << WarningMsg("].") << std::endl;
    }
}

void RpcServer::stop()
{
    m_server.stop();

    if (!m_ipv6Host.empty())
    {
        m_ipv6Server.stop();
    }

    if (m_serverThread.joinable())
    {
        m_serverThread.join();
    }

    if (m_ipv6Thread.joinable())
    {
        m_ipv6Thread.join();
    }
}

std::tuple<std::string, uint16_t> RpcServer::getConnectionInfo()
{
    return {m_host, m_port};
}

std::optional<nlohmann::json> RpcServer::getJsonBody(
    const httplib::Request &req,
    httplib::Response &res,
    const bool bodyRequired)
{
    if (!bodyRequired)
    {
        return nlohmann::json{};
    }

    try
    {
        return nlohmann::json::parse(req.body);
    }
    catch (const nlohmann::json::parse_error &)
    {
        std::stringstream stream;

        if (!req.body.empty())
        {
            stream << "Warning: received body is not JSON encoded!\n"
                   << "Key/value parameters are NOT supported.\n"
                   << "Body:\n" << req.body;

            Logger::logger.log(
                stream.str(),
                Logger::INFO,
                { Logger::DAEMON_RPC }
            );
        }

        stream << "Failed to parse request body as JSON";

        failRequest(400, stream.str(), res);

        return std::nullopt;
    }
}

void RpcServer::middleware(
    const httplib::Request &req,
    httplib::Response &res,
    const RpcMode routePermissions,
    const bool bodyRequired,
    const bool syncRequired,
    std::function<std::tuple<Error, uint16_t>(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body)> handler)
{
    const std::string clientIp = getClientIp(req);

    Logger::logger.log(
        "[" + clientIp + "] Incoming " + req.method + " request: " + req.path + ", User-Agent: " + req.get_header_value("User-Agent"),
        Logger::DEBUG,
        { Logger::DAEMON_RPC }
    );

    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);
    }

    res.set_header("Content-Type", "application/json");

    if (req.body.size() > m_rpcMaxRequestBodyBytes)
    {
        failRequest(413, "RPC request body too large", res);
        return;
    }

    if (!m_rpcAccessToken.empty())
    {
        std::string providedToken = req.get_header_value("X-API-Key");

        if (providedToken.empty())
        {
            const std::string authHeader = req.get_header_value("Authorization");
            static const std::string bearerPrefix = "Bearer ";
            if (authHeader.rfind(bearerPrefix, 0) == 0)
            {
                providedToken = authHeader.substr(bearerPrefix.size());
            }
        }

        if (providedToken != m_rpcAccessToken)
        {
            failRequest(401, "Unauthorized RPC request", res);
            return;
        }
    }

    if (!clientIp.empty() && clientIp != "127.0.0.1" && clientIp != "::1")
    {
        if (isRateLimited(clientIp))
        {
            failRequest(429, "Too many RPC requests, please retry later", res);
            return;
        }
    }

    const auto jsonBody = getJsonBody(req, res, bodyRequired);

    if (!jsonBody)
    {
        return;
    }

    /* If this route requires higher permissions than we have enabled, then
     * reject the request */
    if (routePermissions > m_rpcMode)
    {
        std::stringstream stream;

        stream << "You do not have permission to access this method. Please relaunch your daemon with "
                  "--daemon-mode explorer to access explorer RPC methods.";

        failRequest(403, stream.str(), res);

        return;
    }

    const uint64_t height = m_core->getTopBlockIndex() + 1;
    const uint64_t networkHeight = std::max(1u, m_syncManager->getBlockchainHeight());

    const bool areSynced = m_p2p->get_payload_object().isSynchronized() && height >= networkHeight;

    if (syncRequired && !areSynced)
    {
        failRequest(503, "Daemon must be synced to process this RPC method call, please retry when synced", res);
        return;
    }

    try
    {
        const auto [error, statusCode] = handler(req, res, *jsonBody);

        if (error)
        {
            nlohmann::json j;
            j["errorCode"] = error.getErrorCode();
            j["errorMessage"] = error.getErrorMessage();
            res.body = j.dump();
            res.status = statusCode;
        }
        else
        {
            res.status = statusCode;
        }

        return;
    }
    catch (const std::invalid_argument &e)
    {
        Logger::logger.log(
            "Caught JSON exception, likely missing required json parameter: " + std::string(e.what()),
            Logger::FATAL,
            { Logger::DAEMON_RPC }
        );

        failRequest(400, e.what(), res);
    }
    catch (const std::exception &e)
    {
        std::stringstream error;

        error << "Caught unexpected exception: " << e.what() << " while processing "
              << req.path << " request for User-Agent: " << req.get_header_value("User-Agent");

        Logger::logger.log(
            error.str(),
            Logger::FATAL,
            { Logger::DAEMON_RPC }
        );

        if (req.body != "")
        {
            Logger::logger.log(
                "Body: " + req.body,
                Logger::FATAL,
                { Logger::DAEMON_RPC }
            );
        }

        failRequest(500, "Internal server error: " + std::string(e.what()), res);
    }
}

std::string RpcServer::getClientIp(const httplib::Request &req) const
{
    std::string ip = req.remote_addr;

    if (ip.empty())
    {
        ip = req.get_header_value("REMOTE_ADDR");
    }

    if (!m_rpcTrustProxy)
    {
        return ip;
    }

    const std::string forwardedFor = req.get_header_value("X-Forwarded-For");
    if (forwardedFor.empty())
    {
        return ip;
    }

    const size_t comma = forwardedFor.find(',');
    if (comma == std::string::npos)
    {
        return forwardedFor;
    }

    return forwardedFor.substr(0, comma);
}

bool RpcServer::isRateLimited(const std::string &clientIp)
{
    if (m_rpcMaxRequestsPerMinute == 0)
    {
        return false;
    }

    const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
    const uint64_t windowStart = now - (now % 60);

    std::lock_guard<std::mutex> lock(m_rateLimitMutex);
    auto &entry = m_rateLimitByIp[clientIp];

    if (entry.first != windowStart)
    {
        entry.first = windowStart;
        entry.second = 0;
    }

    if (entry.second >= m_rpcMaxRequestsPerMinute)
    {
        return true;
    }

    ++entry.second;
    return false;
}

void RpcServer::failRequest(uint16_t statusCode, std::string body, httplib::Response &res)
{
    nlohmann::json j;
    j["status"] = "Failed";
    j["error"] = body;
    res.body = j.dump();
    res.status = statusCode;
}

void RpcServer::failJsonRpcRequest(
    const int64_t errorCode,
    const std::string errorMessage,
    httplib::Response &res)
{
    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["error"] = {
        {"message", errorMessage},
        {"code", errorCode}
    };
    res.body = j.dump();
    res.status = 200;
}

void RpcServer::handleOptions(const httplib::Request &req, httplib::Response &res) const
{
    Logger::logger.log(
        "Incoming " + req.method + " request: " + req.path,
        Logger::DEBUG,
        { Logger::DAEMON_RPC }
    );

    std::string supported = "OPTIONS, GET, POST";

    if (m_corsHeader == "")
    {
        supported = "";
    }

    if (req.has_header("Access-Control-Request-Method"))
    {
        res.set_header("Access-Control-Allow-Methods", supported);
    }
    else
    {
        res.set_header("Allow", supported);
    }

    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);
        res.set_header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    }

    res.status = 200;
}

std::tuple<Error, uint16_t> RpcServer::info(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    try
    {
        const uint64_t height = m_core->getTopBlockIndex() + 1;
        const uint64_t networkHeight = std::max(1u, m_syncManager->getBlockchainHeight());
        const auto blockDetails = m_core->getBlockDetails(m_core->getTopBlockIndex());
        const uint64_t difficulty = m_core->getDifficultyForNextBlock();

        uint64_t total_conn = m_p2p->get_connections_count();
        uint64_t outgoing_connections_count = m_p2p->get_outgoing_connections_count();

        nlohmann::json upgradeHeights = nlohmann::json::array();
        for (const uint64_t h : CryptoNote::parameters::FORK_HEIGHTS)
        {
            upgradeHeights.push_back(h);
        }

        nlohmann::json j;
        j["height"] = height;
        j["difficulty"] = difficulty;
        /* Transaction count without coinbase transactions - one per block, so subtract height */
        j["tx_count"] = m_core->getBlockchainTransactionCount() - height;
        j["tx_pool_size"] = m_core->getPoolTransactionCount();
        j["alt_blocks_count"] = m_core->getAlternativeBlockCount();
        j["outgoing_connections_count"] = outgoing_connections_count;
        j["incoming_connections_count"] = total_conn - outgoing_connections_count;
        j["white_peerlist_size"] = m_p2p->getPeerlistManager().get_white_peers_count();
        j["grey_peerlist_size"] = m_p2p->getPeerlistManager().get_gray_peers_count();
        j["last_known_block_index"] = std::max(1u, m_syncManager->getObservedHeight()) - 1;
        j["network_height"] = networkHeight;
        j["upgrade_heights"] = upgradeHeights;
        j["supported_height"] = CryptoNote::parameters::FORK_HEIGHTS_SIZE == 0
            ? 0
            : CryptoNote::parameters::FORK_HEIGHTS[CryptoNote::parameters::CURRENT_FORK_INDEX];
        j["hashrate"] = static_cast<uint64_t>(round(difficulty / CryptoNote::parameters::DIFFICULTY_TARGET));
        j["synced"] = (height == networkHeight);
        j["pruned"] = m_syncManager->isPrunedNode();
        j["prune_depth"] = m_syncManager->getPrunedNodeDepth();
        j["prune_capability_active"] = m_syncManager->isPruneCapabilityActive();
        j["sync_active_peers"] = m_syncManager->getSyncActivePeers();
        j["sync_avg_batch_size"] = m_syncManager->getSyncAvgBatchSize();
        j["sync_demoted_peers"] = m_syncManager->getSyncDemotedPeers();
        j["major_version"] = blockDetails.majorVersion;
        j["minor_version"] = blockDetails.minorVersion;
        j["version"] = PROJECT_VERSION;
        j["status"] = "OK";
        j["start_time"] = m_core->getStartTime();

        res.body = j.dump();

        return {SUCCESS, 200};
    }
    catch (const std::exception &)
    {
        nlohmann::json j;
        j["status"] = "BUSY";
        j["error"] = "Chain is reorganizing, please retry shortly";
        res.body = j.dump();
        return {SUCCESS, 503};
    }
}

std::tuple<Error, uint16_t> RpcServer::height(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    nlohmann::json j;
    j["height"] = m_core->getTopBlockIndex() + 1;
    j["network_height"] = std::max(1u, m_syncManager->getBlockchainHeight());
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::peers(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    std::list<PeerlistEntry> peers_white;
    std::list<PeerlistEntry> peers_gray;

    m_p2p->getPeerlistManager().get_peerlist_full(peers_gray, peers_white);

    nlohmann::json peersArr = nlohmann::json::array();
    for (const auto &peer : peers_white)
    {
        std::stringstream stream;
        stream << peer.adr;
        peersArr.push_back(stream.str());
    }

    nlohmann::json peersGrayArr = nlohmann::json::array();
    for (const auto &peer : peers_gray)
    {
        std::stringstream stream;
        stream << peer.adr;
        peersGrayArr.push_back(stream.str());
    }

    nlohmann::json j;
    j["peers"] = peersArr;
    j["peers_gray"] = peersGrayArr;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::sendTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    std::vector<uint8_t> transaction;

    const std::string rawData = getStringFromJSON(body, "tx_as_hex");

    nlohmann::json j;

    if (!Common::fromHex(rawData, transaction))
    {
        j["status"] = "Failed";
        j["error"] = "Failed to parse transaction from hex buffer";
    }
    else
    {
        Crypto::Hash transactionHash = Crypto::cn_fast_hash(transaction.data(), transaction.size());

        j["transactionHash"] = Common::podToHex(transactionHash);

        std::stringstream stream;

        stream << "Attempting to add transaction " << transactionHash << " from /sendrawtransaction to pool";

        Logger::logger.log(
            stream.str(),
            Logger::DEBUG,
            { Logger::DAEMON_RPC }
        );

        const auto [success, error] = m_core->addTransactionToPool(transaction);

        if (!success)
        {
            /* Empty stream */
            std::stringstream().swap(stream);

            stream << "Failed to add transaction " << transactionHash << " from /sendrawtransaction to pool: " << error;

            Logger::logger.log(
                stream.str(),
                Logger::INFO,
                { Logger::DAEMON_RPC }
            );

            j["status"] = "Failed";
            j["error"] = error;
        }
        else
        {
            m_syncManager->relayTransactions({transaction});

            j["status"] = "OK";
            j["error"] = "";
        }
    }

    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getRandomOuts(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const uint64_t numOutputs = getUint64FromJSON(body, "outs_count");

    nlohmann::json outsArr = nlohmann::json::array();

    for (const auto &jsonAmount : getArrayFromJSON(body, "amounts"))
    {
        const uint64_t amount = jsonAmount.get<uint64_t>();

        std::vector<uint32_t> globalIndexes;
        std::vector<Crypto::PublicKey> publicKeys;

        const auto [success, error] = m_core->getRandomOutputs(
            amount, static_cast<uint16_t>(numOutputs), globalIndexes, publicKeys
        );

        if (!success)
        {
            return {Error(CANT_GET_FAKE_OUTPUTS, error), 400};
        }

        if (globalIndexes.size() != numOutputs)
        {
            std::stringstream stream;

            stream << "Failed to get enough matching outputs for amount " << amount << " ("
                   << Utilities::formatAmount(amount) << "). Requested outputs: " << numOutputs
                   << ", found outputs: " << globalIndexes.size()
                   << ". Further explanation here: https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c"
                   << std::endl
                   << "Note: If you are a public node operator, you can safely ignore this message. "
                   << "It is only relevant to the user sending the transaction.";

            return {Error(CANT_GET_FAKE_OUTPUTS, stream.str()), 400};
        }

        nlohmann::json amountOuts = nlohmann::json::array();
        for (size_t i = 0; i < globalIndexes.size(); i++)
        {
            nlohmann::json outEntry;
            outEntry["global_amount_index"] = globalIndexes[i];
            outEntry["out_key"] = Common::podToHex(publicKeys[i]);
            amountOuts.push_back(outEntry);
        }

        nlohmann::json amountObj;
        amountObj["amount"] = amount;
        amountObj["outs"] = amountOuts;
        outsArr.push_back(amountObj);
    }

    nlohmann::json j;
    j["outs"] = outsArr;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getWalletSyncData(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    std::vector<Crypto::Hash> blockHashCheckpoints;

    if (hasMember(body, "blockHashCheckpoints"))
    {
        for (const auto &jsonHash : getArrayFromJSON(body, "blockHashCheckpoints"))
        {
            std::string hashStr = jsonHash.get<std::string>();

            Crypto::Hash hash;
            if (!Common::podFromHex(hashStr, hash))
            {
                failRequest(400, "blockHashCheckpoints contains invalid hash", res);
                return {SUCCESS, 400};
            }

            blockHashCheckpoints.push_back(hash);
        }
    }

    const uint64_t startHeight = hasMember(body, "startHeight")
        ? getUint64FromJSON(body, "startHeight")
        : 0;

    const uint64_t startTimestamp = hasMember(body, "startTimestamp")
        ? getUint64FromJSON(body, "startTimestamp")
        : 0;

    const uint64_t blockCount = hasMember(body, "blockCount")
        ? getUint64FromJSON(body, "blockCount")
        : 100;

    if (blockCount > m_rpcMaxBlockCount)
    {
        failRequest(400, "blockCount exceeds rpc-max-block-count", res);
        return {SUCCESS, 400};
    }

    const bool skipCoinbaseTransactions = hasMember(body, "skipCoinbaseTransactions")
        ? getBoolFromJSON(body, "skipCoinbaseTransactions")
        : false;

    std::vector<WalletTypes::WalletBlockInfo> walletBlocks;
    std::optional<WalletTypes::TopBlock> topBlockInfo;

    const bool success = m_core->getWalletSyncData(
        blockHashCheckpoints,
        startHeight,
        startTimestamp,
        blockCount,
        skipCoinbaseTransactions,
        walletBlocks,
        topBlockInfo
    );

    if (!success)
    {
        return {SUCCESS, 500};
    }

    nlohmann::json itemsArr = nlohmann::json::array();
    for (const auto &block : walletBlocks)
    {
        nlohmann::json blockObj;

        if (block.coinbaseTransaction)
        {
            nlohmann::json cbOutputs = nlohmann::json::array();
            for (const auto &output : block.coinbaseTransaction->keyOutputs)
            {
                nlohmann::json outObj;
                outObj["key"] = Common::podToHex(output.key);
                outObj["amount"] = output.amount;
                cbOutputs.push_back(outObj);
            }
            nlohmann::json cbTx;
            cbTx["outputs"] = cbOutputs;
            cbTx["hash"] = Common::podToHex(block.coinbaseTransaction->hash);
            cbTx["txPublicKey"] = Common::podToHex(block.coinbaseTransaction->transactionPublicKey);
            cbTx["unlockTime"] = block.coinbaseTransaction->unlockTime;
            blockObj["coinbaseTX"] = cbTx;
        }

        nlohmann::json txArr = nlohmann::json::array();
        for (const auto &transaction : block.transactions)
        {
            nlohmann::json txOutputs = nlohmann::json::array();
            for (const auto &output : transaction.keyOutputs)
            {
                nlohmann::json outObj;
                outObj["key"] = Common::podToHex(output.key);
                outObj["amount"] = output.amount;
                txOutputs.push_back(outObj);
            }

            nlohmann::json txInputs = nlohmann::json::array();
            for (const auto &input : transaction.keyInputs)
            {
                nlohmann::json offsets = nlohmann::json::array();
                for (const auto &offset : input.outputIndexes)
                {
                    offsets.push_back(offset);
                }
                nlohmann::json inputObj;
                inputObj["amount"] = input.amount;
                inputObj["key_offsets"] = offsets;
                inputObj["k_image"] = Common::podToHex(input.keyImage);
                txInputs.push_back(inputObj);
            }

            nlohmann::json txObj;
            txObj["outputs"] = txOutputs;
            txObj["hash"] = Common::podToHex(transaction.hash);
            txObj["txPublicKey"] = Common::podToHex(transaction.transactionPublicKey);
            txObj["unlockTime"] = transaction.unlockTime;
            txObj["paymentID"] = transaction.paymentID;
            txObj["inputs"] = txInputs;
            txArr.push_back(txObj);
        }

        blockObj["transactions"] = txArr;
        blockObj["blockHeight"] = block.blockHeight;
        blockObj["blockHash"] = Common::podToHex(block.blockHash);
        blockObj["blockTimestamp"] = block.blockTimestamp;
        itemsArr.push_back(blockObj);
    }

    nlohmann::json j;
    j["items"] = itemsArr;

    if (topBlockInfo)
    {
        nlohmann::json topBlock;
        topBlock["hash"] = Common::podToHex(topBlockInfo->hash);
        topBlock["height"] = topBlockInfo->height;
        j["topBlock"] = topBlock;
    }

    j["synced"] = walletBlocks.empty();
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getGlobalIndexes(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const uint64_t startHeight = getUint64FromJSON(body, "startHeight");
    const uint64_t endHeight = getUint64FromJSON(body, "endHeight");

    if (endHeight < startHeight)
    {
        failRequest(400, "endHeight must be >= startHeight", res);
        return {SUCCESS, 400};
    }

    const uint64_t rangeSpan = endHeight - startHeight;
    if (rangeSpan >= m_rpcMaxGlobalIndexesRange)
    {
        failRequest(400, "Requested range exceeds rpc-max-global-index-range", res);
        return {SUCCESS, 400};
    }

    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> indexes;

    const bool success = m_core->getGlobalIndexesForRange(startHeight, endHeight, indexes);

    if (!success)
    {
        nlohmann::json j;
        j["status"] = "Failed";
        res.body = j.dump();
        return {SUCCESS, 500};
    }

    nlohmann::json indexesArr = nlohmann::json::array();
    for (const auto &[hash, globalIndexes] : indexes)
    {
        nlohmann::json valueArr = nlohmann::json::array();
        for (const auto index : globalIndexes)
        {
            valueArr.push_back(index);
        }
        nlohmann::json entry;
        entry["key"] = Common::podToHex(hash);
        entry["value"] = valueArr;
        indexesArr.push_back(entry);
    }

    nlohmann::json j;
    j["indexes"] = indexesArr;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockTemplate(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");

    const uint64_t reserveSize = getUint64FromJSON(params, "reserve_size");

    if (reserveSize > 255)
    {
        failJsonRpcRequest(
            -3,
            "Too big reserved size, maximum allowed is 255",
            res
        );

        return {SUCCESS, 200};
    }

    const std::string address = getStringFromJSON(params, "wallet_address");

    Error addressError = validateAddresses({address}, false);

    if (addressError)
    {
        failJsonRpcRequest(
            -4,
            addressError.getErrorMessage(),
            res
        );

        return {SUCCESS, 200};
    }

    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

    CryptoNote::BlockTemplate blockTemplate;

    std::vector<uint8_t> blobReserve;
    blobReserve.resize(reserveSize, 0);

    uint64_t difficulty;
    uint32_t height;

    const auto [success, error] = m_core->getBlockTemplate(
        blockTemplate, publicViewKey, publicSpendKey, blobReserve, difficulty, height
    );

    if (!success)
    {
        failJsonRpcRequest(
            -5,
            "Failed to create block template: " + error,
            res
        );

        return {SUCCESS, 200};
    }

    std::vector<uint8_t> blockBlob = CryptoNote::toBinaryArray(blockTemplate);

    const auto transactionPublicKey = Utilities::getTransactionPublicKeyFromExtra(
        blockTemplate.baseTransaction.extra
    );

    uint64_t reservedOffset = 0;

    if (reserveSize > 0)
    {
        /* Find where in the block blob the transaction public key is */
        const auto it = std::search(
            blockBlob.begin(),
            blockBlob.end(),
            std::begin(transactionPublicKey.data),
            std::end(transactionPublicKey.data)
        );

        /* The reserved offset is past the transactionPublicKey, then past
         * the extra nonce tags */
        reservedOffset = (it - blockBlob.begin()) + sizeof(transactionPublicKey) + 2;

        if (reservedOffset + reserveSize > blockBlob.size())
        {
            failJsonRpcRequest(
                -5,
                "Internal error: failed to create block template, not enough space for reserved bytes",
                res
            );

            return {SUCCESS, 200};
        }
    }

    nlohmann::json result;
    result["height"] = height;
    result["difficulty"] = difficulty;
    result["reserved_offset"] = reservedOffset;
    result["blocktemplate_blob"] = Common::toHex(blockBlob);
    result["status"] = "OK";

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::submitBlock(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getArrayFromJSON(body, "params");

    if (params.size() != 1)
    {
        failJsonRpcRequest(
            -1,
            "You must submit one and only one block blob! (Found " + std::to_string(params.size()) + ")",
            res
        );

        return {SUCCESS, 200};
    }

    const std::string blockBlob = getStringFromJSONString(params[0]);

    std::vector<uint8_t> rawBlob;

    if (!Common::fromHex(blockBlob, rawBlob))
    {
        failJsonRpcRequest(
            -6,
            "Submitted block blob is not hex!",
            res
        );

        return {SUCCESS, 200};
    }

    const auto submitResult = m_core->submitBlock(rawBlob);

    if (submitResult != CryptoNote::error::AddBlockErrorCondition::BLOCK_ADDED)
    {
        failJsonRpcRequest(
            -7,
            "Block not accepted",
            res
        );

        return {SUCCESS, 200};
    }

    if (submitResult == CryptoNote::error::AddBlockErrorCode::ADDED_TO_MAIN
        || submitResult == CryptoNote::error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED)
    {
        CryptoNote::NOTIFY_NEW_BLOCK::request newBlockMessage;

        CryptoNote::BlockTemplate blockTemplate;
        CryptoNote::fromBinaryArray(blockTemplate, rawBlob);
        newBlockMessage.block = CryptoNote::RawBlockLegacy(rawBlob, blockTemplate, m_core);
        newBlockMessage.hop = 0;
        newBlockMessage.current_blockchain_height = m_core->getTopBlockIndex() + 1;

        m_syncManager->relayBlock(newBlockMessage);
    }

    nlohmann::json result;
    result["status"] = "OK";

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockCount(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    nlohmann::json result;
    result["status"] = "OK";
    result["count"] = m_core->getTopBlockIndex() + 1;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getLastBlockHeader(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    try
    {
        const auto height = m_core->getTopBlockIndex();
        const auto hash = m_core->getBlockHashByIndex(height);

        if (hash == Constants::NULL_HASH)
        {
            throw std::runtime_error("Top block hash is null during chain reorganization");
        }

        const auto topBlock = m_core->getBlockByHash(hash);
        const auto outputs = topBlock.baseTransaction.outputs;
        const auto extraDetails = m_core->getBlockDetails(hash);

        const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
            [](const auto acc, const auto out) {
                return acc + out.amount;
            }
        );

        nlohmann::json blockHeader;
        blockHeader["major_version"] = topBlock.majorVersion;
        blockHeader["minor_version"] = topBlock.minorVersion;
        blockHeader["timestamp"] = topBlock.timestamp;
        blockHeader["prev_hash"] = Common::podToHex(topBlock.previousBlockHash);
        blockHeader["nonce"] = topBlock.nonce;
        blockHeader["orphan_status"] = extraDetails.isAlternative;
        blockHeader["height"] = height;
        blockHeader["depth"] = uint64_t(0);
        blockHeader["hash"] = Common::podToHex(hash);
        blockHeader["difficulty"] = m_core->getBlockDifficulty(height);
        blockHeader["reward"] = reward;
        blockHeader["num_txes"] = extraDetails.transactions.size();
        blockHeader["block_size"] = extraDetails.blockSize;

        nlohmann::json result;
        result["status"] = "OK";
        result["block_header"] = blockHeader;

        nlohmann::json j;
        j["jsonrpc"] = "2.0";
        j["result"] = result;
        res.body = j.dump();

        return {SUCCESS, 200};
    }
    catch (const std::exception &e)
    {
        nlohmann::json j;
        j["jsonrpc"] = "2.0";
        j["error"] = {{"code", -9}, {"message", "Chain is reorganizing, please retry shortly"}};
        res.body = j.dump();
        return {SUCCESS, 503};
    }
}

std::tuple<Error, uint16_t> RpcServer::getBlockHeaderByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");
    const auto hashStr = getStringFromJSON(params, "hash");
    const auto topHeight = m_core->getTopBlockIndex();

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        failJsonRpcRequest(
            -1,
            "Block hash specified is not a valid hex!",
            res
        );

        return {SUCCESS, 200};
    }

    CryptoNote::BlockTemplate block;

    try
    {
        block = m_core->getBlockByHash(hash);
    }
    catch (const std::runtime_error &)
    {
        failJsonRpcRequest(
            -5,
            "Block hash specified does not exist!",
            res
        );

        return {SUCCESS, 200};
    }

    CryptoNote::CachedBlock cachedBlock(block);

    const auto height = cachedBlock.getBlockIndex();
    const auto outputs = block.baseTransaction.outputs;
    const auto extraDetails = m_core->getBlockDetails(hash);

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    nlohmann::json blockHeader;
    blockHeader["major_version"] = block.majorVersion;
    blockHeader["minor_version"] = block.minorVersion;
    blockHeader["timestamp"] = block.timestamp;
    blockHeader["prev_hash"] = Common::podToHex(block.previousBlockHash);
    blockHeader["nonce"] = block.nonce;
    blockHeader["orphan_status"] = extraDetails.isAlternative;
    blockHeader["height"] = height;
    blockHeader["depth"] = topHeight - height;
    blockHeader["hash"] = Common::podToHex(hash);
    blockHeader["difficulty"] = m_core->getBlockDifficulty(height);
    blockHeader["reward"] = reward;
    blockHeader["num_txes"] = extraDetails.transactions.size();
    blockHeader["block_size"] = extraDetails.blockSize;

    nlohmann::json result;
    result["status"] = "OK";
    result["block_header"] = blockHeader;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockHeaderByHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");
    const auto height = getUint64FromJSON(params, "height");
    const auto topHeight = m_core->getTopBlockIndex();

    if (height > topHeight)
    {
        failJsonRpcRequest(
            -2,
            "Requested block header for a height that is higher than the current "
            "blockchain height! Current height: " + std::to_string(topHeight),
            res
        );

        return {SUCCESS, 200};
    }

    const auto hash = m_core->getBlockHashByIndex(height);
    const auto block = m_core->getBlockByHash(hash);

    const auto outputs = block.baseTransaction.outputs;
    const auto extraDetails = m_core->getBlockDetails(hash);

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    nlohmann::json blockHeader;
    blockHeader["major_version"] = block.majorVersion;
    blockHeader["minor_version"] = block.minorVersion;
    blockHeader["timestamp"] = block.timestamp;
    blockHeader["prev_hash"] = Common::podToHex(block.previousBlockHash);
    blockHeader["nonce"] = block.nonce;
    blockHeader["orphan_status"] = extraDetails.isAlternative;
    blockHeader["height"] = height;
    blockHeader["depth"] = topHeight - height;
    blockHeader["hash"] = Common::podToHex(hash);
    blockHeader["difficulty"] = m_core->getBlockDifficulty(height);
    blockHeader["reward"] = reward;
    blockHeader["num_txes"] = extraDetails.transactions.size();
    blockHeader["block_size"] = extraDetails.blockSize;

    nlohmann::json result;
    result["status"] = "OK";
    result["block_header"] = blockHeader;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlocksByHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");
    const auto height = getUint64FromJSON(params, "height");
    const auto topHeight = m_core->getTopBlockIndex();

    if (height > topHeight)
    {
        failJsonRpcRequest(
            -2,
            "Requested block header for a height that is higher than the current "
            "blockchain height! Current height: " + std::to_string(topHeight),
            res
        );

        return {SUCCESS, 200};
    }

    const uint64_t MAX_BLOCKS_COUNT = 30;
    const uint64_t startHeight = height < MAX_BLOCKS_COUNT ? 0 : height - MAX_BLOCKS_COUNT;

    nlohmann::json blocksArr = nlohmann::json::array();
    for (uint64_t i = height; i >= startHeight; i--)
    {
        const auto hash = m_core->getBlockHashByIndex(i);
        const auto block = m_core->getBlockByHash(hash);
        const auto extraDetails = m_core->getBlockDetails(hash);

        nlohmann::json blockObj;
        blockObj["cumul_size"] = extraDetails.blockSize;
        blockObj["difficulty"] = extraDetails.difficulty;
        blockObj["hash"] = Common::podToHex(hash);
        blockObj["height"] = i;
        blockObj["timestamp"] = block.timestamp;
        /* Plus one for coinbase tx */
        blockObj["tx_count"] = block.transactionHashes.size() + 1;
        blocksArr.push_back(blockObj);
    }

    nlohmann::json result;
    result["status"] = "OK";
    result["blocks"] = blocksArr;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockDetailsByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");
    const auto hashStr = getStringFromJSON(params, "hash");
    const auto topHeight = m_core->getTopBlockIndex();

    Crypto::Hash hash;

    if (hashStr.length() == 64)
    {
        if (!Common::podFromHex(hashStr, hash))
        {
            failJsonRpcRequest(
                -1,
                "Block hash specified is not a valid hex!",
                res
            );

            return {SUCCESS, 200};
        }
    }
    else
    {
        /* Hash parameter can be both a hash string, and a number... because cryptonote.. */
        try
        {
            uint64_t height = std::stoull(hashStr);

            hash = m_core->getBlockHashByIndex(height - 1);

            if (hash == Constants::NULL_HASH)
            {
                failJsonRpcRequest(
                    -2,
                    "Requested hash for a height that is higher than the current "
                    "blockchain height! Current height: " + std::to_string(topHeight),
                    res
                );

                return {SUCCESS, 200};
            }
        }
        catch (const std::out_of_range &)
        {
            failJsonRpcRequest(
                -1,
                "Block hash specified is not valid!",
                res
            );

            return {SUCCESS, 200};
        }
        catch (const std::invalid_argument &)
        {
            failJsonRpcRequest(
                -1,
                "Block hash specified is not valid!",
                res
            );

            return {SUCCESS, 200};
        }
    }

    const auto block = m_core->getBlockByHash(hash);
    const auto extraDetails = m_core->getBlockDetails(hash);
    const auto height = CryptoNote::CachedBlock(block).getBlockIndex();
    const auto outputs = block.baseTransaction.outputs;

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    const uint64_t blockSizeMedian = std::max(
        extraDetails.sizeMedian,
        static_cast<uint64_t>(
            m_core->getCurrency().blockGrantedFullRewardZoneByBlockVersion(block.majorVersion)
        )
    );

    nlohmann::json txArr = nlohmann::json::array();
    for (const auto &transaction : extraDetails.transactions)
    {
        nlohmann::json txObj;
        txObj["hash"] = Common::podToHex(transaction.hash);
        txObj["fee"] = transaction.fee;
        txObj["amount_out"] = transaction.totalOutputsAmount;
        txObj["size"] = transaction.size;
        txArr.push_back(txObj);
    }

    nlohmann::json blockObj;
    blockObj["major_version"] = block.majorVersion;
    blockObj["minor_version"] = block.minorVersion;
    blockObj["timestamp"] = block.timestamp;
    blockObj["prev_hash"] = Common::podToHex(block.previousBlockHash);
    blockObj["nonce"] = block.nonce;
    blockObj["orphan_status"] = extraDetails.isAlternative;
    blockObj["height"] = height;
    blockObj["depth"] = topHeight - height;
    blockObj["hash"] = Common::podToHex(hash);
    blockObj["difficulty"] = m_core->getBlockDifficulty(height);
    blockObj["reward"] = reward;
    blockObj["blockSize"] = extraDetails.blockSize;
    blockObj["transactionsCumulativeSize"] = extraDetails.transactionsCumulativeSize;
    blockObj["alreadyGeneratedCoins"] = std::to_string(extraDetails.alreadyGeneratedCoins);
    blockObj["alreadyGeneratedTransactions"] = extraDetails.alreadyGeneratedTransactions;
    blockObj["sizeMedian"] = extraDetails.sizeMedian;
    blockObj["baseReward"] = extraDetails.baseReward;
    blockObj["penalty"] = extraDetails.penalty;
    blockObj["effectiveSizeMedian"] = blockSizeMedian;
    blockObj["transactions"] = txArr;
    blockObj["totalFeeAmount"] = extraDetails.totalFeeAmount;

    nlohmann::json result;
    result["status"] = "OK";
    result["block"] = blockObj;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getTransactionDetailsByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");
    const auto hashStr = getStringFromJSON(params, "hash");

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        failJsonRpcRequest(
            -1,
            "Block hash specified is not a valid hex!",
            res
        );

        return {SUCCESS, 200};
    }

    std::vector<Crypto::Hash> ignore;
    std::vector<std::vector<uint8_t>> rawTXs;
    std::vector<Crypto::Hash> hashes { hash };

    m_core->getTransactions(hashes, rawTXs, ignore);

    if (rawTXs.size() != 1)
    {
        failJsonRpcRequest(
            -1,
            "Block hash specified does not exist!",
            res
        );

        return {SUCCESS, 200};
    }

    CryptoNote::Transaction transaction;
    CryptoNote::TransactionDetails txDetails = m_core->getTransactionDetails(hash);

    const uint64_t blockHeight = txDetails.blockIndex;
    const auto blockHash = m_core->getBlockHashByIndex(blockHeight);
    const auto block = m_core->getBlockByHash(blockHash);
    const auto extraDetails = m_core->getBlockDetails(blockHash);

    fromBinaryArray(transaction, rawTXs[0]);

    nlohmann::json blockObj;
    blockObj["cumul_size"] = extraDetails.blockSize;
    blockObj["difficulty"] = extraDetails.difficulty;
    blockObj["hash"] = Common::podToHex(blockHash);
    blockObj["height"] = blockHeight;
    blockObj["timestamp"] = block.timestamp;
    /* Plus one for coinbase tx */
    blockObj["tx_count"] = block.transactionHashes.size() + 1;

    nlohmann::json vinArr = nlohmann::json::array();
    for (const auto &input : transaction.inputs)
    {
        const auto type = input.type() == typeid(CryptoNote::BaseInput) ? "ff" : "02";

        nlohmann::json valueObj;
        if (input.type() == typeid(CryptoNote::BaseInput))
        {
            valueObj["height"] = boost::get<CryptoNote::BaseInput>(input).blockIndex;
        }
        else
        {
            const auto keyInput = boost::get<CryptoNote::KeyInput>(input);
            nlohmann::json offsets = nlohmann::json::array();
            for (const auto index : keyInput.outputIndexes)
            {
                offsets.push_back(index);
            }
            valueObj["k_image"] = Common::podToHex(keyInput.keyImage);
            valueObj["amount"] = keyInput.amount;
            valueObj["key_offsets"] = offsets;
        }

        nlohmann::json inputObj;
        inputObj["type"] = type;
        inputObj["value"] = valueObj;
        vinArr.push_back(inputObj);
    }

    nlohmann::json voutArr = nlohmann::json::array();
    for (const auto &output : transaction.outputs)
    {
        nlohmann::json dataObj;
        dataObj["key"] = Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key);

        nlohmann::json targetObj;
        targetObj["data"] = dataObj;
        targetObj["type"] = "02";

        nlohmann::json outObj;
        outObj["amount"] = output.amount;
        outObj["target"] = targetObj;
        voutArr.push_back(outObj);
    }

    nlohmann::json txObj;
    txObj["extra"] = Common::podToHex(transaction.extra);
    txObj["publicKey"] = Common::podToHex(txDetails.extra.publicKey);
    txObj["nonce"] = Common::toHex(txDetails.extra.nonce);
    txObj["unlock_time"] = transaction.unlockTime;
    txObj["version"] = transaction.version;
    txObj["vin"] = vinArr;
    txObj["vout"] = voutArr;

    nlohmann::json txDetailsObj;
    txDetailsObj["hash"] = Common::podToHex(txDetails.hash);
    txDetailsObj["amount_out"] = txDetails.totalOutputsAmount;
    txDetailsObj["fee"] = txDetails.fee;
    txDetailsObj["mixin"] = txDetails.mixin;
    txDetailsObj["paymentId"] = Utilities::getPaymentIDFromExtra(transaction.extra);
    txDetailsObj["size"] = txDetails.size;

    nlohmann::json result;
    result["status"] = "OK";
    result["block"] = blockObj;
    result["tx"] = txObj;
    result["txDetails"] = txDetailsObj;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getTransactionsInPool(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    nlohmann::json txArr = nlohmann::json::array();
    for (const auto &tx : m_core->getPoolTransactions())
    {
        const uint64_t outputAmount = std::accumulate(tx.outputs.begin(), tx.outputs.end(), 0ull,
            [](const auto acc, const auto out) {
                return acc + out.amount;
            }
        );

        const uint64_t inputAmount = std::accumulate(tx.inputs.begin(), tx.inputs.end(), 0ull,
            [](const auto acc, const auto in) {
                if (in.type() == typeid(CryptoNote::KeyInput))
                {
                    return acc + boost::get<CryptoNote::KeyInput>(in).amount;
                }

                return acc;
            }
        );

        const uint64_t fee = inputAmount - outputAmount;

        nlohmann::json txObj;
        txObj["hash"] = Common::podToHex(getObjectHash(tx));
        txObj["fee"] = fee;
        txObj["amount_out"] = outputAmount;
        txObj["size"] = getObjectBinarySize(tx);
        txArr.push_back(txObj);
    }

    nlohmann::json result;
    result["status"] = "OK";
    result["transactions"] = txArr;

    nlohmann::json j;
    j["jsonrpc"] = "2.0";
    j["result"] = result;
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::queryBlocksLite(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    uint64_t timestamp = 0;

    if (hasMember(body, "timestamp"))
    {
        timestamp = getUint64FromJSON(body, "timestamp");
    }

    std::vector<Crypto::Hash> knownBlockHashes;

    if (hasMember(body, "blockIds"))
    {
        for (const auto &hashStrJson : getArrayFromJSON(body, "blockIds"))
        {
            Crypto::Hash hash;

            if (!Common::podFromHex(getStringFromJSONString(hashStrJson), hash))
            {
                failRequest(400, "Block hash specified is not a valid hex string!", res);
                return {SUCCESS, 400};
            }

            knownBlockHashes.push_back(hash);
        }
    }

    uint32_t startHeight;
    uint32_t currentHeight;
    uint32_t fullOffset;

    std::vector<CryptoNote::BlockShortInfo> blocks;

    if (!m_core->queryBlocksLite(knownBlockHashes, timestamp, startHeight, currentHeight, fullOffset, blocks))
    {
        failRequest(500, "Internal error: failed to queryblockslite", res);
        return {SUCCESS, 500};
    }

    nlohmann::json itemsArr = nlohmann::json::array();
    for (const auto &block : blocks)
    {
        nlohmann::json blockBytes = nlohmann::json::array();
        for (const auto c : block.block)
        {
            blockBytes.push_back(c);
        }

        nlohmann::json prefixesArr = nlohmann::json::array();
        for (const auto &prefix : block.txPrefixes)
        {
            nlohmann::json vinArr = nlohmann::json::array();
            for (const auto &input : prefix.txPrefix.inputs)
            {
                const auto type = input.type() == typeid(CryptoNote::BaseInput) ? "ff" : "02";

                nlohmann::json valueObj;
                if (input.type() == typeid(CryptoNote::BaseInput))
                {
                    valueObj["height"] = boost::get<CryptoNote::BaseInput>(input).blockIndex;
                }
                else
                {
                    const auto keyInput = boost::get<CryptoNote::KeyInput>(input);
                    nlohmann::json offsets = nlohmann::json::array();
                    for (const auto index : keyInput.outputIndexes)
                    {
                        offsets.push_back(index);
                    }
                    valueObj["k_image"] = Common::podToHex(keyInput.keyImage);
                    valueObj["amount"] = keyInput.amount;
                    valueObj["key_offsets"] = offsets;
                }

                nlohmann::json inputObj;
                inputObj["type"] = type;
                inputObj["value"] = valueObj;
                vinArr.push_back(inputObj);
            }

            nlohmann::json voutArr = nlohmann::json::array();
            for (const auto &output : prefix.txPrefix.outputs)
            {
                nlohmann::json dataObj;
                dataObj["key"] = Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key);

                nlohmann::json targetObj;
                targetObj["data"] = dataObj;
                targetObj["type"] = "02";

                nlohmann::json outObj;
                outObj["amount"] = output.amount;
                outObj["target"] = targetObj;
                voutArr.push_back(outObj);
            }

            nlohmann::json txPrefixObj;
            txPrefixObj["extra"] = Common::toHex(prefix.txPrefix.extra);
            txPrefixObj["unlock_time"] = prefix.txPrefix.unlockTime;
            txPrefixObj["version"] = prefix.txPrefix.version;
            txPrefixObj["vin"] = vinArr;
            txPrefixObj["vout"] = voutArr;

            nlohmann::json prefixEntry;
            prefixEntry["transactionPrefixInfo.txHash"] = Common::podToHex(prefix.txHash);
            prefixEntry["transactionPrefixInfo.txPrefix"] = txPrefixObj;
            prefixesArr.push_back(prefixEntry);
        }

        nlohmann::json blockEntry;
        blockEntry["blockShortInfo.block"] = blockBytes;
        blockEntry["blockShortInfo.blockId"] = Common::podToHex(block.blockId);
        blockEntry["blockShortInfo.txPrefixes"] = prefixesArr;
        itemsArr.push_back(blockEntry);
    }

    nlohmann::json j;
    j["fullOffset"] = fullOffset;
    j["currentHeight"] = currentHeight;
    j["startHeight"] = startHeight;
    j["items"] = itemsArr;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getTransactionsStatus(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    std::unordered_set<Crypto::Hash> transactionHashes;

    for (const auto &hashStr : getArrayFromJSON(body, "transactionHashes"))
    {
        Crypto::Hash hash;

        if (!Common::podFromHex(getStringFromJSONString(hashStr), hash))
        {
            failRequest(400, "Transaction hash specified is not a valid hex string!", res);
            return {SUCCESS, 400};
        }

        transactionHashes.insert(hash);
    }

    std::unordered_set<Crypto::Hash> transactionsInPool;
    std::unordered_set<Crypto::Hash> transactionsInBlock;
    std::unordered_set<Crypto::Hash> transactionsUnknown;

    const bool success = m_core->getTransactionsStatus(
        transactionHashes, transactionsInPool, transactionsInBlock, transactionsUnknown
    );

    if (!success)
    {
        failRequest(500, "Internal error: failed to getTransactionsStatus", res);
        return {SUCCESS, 500};
    }

    nlohmann::json inBlockArr = nlohmann::json::array();
    for (const auto &hash : transactionsInBlock)
    {
        inBlockArr.push_back(Common::podToHex(hash));
    }

    nlohmann::json inPoolArr = nlohmann::json::array();
    for (const auto &hash : transactionsInPool)
    {
        inPoolArr.push_back(Common::podToHex(hash));
    }

    nlohmann::json unknownArr = nlohmann::json::array();
    for (const auto &hash : transactionsUnknown)
    {
        unknownArr.push_back(Common::podToHex(hash));
    }

    nlohmann::json j;
    j["transactionsInBlock"] = inBlockArr;
    j["transactionsInPool"] = inPoolArr;
    j["transactionsUnknown"] = unknownArr;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getPoolChanges(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    Crypto::Hash lastBlockHash;

    if (!Common::podFromHex(getStringFromJSON(body, "tailBlockId"), lastBlockHash))
    {
        failRequest(400, "tailBlockId specified is not a valid hex string!", res);
        return {SUCCESS, 400};
    }

    std::vector<Crypto::Hash> knownHashes;

    for (const auto &hashStr : getArrayFromJSON(body, "knownTxsIds"))
    {
        Crypto::Hash hash;

        if (!Common::podFromHex(getStringFromJSONString(hashStr), hash))
        {
            failRequest(400, "Transaction hash specified is not a valid hex string!", res);
            return {SUCCESS, 400};
        }

        knownHashes.push_back(hash);
    }

    std::vector<CryptoNote::TransactionPrefixInfo> addedTransactions;
    std::vector<Crypto::Hash> deletedTransactions;

    const bool atTopOfChain = m_core->getPoolChangesLite(
        lastBlockHash, knownHashes, addedTransactions, deletedTransactions
    );

    nlohmann::json addedTxsArr = nlohmann::json::array();
    for (const auto &prefix : addedTransactions)
    {
        nlohmann::json vinArr = nlohmann::json::array();
        for (const auto &input : prefix.txPrefix.inputs)
        {
            const auto type = input.type() == typeid(CryptoNote::BaseInput) ? "ff" : "02";

            nlohmann::json valueObj;
            if (input.type() == typeid(CryptoNote::BaseInput))
            {
                valueObj["height"] = boost::get<CryptoNote::BaseInput>(input).blockIndex;
            }
            else
            {
                const auto keyInput = boost::get<CryptoNote::KeyInput>(input);
                nlohmann::json offsets = nlohmann::json::array();
                for (const auto &index : keyInput.outputIndexes)
                {
                    offsets.push_back(index);
                }
                valueObj["k_image"] = Common::podToHex(keyInput.keyImage);
                valueObj["amount"] = keyInput.amount;
                valueObj["key_offsets"] = offsets;
            }

            nlohmann::json inputObj;
            inputObj["type"] = type;
            inputObj["value"] = valueObj;
            vinArr.push_back(inputObj);
        }

        nlohmann::json voutArr = nlohmann::json::array();
        for (const auto &output : prefix.txPrefix.outputs)
        {
            nlohmann::json dataObj;
            dataObj["key"] = Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key);

            nlohmann::json targetObj;
            targetObj["data"] = dataObj;
            targetObj["type"] = "02";

            nlohmann::json outObj;
            outObj["amount"] = output.amount;
            outObj["target"] = targetObj;
            voutArr.push_back(outObj);
        }

        nlohmann::json txPrefixObj;
        txPrefixObj["extra"] = Common::toHex(prefix.txPrefix.extra);
        txPrefixObj["unlock_time"] = prefix.txPrefix.unlockTime;
        txPrefixObj["version"] = prefix.txPrefix.version;
        txPrefixObj["vin"] = vinArr;
        txPrefixObj["vout"] = voutArr;

        nlohmann::json prefixEntry;
        prefixEntry["transactionPrefixInfo.txHash"] = Common::podToHex(prefix.txHash);
        prefixEntry["transactionPrefixInfo.txPrefix"] = txPrefixObj;
        addedTxsArr.push_back(prefixEntry);
    }

    nlohmann::json deletedArr = nlohmann::json::array();
    for (const auto &hash : deletedTransactions)
    {
        deletedArr.push_back(Common::podToHex(hash));
    }

    nlohmann::json j;
    j["addedTxs"] = addedTxsArr;
    j["deletedTxsIds"] = deletedArr;
    j["isTailBlockActual"] = atTopOfChain;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::queryBlocksDetailed(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    uint64_t timestamp = 0;

    if (hasMember(body, "timestamp"))
    {
        timestamp = getUint64FromJSON(body, "timestamp");
    }

    std::vector<Crypto::Hash> knownBlockHashes;

    if (hasMember(body, "blockIds"))
    {
        for (const auto &hashStrJson : getArrayFromJSON(body, "blockIds"))
        {
            Crypto::Hash hash;

            if (!Common::podFromHex(getStringFromJSONString(hashStrJson), hash))
            {
                failRequest(400, "Block hash specified is not a valid hex string!", res);
                return {SUCCESS, 400};
            }

            knownBlockHashes.push_back(hash);
        }
    }

    uint64_t startHeight;
    uint64_t currentHeight;
    uint64_t fullOffset;

    uint64_t blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;

    if (hasMember(body, "blockCount"))
    {
        blockCount = getUint64FromJSON(body, "blockCount");
    }

    std::vector<CryptoNote::BlockDetails> blocks;

    if (!m_core->queryBlocksDetailed(knownBlockHashes, timestamp, startHeight, currentHeight, fullOffset, blocks, blockCount))
    {
        failRequest(500, "Internal error: failed to queryblockslite", res);
        return {SUCCESS, 500};
    }

    nlohmann::json blocksArr = nlohmann::json::array();
    for (const auto &block : blocks)
    {
        nlohmann::json txsArr = nlohmann::json::array();
        for (const auto &tx : block.transactions)
        {
            nlohmann::json nonceArr = nlohmann::json::array();
            for (const auto &c : tx.extra.nonce)
            {
                nonceArr.push_back(c);
            }

            nlohmann::json extraObj;
            extraObj["nonce"] = nonceArr;
            extraObj["publicKey"] = Common::podToHex(tx.extra.publicKey);
            extraObj["raw"] = Common::toHex(tx.extra.raw);

            nlohmann::json inputsArr = nlohmann::json::array();
            for (const auto &input : tx.inputs)
            {
                const auto type = input.type() == typeid(CryptoNote::BaseInputDetails) ? "ff" : "02";

                nlohmann::json dataObj;
                if (input.type() == typeid(CryptoNote::BaseInputDetails))
                {
                    const auto in = boost::get<CryptoNote::BaseInputDetails>(input);
                    nlohmann::json inputSubObj;
                    inputSubObj["height"] = in.input.blockIndex;
                    dataObj["amount"] = in.amount;
                    dataObj["input"] = inputSubObj;
                }
                else
                {
                    const auto in = boost::get<CryptoNote::KeyInputDetails>(input);
                    nlohmann::json offsets = nlohmann::json::array();
                    for (const auto &index : in.input.outputIndexes)
                    {
                        offsets.push_back(index);
                    }
                    nlohmann::json inputSubObj;
                    inputSubObj["amount"] = in.input.amount;
                    inputSubObj["k_image"] = Common::podToHex(in.input.keyImage);
                    inputSubObj["key_offsets"] = offsets;

                    nlohmann::json outputRef;
                    outputRef["transactionHash"] = Common::podToHex(in.output.transactionHash);
                    outputRef["number"] = in.output.number;

                    dataObj["input"] = inputSubObj;
                    dataObj["mixin"] = in.mixin;
                    dataObj["output"] = outputRef;
                }

                nlohmann::json inputEntry;
                inputEntry["type"] = type;
                inputEntry["data"] = dataObj;
                inputsArr.push_back(inputEntry);
            }

            nlohmann::json outputsArr = nlohmann::json::array();
            for (const auto &output : tx.outputs)
            {
                nlohmann::json dataObj;
                dataObj["key"] = Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.output.target).key);

                nlohmann::json targetObj;
                targetObj["data"] = dataObj;
                targetObj["type"] = "02";

                nlohmann::json outputInner;
                outputInner["amount"] = output.output.amount;
                outputInner["target"] = targetObj;

                nlohmann::json outEntry;
                outEntry["globalIndex"] = output.globalIndex;
                outEntry["output"] = outputInner;
                outputsArr.push_back(outEntry);
            }

            nlohmann::json sigsArr = nlohmann::json::array();
            {
                int i = 0;
                for (const auto &sigs : tx.signatures)
                {
                    for (const auto &sig : sigs)
                    {
                        nlohmann::json sigObj;
                        sigObj["first"] = i;
                        sigObj["second"] = Common::podToHex(sig);
                        sigsArr.push_back(sigObj);
                    }
                    i++;
                }
            }

            nlohmann::json txObj;
            txObj["blockHash"] = Common::podToHex(block.hash);
            txObj["blockIndex"] = block.index;
            txObj["extra"] = extraObj;
            txObj["fee"] = tx.fee;
            txObj["hash"] = Common::podToHex(tx.hash);
            txObj["inBlockchain"] = tx.inBlockchain;
            txObj["inputs"] = inputsArr;
            txObj["mixin"] = tx.mixin;
            txObj["outputs"] = outputsArr;
            txObj["paymentId"] = Common::podToHex(tx.paymentId);
            txObj["signatures"] = sigsArr;
            txObj["signaturesSize"] = tx.signatures.size();
            txObj["size"] = tx.size;
            txObj["timestamp"] = tx.timestamp;
            txObj["totalInputsAmount"] = tx.totalInputsAmount;
            txObj["totalOutputsAmount"] = tx.totalOutputsAmount;
            txObj["unlockTime"] = tx.unlockTime;
            txsArr.push_back(txObj);
        }

        nlohmann::json blockObj;
        blockObj["major_version"] = block.majorVersion;
        blockObj["minor_version"] = block.minorVersion;
        blockObj["timestamp"] = block.timestamp;
        blockObj["prevBlockHash"] = Common::podToHex(block.prevBlockHash);
        blockObj["index"] = block.index;
        blockObj["hash"] = Common::podToHex(block.hash);
        blockObj["difficulty"] = block.difficulty;
        blockObj["reward"] = block.reward;
        blockObj["blockSize"] = block.blockSize;
        blockObj["alreadyGeneratedCoins"] = std::to_string(block.alreadyGeneratedCoins);
        blockObj["alreadyGeneratedTransactions"] = block.alreadyGeneratedTransactions;
        blockObj["sizeMedian"] = block.sizeMedian;
        blockObj["baseReward"] = block.baseReward;
        blockObj["nonce"] = block.nonce;
        blockObj["totalFeeAmount"] = block.totalFeeAmount;
        blockObj["transactionsCumulativeSize"] = block.transactionsCumulativeSize;
        blockObj["transactions"] = txsArr;
        blocksArr.push_back(blockObj);
    }

    nlohmann::json j;
    j["fullOffset"] = fullOffset;
    j["currentHeight"] = currentHeight;
    j["startHeight"] = startHeight;
    j["blocks"] = blocksArr;
    j["status"] = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

/* Deprecated. Use getGlobalIndexes instead. */
std::tuple<Error, uint16_t> RpcServer::getGlobalIndexesDeprecated(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    Crypto::Hash hash;

    if (!Common::podFromHex(getStringFromJSON(body, "txid"), hash))
    {
        failRequest(400, "txid specified is not a valid hex string!", res);
        return {SUCCESS, 400};
    }

    std::vector<uint32_t> indexes;

    const bool success = m_core->getTransactionGlobalIndexes(hash, indexes);

    if (!success)
    {
        failRequest(500, "Internal error: Failed to getTransactionGlobalIndexes", res);
        return {SUCCESS, 500};
    }

    nlohmann::json indexesArr = nlohmann::json::array();
    for (const auto &index : indexes)
        indexesArr.push_back(index);

    nlohmann::json j;
    j["o_indexes"] = indexesArr;
    j["status"]    = "OK";
    res.body = j.dump();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getRawBlocks(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    std::vector<Crypto::Hash> blockHashCheckpoints;

    if (hasMember(body, "blockHashCheckpoints"))
    {
        for (const auto &jsonHash : getArrayFromJSON(body, "blockHashCheckpoints"))
        {
            const std::string hashStr = getStringFromJSONString(jsonHash);

            Crypto::Hash hash;
            if (!Common::podFromHex(hashStr, hash))
            {
                failRequest(400, "blockHashCheckpoints contains invalid hash", res);
                return {SUCCESS, 400};
            }

            blockHashCheckpoints.push_back(hash);
        }
    }

    const uint64_t startHeight = hasMember(body, "startHeight")
        ? getUint64FromJSON(body, "startHeight")
        : 0;

    const uint64_t startTimestamp = hasMember(body, "startTimestamp")
        ? getUint64FromJSON(body, "startTimestamp")
        : 0;

    const uint64_t blockCount = hasMember(body, "blockCount")
        ? getUint64FromJSON(body, "blockCount")
        : 100;

    if (blockCount > m_rpcMaxBlockCount)
    {
        failRequest(400, "blockCount exceeds rpc-max-block-count", res);
        return {SUCCESS, 400};
    }

    const bool skipCoinbaseTransactions = hasMember(body, "skipCoinbaseTransactions")
        ? getBoolFromJSON(body, "skipCoinbaseTransactions")
        : false;

    std::vector<CryptoNote::RawBlock> blocks;
    std::optional<WalletTypes::TopBlock> topBlockInfo;

    const bool success = m_core->getRawBlocks(
        blockHashCheckpoints,
        startHeight,
        startTimestamp,
        blockCount,
        skipCoinbaseTransactions,
        blocks,
        topBlockInfo
    );

    if (!success)
    {
        return {SUCCESS, 500};
    }

    nlohmann::json itemsArr = nlohmann::json::array();
    for (const auto &block : blocks)
    {
        nlohmann::json txArr = nlohmann::json::array();
        for (const auto &transaction : block.transactions)
            txArr.push_back(Common::toHex(transaction));

        itemsArr.push_back({
            {"block",        Common::toHex(block.block)},
            {"transactions", txArr}
        });
    }

    nlohmann::json j;
    j["items"]  = itemsArr;
    j["synced"] = blocks.empty();
    j["status"] = "OK";

    if (topBlockInfo)
    {
        j["topBlock"] = {
            {"hash",   Common::podToHex(topBlockInfo->hash)},
            {"height", topBlockInfo->height}
        };
    }

    res.body = j.dump();

    return {SUCCESS, 200};
}

