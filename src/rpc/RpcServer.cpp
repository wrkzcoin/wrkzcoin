// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <rpc/RpcServer.h>

#include "httplib.h"
//////////////////////////

#include <algorithm>
#include <iostream>
#include <ctime>
#include <thread>

#include "version.h"

#include <config/Constants.h>
#include <config/WalletConfig.h>
#include <common/CryptoNoteTools.h>
#include <common/IpcSocket.h>
#include <errors/ValidateParameters.h>
#include <logger/Logger.h>
#include <serialization/SerializationTools.h>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/ParseExtra.h>
#include <variant>

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

const char *RpcServer::compressionAlgorithm()
{
#if defined(CPPHTTPLIB_BROTLI_SUPPORT)
    return "br";
#elif defined(CPPHTTPLIB_ZLIB_SUPPORT)
    return "gzip";
#else
    return "none";
#endif
}

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
    const uint64_t rpcSyncCacheBytes,
    const bool rpcTrustProxy,
    const std::string rpcIpcPath,
    const uint32_t rpcIpcMode,
    const std::string rpcIpcGroup,
    const bool rpcIpcRequireToken,
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
    m_ipcPath(rpcIpcPath),
    m_ipcMode(rpcIpcMode),
    m_ipcGroup(rpcIpcGroup),
    m_ipcRequireToken(rpcIpcRequireToken),
    m_rpcMode(rpcMode),
    m_core(core),
    m_p2p(p2p),
    m_syncManager(syncManager),
    m_syncCacheMaxBytes(rpcSyncCacheBytes)
{
    m_server = std::make_unique<httplib::Server>();
    m_ipv6Server = std::make_unique<httplib::Server>();

    applyServerTuning(*m_server);
    m_server->set_address_family(AF_INET);
    m_server->set_read_timeout(std::chrono::seconds(m_rpcReadTimeout));
    m_server->set_write_timeout(std::chrono::seconds(m_rpcWriteTimeout));
    m_server->set_payload_max_length(static_cast<size_t>(m_rpcMaxRequestBodyBytes));
    m_server->set_error_logger([](const httplib::Error &error, const httplib::Request *) {
        Logger::logger.log(
            "RPC server startup error: " + httplib::to_string(error),
            Logger::WARNING,
            { Logger::DAEMON_RPC }
        );
    });

    setupRoutes(*m_server, false);

    if (!m_ipv6Host.empty())
    {
        applyServerTuning(*m_ipv6Server);
        m_ipv6Server->set_address_family(AF_INET6);
        m_ipv6Server->set_ipv6_v6only(true);
        m_ipv6Server->set_read_timeout(std::chrono::seconds(m_rpcReadTimeout));
        m_ipv6Server->set_write_timeout(std::chrono::seconds(m_rpcWriteTimeout));
        m_ipv6Server->set_payload_max_length(static_cast<size_t>(m_rpcMaxRequestBodyBytes));
        m_ipv6Server->set_error_logger([](const httplib::Error &error, const httplib::Request *) {
            Logger::logger.log(
                "RPC IPv6 server startup error: " + httplib::to_string(error),
                Logger::WARNING,
                { Logger::DAEMON_RPC }
            );
        });
        setupRoutes(*m_ipv6Server, false);
    }

    if (!m_ipcPath.empty())
    {
        m_ipcServer = std::make_unique<httplib::Server>();

        applyServerTuning(*m_ipcServer);
        m_ipcServer->set_read_timeout(std::chrono::seconds(m_rpcReadTimeout));
        m_ipcServer->set_write_timeout(std::chrono::seconds(m_rpcWriteTimeout));
        m_ipcServer->set_payload_max_length(static_cast<size_t>(m_rpcMaxRequestBodyBytes));
        m_ipcServer->set_error_logger([](const httplib::Error &error, const httplib::Request *) {
            Logger::logger.log(
                "RPC IPC server error: " + httplib::to_string(error),
                Logger::WARNING,
                { Logger::DAEMON_RPC }
            );
        });

        setupRoutes(*m_ipcServer, true);
    }
}

void RpcServer::setupRoutes(httplib::Server &srv, const bool isIpc)
{
    const bool bodyRequired = true;
    const bool bodyNotRequired = false;

    const bool syncRequired = true;
    const bool syncNotRequired = false;

    /* Route the request through our middleware function, before forwarding
       to the specified function */
    const auto router = [this, isIpc](const auto function, const RpcMode routePermissions, const bool isBodyRequired, const bool syncRequired) {
        return [=](const httplib::Request &req, httplib::Response &res) {
            /* Pass the inputted function with the arguments passed through
               to middleware */
            middleware(
                req,
                res,
                routePermissions,
                isBodyRequired,
                syncRequired,
                isIpc,
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
        else if (method == "f_transactions_by_payment_id_json")
        {
            /* Explorer mode: this walks a database index, so it is not
               something a plain node should answer for anyone who asks. */
            router(
                &RpcServer::getTransactionHashesByPaymentId,
                RpcMode::Explorer,
                bodyRequired,
                syncNotRequired)(req, res);
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
    /* Bind the IPC socket on this thread, before anything else starts. The
       permission window in Ipc::bindServer is closed with the process umask,
       which every thread shares, so it must not overlap another listener
       coming up. */
    if (m_ipcServer)
    {
        std::string error;

        if (Common::Ipc::bindServer(*m_ipcServer, m_ipcPath, m_ipcMode, m_ipcGroup, error))
        {
            m_ipcBound = true;

            Logger::logger.log(
                "RPC IPC listener bound to " + Common::Ipc::describe(m_ipcPath) + " with mode "
                    + Common::Ipc::formatMode(m_ipcMode)
                    + (m_ipcGroup.empty() ? "" : " group " + m_ipcGroup),
                Logger::INFO,
                { Logger::DAEMON_RPC }
            );

            m_ipcThread = std::thread(&RpcServer::listenIpc, this);
        }
        else
        {
            /* Same policy as the IPv6 listener: a failed extra listener warns
               and the node keeps serving on the ones that did come up. */
            std::cout << WarningMsg("Failed to start RPC IPC listener: " + error) << std::endl;

            Logger::logger.log(
                "Failed to start RPC IPC listener: " + error,
                Logger::WARNING,
                { Logger::DAEMON_RPC }
            );
        }
    }

    m_serverThread = std::thread(&RpcServer::listen, this);

    if (!m_ipv6Host.empty())
    {
        m_ipv6Thread = std::thread(&RpcServer::listenIpv6, this);
    }
}

void RpcServer::listen()
{
    const auto isListening = m_server->listen(m_host, m_port);

    if (!isListening)
    {
        std::cout << WarningMsg("Failed to start RPC server.") << std::endl;
        exit(1);
    }
}

void RpcServer::listenIpv6()
{
    const auto isListening = m_ipv6Server->listen(m_ipv6Host, m_port);

    if (!isListening)
    {
        std::cout << WarningMsg("Failed to start IPv6 RPC server on [")
                  << WarningMsg(m_ipv6Host)
                  << WarningMsg("].") << std::endl;
    }
}

void RpcServer::listenIpc()
{
    if (!m_ipcServer->listen_after_bind())
    {
        std::cout << WarningMsg("RPC IPC listener on " + m_ipcPath + " stopped unexpectedly.") << std::endl;
    }
}

void RpcServer::stop()
{
    m_server->stop();

    if (!m_ipv6Host.empty())
    {
        m_ipv6Server->stop();
    }

    if (m_ipcServer && m_ipcBound)
    {
        m_ipcServer->stop();
    }

    if (m_serverThread.joinable())
    {
        m_serverThread.join();
    }

    if (m_ipv6Thread.joinable())
    {
        m_ipv6Thread.join();
    }

    if (m_ipcThread.joinable())
    {
        m_ipcThread.join();
    }

    /* Nothing else removes the socket file, and a leftover one blocks the next
       start until it is cleared by hand. */
    if (m_ipcBound)
    {
        Common::Ipc::cleanup(m_ipcPath);
        m_ipcBound = false;
    }
}

std::tuple<std::string, uint16_t> RpcServer::getConnectionInfo()
{
    return {m_host, m_port};
}

std::string RpcServer::getIpcPath() const
{
    /* Reported only once the bind actually succeeded, so a caller that routes
       itself over IPC never ends up pointed at a socket that was never
       created. */
    return m_ipcBound ? m_ipcPath : std::string();
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
    const bool isIpc,
    std::function<std::tuple<Error, uint16_t>(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body)> handler)
{
    /* An AF_UNIX peer has no address. httplib leaves remote_addr empty and
       puts the peer's process id in remote_port instead, which is the only
       thing worth naming this caller by in a log. */
    const std::string clientIp = isIpc ? ("ipc pid " + std::to_string(req.remote_port)) : getClientIp(req);

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

    /* On the IPC socket the mode on the socket file already decided who is
       allowed to be here, and the kernel enforced it. Demanding the shared
       secret on top adds nothing, and would mean every local integration has
       to be handed the token to do what its uid already entitles it to. An
       operator who wants both can set --rpc-ipc-require-token. */
    if (!m_rpcAccessToken.empty() && (!isIpc || m_ipcRequireToken))
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

    /* IPC callers are exempt for the same reason loopback is: the rate limiter
       exists to blunt anonymous remote traffic, and a process that cleared the
       socket's permissions is neither anonymous nor remote. Rate limiting it
       would only throttle a local wallet mid-sync. */
    if (!isIpc && !clientIp.empty() && clientIp != "127.0.0.1" && clientIp != "::1")
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

std::optional<std::string> RpcServer::lookupSyncCache(const WalletSyncCacheKey &key)
{
    std::lock_guard<std::mutex> lock(m_syncCacheMutex);

    const auto it = m_syncCacheIndex.find(key);

    if (it == m_syncCacheIndex.end())
    {
        return std::nullopt;
    }

    /* Touch it, so a range everyone is currently syncing past outlives one
       that a single wallet asked for once. */
    m_syncCacheEntries.splice(m_syncCacheEntries.begin(), m_syncCacheEntries, it->second);

    return it->second->second;
}

void RpcServer::storeSyncCache(
    const WalletSyncCacheKey &key,
    const std::string &body,
    const std::vector<WalletTypes::WalletBlockInfo> &blocks,
    const uint64_t topBlockIndex)
{
    if (m_syncCacheMaxBytes == 0 || blocks.empty() || body.size() > m_syncCacheMaxBytes)
    {
        return;
    }

    /* The key's start index came from a resolve that ran before the core call,
       so a reorg in between could have moved the core's own answer earlier.
       A body that begins below the height we are about to file it under would
       hand a later caller blocks it did not ask for, so drop it instead. It
       may legitimately begin above, when everything in between held nothing
       worth sending. */
    if (blocks.front().blockHeight < key.startIndex)
    {
        return;
    }

    /* Only cache a range far enough behind the tip that this node will never
       reorganise it away - it prunes alt chains that fork deeper than
       CRYPTONOTE_MAX_ALT_BLOCK_DEPTH, so it cannot adopt one either. Doubling
       that leaves room for the tip to advance while the entry is live. */
    const uint64_t reorgSafetyMargin = 2 * CryptoNote::parameters::CRYPTONOTE_MAX_ALT_BLOCK_DEPTH;

    const uint64_t highestHeight = blocks.back().blockHeight;

    if (highestHeight + reorgSafetyMargin > topBlockIndex)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_syncCacheMutex);

    /* Another thread may have inserted the same range while we were building
       ours; keep whichever is already there rather than duplicating it. */
    if (m_syncCacheIndex.count(key) != 0)
    {
        return;
    }

    m_syncCacheEntries.emplace_front(key, body);
    m_syncCacheIndex.emplace(key, m_syncCacheEntries.begin());
    m_syncCacheBytes += body.size();

    while (m_syncCacheBytes > m_syncCacheMaxBytes && !m_syncCacheEntries.empty())
    {
        const auto &oldest = m_syncCacheEntries.back();

        m_syncCacheBytes -= oldest.second.size();
        m_syncCacheIndex.erase(oldest.first);
        m_syncCacheEntries.pop_back();
    }
}

void RpcServer::discardSyncCacheOnReorg(const uint64_t topBlockIndex)
{
    if (m_syncCacheMaxBytes == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_syncCacheMutex);

    /* The tip only ever moves backwards when the chain reorganised. Cached
       bodies describe blocks that may no longer be on the main chain, and
       there is no cheap way to tell which, so drop the lot. This costs a
       rebuild roughly never - a node that reorganises this often has bigger
       problems than its sync cache. */
    if (topBlockIndex < m_syncCacheTopBlockIndex)
    {
        Logger::logger.log(
            "Chain reorganised, discarding " + std::to_string(m_syncCacheEntries.size())
                + " cached wallet sync responses",
            Logger::DEBUG,
            { Logger::DAEMON_RPC }
        );

        m_syncCacheEntries.clear();
        m_syncCacheIndex.clear();
        m_syncCacheBytes = 0;
    }

    m_syncCacheTopBlockIndex = topBlockIndex;
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

    /* The map is keyed by remote address, so without this it grows without
       bound - one entry per address that ever touched the node, which an
       attacker on a large address range controls directly. Entries from an
       older window carry no state worth keeping, so drop them when we roll
       over into a new one. */
    if (windowStart != m_rateLimitWindowStart)
    {
        m_rateLimitByIp.clear();
        m_rateLimitWindowStart = windowStart;
    }

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

        /* Only the block version is wanted from this, but getBlockDetails builds
           the whole block including details for every transaction in it, which
           needs the transaction records a lite node does not keep below its lite
           height. While such a node is still catching up its own top block sits
           down there, so this threw and took the entire endpoint down with it -
           /info answered BUSY, and the status console command with it, for the
           whole of a very long sync. The version is worth losing; everything
           else here is not. */
        uint8_t topMajorVersion = 0;
        uint8_t topMinorVersion = 0;

        try
        {
            const auto blockDetails = m_core->getBlockDetails(m_core->getTopBlockIndex());
            topMajorVersion = blockDetails.majorVersion;
            topMinorVersion = blockDetails.minorVersion;
        }
        catch (const std::exception &)
        {
            /* Left at zero, which reads as "not known from here". */
        }

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
        j["seed_nodes_count"] = m_p2p->get_seed_nodes_count();
        j["last_seed_bootstrap"] = m_p2p->get_last_seed_bootstrap_time();
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
        /* Wallets read these to floor their scan height: nothing below
           lite_start_height can be found on this daemon. See LITENODE.md. */
        j["lite"] = m_syncManager->getLiteNodeHeight() != 0;
        j["lite_start_height"] = m_syncManager->getLiteNodeHeight();
        j["sync_active_peers"] = m_syncManager->getSyncActivePeers();
        j["sync_avg_batch_size"] = m_syncManager->getSyncAvgBatchSize();
        j["sync_demoted_peers"] = m_syncManager->getSyncDemotedPeers();
        j["major_version"] = topMajorVersion;
        j["minor_version"] = topMinorVersion;
        j["version"] = PROJECT_VERSION;
        j["status"] = "OK";
        j["start_time"] = m_core->getStartTime();

        /* Response compression is decided when the daemon is built - httplib
           only gzips if zlib was found at configure time. A node built on a
           box without the zlib headers silently sends several times as many
           bytes to every syncing wallet, forever, with nothing to say so.
           Publishing it here lets an operator see it, and lets a wallet
           report it. */
        j["compression"] = RpcServer::compressionAlgorithm();

        nlohmann::json syncFeatures = nlohmann::json::array();
        syncFeatures.push_back(CryptoNote::SyncFeatures::SKIP_EMPTY_BLOCKS);
        syncFeatures.push_back(CryptoNote::SyncFeatures::BASE64_ENCODING);
        syncFeatures.push_back(CryptoNote::SyncFeatures::HEIGHT_RANGE);
        j["sync_features"] = syncFeatures;

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

        /* Deliberately not an error when we found fewer than were asked for.
           Failing the whole batch told the caller nothing it could act on - and
           because a non-200 is indistinguishable from an unreachable node, it
           surfaced in wallets as "daemon offline". Return what this denomination
           actually has and let the wallet decide: it can drop to a ring size the
           chain supports, or report a shortage it can name. Wallets predating
           this already check the per-amount count themselves, so a short list
           gives them the right error too. */
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

    uint64_t startHeight = hasMember(body, "startHeight")
        ? getUint64FromJSON(body, "startHeight")
        : 0;

    uint64_t startTimestamp = hasMember(body, "startTimestamp")
        ? getUint64FromJSON(body, "startTimestamp")
        : 0;

    /* On a lite node nothing below the lite height was ever stored, so a wallet
       asking to start below it is served from the lite height rather than fed an
       empty response forever. The wallet learns where that floor is from
       lite_start_height in /info. A timestamp start is dropped at the same time:
       the timestamp indexes do not reach down there either, so leaving it set
       would only resolve the start back below the floor. See LITENODE.md. */
    if (const uint64_t liteStartHeight = m_syncManager->getLiteNodeHeight();
        liteStartHeight != 0 && startHeight < liteStartHeight)
    {
        startHeight = liteStartHeight;
        startTimestamp = 0;
    }

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

    /* Opt in, so wallets that predate the flag keep receiving key_offsets */
    const bool skipInputKeyOffsets = hasMember(body, "skipInputKeyOffsets")
        ? getBoolFromJSON(body, "skipInputKeyOffsets")
        : false;

    /* Also opt in - leaving blocks out changes which heights a response
       covers, and a wallet that does not know that would read the gaps as a
       chain fork. */
    const bool skipEmptyBlocks = hasMember(body, "skipEmptyBlocks")
        ? getBoolFromJSON(body, "skipEmptyBlocks")
        : false;

    /* Exclusive upper bound on the heights this response may cover. A caller
       fetching several windows at once needs them to tile the chain exactly,
       which it cannot arrange if we choose where each one ends. Zero, and
       absent, both mean unbounded. */
    const uint64_t endHeight = hasMember(body, "endHeight")
        ? getUint64FromJSON(body, "endHeight")
        : 0;

    const std::string encoding = hasMember(body, "encoding")
        ? getStringFromJSON(body, "encoding")
        : "hex";

    if (encoding != "hex" && encoding != "base64")
    {
        failRequest(400, "encoding must be either 'hex' or 'base64'", res);
        return {SUCCESS, 400};
    }

    const bool base64 = encoding == "base64";

    const auto encodePod = [base64](const auto &pod) {
        return base64 ? Common::podToBase64(pod) : Common::podToHex(pod);
    };

    /* Every wallet syncing from the same height asks for the same range, and
       assembling one is thousands of database reads plus the encoding of the
       result. Serve a body we have already built where we can. */
    const uint64_t topBlockIndex = m_core->getTopBlockIndex();

    discardSyncCacheOnReorg(topBlockIndex);

    WalletSyncCacheKey cacheKey {};
    bool cacheable = false;

    uint64_t resolvedStartIndex = 0;

    if (m_syncCacheMaxBytes > 0
        && m_core->getWalletSyncStartIndex(blockHashCheckpoints, startHeight, startTimestamp, resolvedStartIndex))
    {
        cacheKey = {
            resolvedStartIndex,
            blockCount,
            endHeight,
            skipCoinbaseTransactions,
            skipInputKeyOffsets,
            skipEmptyBlocks,
            base64
        };

        cacheable = true;

        if (const auto cached = lookupSyncCache(cacheKey))
        {
            res.body = *cached;
            return {SUCCESS, 200};
        }
    }

    std::vector<WalletTypes::WalletBlockInfo> walletBlocks;
    std::optional<WalletTypes::TopBlock> topBlockInfo;
    uint64_t scannedToHeight = 0;

    const bool success = m_core->getWalletSyncData(
        blockHashCheckpoints,
        startHeight,
        startTimestamp,
        blockCount,
        endHeight,
        skipCoinbaseTransactions,
        skipEmptyBlocks,
        walletBlocks,
        topBlockInfo,
        scannedToHeight
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
                outObj["key"] = encodePod(output.key);
                outObj["amount"] = output.amount;

                /* We already have this from the database. Sending it saves the
                   wallet a whole extra /get_global_indexes_for_range round trip
                   for every block that turns out to contain one of its outputs.
                   Wallets that don't know the field ignore it. */
                if (output.globalOutputIndex)
                {
                    outObj["globalIndex"] = *output.globalOutputIndex;
                }

                cbOutputs.push_back(outObj);
            }
            nlohmann::json cbTx;
            cbTx["outputs"] = cbOutputs;
            cbTx["hash"] = encodePod(block.coinbaseTransaction->hash);
            cbTx["txPublicKey"] = encodePod(block.coinbaseTransaction->transactionPublicKey);
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
                outObj["key"] = encodePod(output.key);
                outObj["amount"] = output.amount;

                if (output.globalOutputIndex)
                {
                    outObj["globalIndex"] = *output.globalOutputIndex;
                }

                txOutputs.push_back(outObj);
            }

            nlohmann::json txInputs = nlohmann::json::array();
            for (const auto &input : transaction.keyInputs)
            {
                nlohmann::json inputObj;
                inputObj["amount"] = input.amount;
                inputObj["k_image"] = encodePod(input.keyImage);

                /* The ring member offsets are only needed to build a spend, and
                   a syncing wallet builds its own from get_random_outs. They are
                   a large slice of the response body, so a wallet that has said
                   it doesn't want them gets them omitted. */
                if (!skipInputKeyOffsets)
                {
                    nlohmann::json offsets = nlohmann::json::array();
                    for (const auto &offset : input.outputIndexes)
                    {
                        offsets.push_back(offset);
                    }
                    inputObj["key_offsets"] = offsets;
                }

                txInputs.push_back(inputObj);
            }

            nlohmann::json txObj;
            txObj["outputs"] = txOutputs;
            txObj["hash"] = encodePod(transaction.hash);
            txObj["txPublicKey"] = encodePod(transaction.transactionPublicKey);
            txObj["unlockTime"] = transaction.unlockTime;
            txObj["paymentID"] = transaction.paymentID;
            txObj["inputs"] = txInputs;
            txArr.push_back(txObj);
        }

        blockObj["transactions"] = txArr;
        blockObj["blockHeight"] = block.blockHeight;
        blockObj["blockHash"] = encodePod(block.blockHash);
        blockObj["blockTimestamp"] = block.blockTimestamp;
        itemsArr.push_back(blockObj);
    }

    nlohmann::json j;
    j["items"] = itemsArr;

    if (topBlockInfo)
    {
        nlohmann::json topBlock;
        topBlock["hash"] = encodePod(topBlockInfo->hash);
        topBlock["height"] = topBlockInfo->height;
        j["topBlock"] = topBlock;
    }

    /* How far the daemon actually looked, so a caller can move past a stretch
       of blocks that held nothing for it without having to ask again to find
       out there was nothing there. */
    if (scannedToHeight != 0)
    {
        j["scannedToHeight"] = scannedToHeight;
    }

    j["synced"] = walletBlocks.empty();
    j["status"] = "OK";
    res.body = j.dump();

    if (cacheable)
    {
        storeSyncCache(cacheKey, res.body, walletBlocks, topBlockIndex);
    }

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

    /* Unlike the sync endpoints there is nothing sensible to clamp to here - the
       caller asked about specific heights and a silently narrowed range would
       read as "these heights hold no transactions", which is a different and
       wrong answer. Say plainly that this node cannot know. */
    if (const uint64_t liteStartHeight = m_syncManager->getLiteNodeHeight();
        liteStartHeight != 0 && startHeight < liteStartHeight)
    {
        failRequest(
            400,
            "This node is a lite node and stores no transaction data below height "
                + std::to_string(liteStartHeight),
            res);
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
        const auto type = std::holds_alternative<CryptoNote::BaseInput>(input) ? "ff" : "02";

        nlohmann::json valueObj;
        if (std::holds_alternative<CryptoNote::BaseInput>(input))
        {
            valueObj["height"] = std::get<CryptoNote::BaseInput>(input).blockIndex;
        }
        else
        {
            const auto keyInput = std::get<CryptoNote::KeyInput>(input);
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
        dataObj["key"] = Common::podToHex(std::get<CryptoNote::KeyOutput>(output.target).key);

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
    const Utilities::ParsedExtra parsedTxExtra = Utilities::parseExtra(transaction.extra);

    txDetailsObj["paymentId"] = parsedTxExtra.paymentID;

    /* When set, paymentId is ciphertext - only the sender and the receiver hold
       the shared secret needed to read it. Explorers should label it as
       encrypted rather than presenting it as a readable payment ID. */
    txDetailsObj["paymentIdEncrypted"] = parsedTxExtra.paymentIDEncrypted;
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
                if (std::holds_alternative<CryptoNote::KeyInput>(in))
                {
                    return acc + std::get<CryptoNote::KeyInput>(in).amount;
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

std::tuple<Error, uint16_t> RpcServer::getTransactionHashesByPaymentId(
    const httplib::Request &req,
    httplib::Response &res,
    const nlohmann::json &body)
{
    const auto params = getObjectFromJSON(body, "params");
    const auto paymentIdStr = getStringFromJSON(params, "paymentId");

    /* Only long payment IDs are indexed. A short one is encrypted against the
       shared secret with its receiver, so the same payment ID produces
       different bytes in every transaction and there is nothing stable to look
       up. Say that, rather than returning an empty result that reads as "this
       payment ID was never used". */
    if (paymentIdStr.length() == WalletConfig::shortPaymentIDLength)
    {
        failJsonRpcRequest(
            -1,
            "Short payment IDs are encrypted to their receiver and cannot be looked up. "
            "Only the sender and the receiver can read one.",
            res
        );

        return {SUCCESS, 200};
    }

    Crypto::Hash paymentId;

    if (!Common::podFromHex(paymentIdStr, paymentId))
    {
        failJsonRpcRequest(
            -1,
            "Payment ID specified is not 64 valid hex characters!",
            res
        );

        return {SUCCESS, 200};
    }

    std::vector<Crypto::Hash> hashes = m_core->getTransactionHashesByPaymentId(paymentId);

    /* A payment ID that has been reused - a shared exchange deposit ID, say -
       can name a very large number of transactions. Bound the answer so one
       lookup cannot pull an unbounded amount out of the database, and tell the
       caller when the list was cut short rather than silently truncating. */
    const size_t total = hashes.size();
    const bool truncated = total > m_rpcMaxBlockCount;

    if (truncated)
    {
        hashes.resize(m_rpcMaxBlockCount);
    }

    nlohmann::json hashArr = nlohmann::json::array();

    for (const auto &hash : hashes)
    {
        hashArr.push_back(Common::podToHex(hash));
    }

    nlohmann::json result;
    result["status"] = "OK";
    result["transactionHashes"] = hashArr;
    result["totalCount"] = total;
    result["truncated"] = truncated;

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
                const auto type = std::holds_alternative<CryptoNote::BaseInput>(input) ? "ff" : "02";

                nlohmann::json valueObj;
                if (std::holds_alternative<CryptoNote::BaseInput>(input))
                {
                    valueObj["height"] = std::get<CryptoNote::BaseInput>(input).blockIndex;
                }
                else
                {
                    const auto keyInput = std::get<CryptoNote::KeyInput>(input);
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
                dataObj["key"] = Common::podToHex(std::get<CryptoNote::KeyOutput>(output.target).key);

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
            const auto type = std::holds_alternative<CryptoNote::BaseInput>(input) ? "ff" : "02";

            nlohmann::json valueObj;
            if (std::holds_alternative<CryptoNote::BaseInput>(input))
            {
                valueObj["height"] = std::get<CryptoNote::BaseInput>(input).blockIndex;
            }
            else
            {
                const auto keyInput = std::get<CryptoNote::KeyInput>(input);
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
            dataObj["key"] = Common::podToHex(std::get<CryptoNote::KeyOutput>(output.target).key);

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
                const auto type = std::holds_alternative<CryptoNote::BaseInputDetails>(input) ? "ff" : "02";

                nlohmann::json dataObj;
                if (std::holds_alternative<CryptoNote::BaseInputDetails>(input))
                {
                    const auto in = std::get<CryptoNote::BaseInputDetails>(input);
                    nlohmann::json inputSubObj;
                    inputSubObj["height"] = in.input.blockIndex;
                    dataObj["amount"] = in.amount;
                    dataObj["input"] = inputSubObj;
                }
                else
                {
                    const auto in = std::get<CryptoNote::KeyInputDetails>(input);
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
                dataObj["key"] = Common::podToHex(std::get<CryptoNote::KeyOutput>(output.output.target).key);

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

    uint64_t startHeight = hasMember(body, "startHeight")
        ? getUint64FromJSON(body, "startHeight")
        : 0;

    uint64_t startTimestamp = hasMember(body, "startTimestamp")
        ? getUint64FromJSON(body, "startTimestamp")
        : 0;

    /* On a lite node nothing below the lite height was ever stored, so a wallet
       asking to start below it is served from the lite height rather than fed an
       empty response forever. The wallet learns where that floor is from
       lite_start_height in /info. A timestamp start is dropped at the same time:
       the timestamp indexes do not reach down there either, so leaving it set
       would only resolve the start back below the floor. See LITENODE.md. */
    if (const uint64_t liteStartHeight = m_syncManager->getLiteNodeHeight();
        liteStartHeight != 0 && startHeight < liteStartHeight)
    {
        startHeight = liteStartHeight;
        startTimestamp = 0;
    }

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

