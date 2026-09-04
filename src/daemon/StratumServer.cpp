// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "StratumServer.h"

#include "json.hpp"

#include <common/CheckDifficulty.h>
#include <common/StringTools.h>
#include <common/TransactionExtra.h>
#include <cryptonotecore/AddBlockErrorCondition.h>
#include <cryptonotecore/CachedBlock.h>
#include <cstring>
#include <errors/ValidateParameters.h>
#include <iomanip>
#include <limits>
#include <serialization/SerializationTools.h>
#include <sstream>
#include <system/InterruptedException.h>
#include <system/IpAddress.h>
#include <utilities/Addresses.h>

using namespace Logging;

namespace Daemon
{
    namespace
    {
        /* How many past jobs a connection keeps. A share found just as a new
           block lands still arrives against the job it was working on. */
        constexpr size_t JOB_HISTORY = 4;

        /* A stratum line is a small JSON object. Anything past this is either
           a broken client or someone probing, and gets the connection closed
           rather than an unbounded buffer. */
        constexpr size_t MAX_LINE_BYTES = 8192;

        constexpr size_t READ_CHUNK_BYTES = 4096;

        /* The extra nonce handed to each connection, so two rigs pointed at
           the same node never grind identical nonce space. */
        constexpr size_t EXTRA_NONCE_BYTES = 8;

        std::string algorithmName(const uint8_t majorVersion)
        {
            /* Mirrors HASHING_ALGORITHMS_BY_BLOCK_VERSION, in the spelling
               miners use. Only the current fork is reachable in practice; the
               rest are here so this stays honest across a version bump. */
            switch (majorVersion)
            {
                case CryptoNote::BLOCK_MAJOR_VERSION_4:
                    return "cn-lite/1";
                case CryptoNote::BLOCK_MAJOR_VERSION_5:
                    return "cn-pico/trtl";
                case CryptoNote::BLOCK_MAJOR_VERSION_6:
                    return "argon2/chukwa";
                case CryptoNote::BLOCK_MAJOR_VERSION_7:
                    return "cn/upx2";
                default:
                    return "cn/0";
            }
        }

        /* Miners compare the top eight bytes of the hash, read little endian,
           against this. check_hash() is still what decides a share here - the
           target only sets how often the miner bothers to send one. */
        std::string encodeTarget(const uint64_t difficulty)
        {
            const uint64_t target =
                difficulty <= 1 ? std::numeric_limits<uint64_t>::max() : std::numeric_limits<uint64_t>::max() / difficulty;

            uint8_t bytes[sizeof(target)];
            std::memcpy(bytes, &target, sizeof(target));

            return Common::toHex(bytes, sizeof(bytes));
        }

        std::string jsonString(const nlohmann::json &object, const std::string &key)
        {
            if (!object.is_object() || !object.contains(key) || !object.at(key).is_string())
            {
                return std::string();
            }

            return object.at(key).get<std::string>();
        }
    } // namespace

    StratumServer::Client::Client(System::Dispatcher &dispatcher, System::TcpConnection &&connection):
        connection(std::move(connection)),
        outboxReady(dispatcher)
    {
    }

    StratumServer::StratumServer(
        System::Dispatcher &dispatcher,
        CryptoNote::Core &core,
        const CryptoNote::ICryptoNoteProtocolQuery &protocol,
        std::shared_ptr<Logging::ILogger> logger,
        const std::string &bindAddress,
        const uint16_t port,
        const uint64_t shareDifficulty,
        const size_t maxConnections):
        m_dispatcher(dispatcher),
        m_core(core),
        m_protocol(protocol),
        m_logger(logger, "StratumServer"),
        m_bindAddress(bindAddress),
        m_port(port),
        m_shareDifficulty(shareDifficulty),
        m_maxConnections(maxConnections == 0 ? 1 : maxConnections),
        m_contextGroup(dispatcher),
        m_queue(dispatcher)
    {
    }

    StratumServer::~StratumServer()
    {
        stop();
    }

    bool StratumServer::start()
    {
        if (m_running)
        {
            return true;
        }

        try
        {
            m_listener = System::TcpListener(m_dispatcher, System::IpAddress(m_bindAddress), m_port);
        }
        catch (const std::exception &e)
        {
            m_logger(ERROR, BRIGHT_RED) << "Failed to bind stratum server to " << m_bindAddress << ":" << m_port << " - "
                                        << e.what();
            return false;
        }

        m_queueGuard.reset(new QueueGuard(m_core, m_queue));
        m_running = true;

        m_contextGroup.spawn([this] { acceptLoop(); });
        m_contextGroup.spawn([this] { blockLoop(); });

        m_logger(INFO) << "Stratum server listening on " << m_bindAddress << ":" << m_port;

        if (m_shareDifficulty == 0)
        {
            m_logger(INFO) << "Stratum shares are set at the network difficulty: a miner only reports when it has "
                              "actually found a block.";
        }
        else
        {
            m_logger(INFO) << "Stratum share difficulty fixed at " << m_shareDifficulty
                           << ". Shares below the network difficulty are counted, not submitted.";
        }

        return true;
    }

    void StratumServer::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;
        m_queue.stop();

        for (const auto &client : m_clients)
        {
            client->closing = true;
            client->outboxReady.set();
        }

        m_contextGroup.interrupt();
        m_contextGroup.wait();

        m_clients.clear();
        m_queueGuard.reset();
    }

    void StratumServer::acceptLoop()
    {
        while (m_running)
        {
            try
            {
                System::TcpConnection connection = m_listener.accept();

                if (m_clients.size() >= m_maxConnections)
                {
                    /* Letting it fall out of scope closes it. Saying so once
                       per refusal is what tells an operator the cap is the
                       reason their rig cannot get in. */
                    m_logger(WARNING) << "Refused a stratum connection: already at the " << m_maxConnections
                                      << " connection limit";
                    continue;
                }

                auto client = std::make_shared<Client>(m_dispatcher, std::move(connection));

                try
                {
                    client->peer = client->connection.getPeerIpAddress().toString();
                }
                catch (const std::exception &)
                {
                    client->peer = "unknown";
                }

                m_clients.push_back(client);

                m_contextGroup.spawn([this, client] { readLoop(client); });
                m_contextGroup.spawn([this, client] { writeLoop(client); });
            }
            catch (const System::InterruptedException &)
            {
                break;
            }
            catch (const std::exception &e)
            {
                m_logger(WARNING) << "Stratum accept failed: " << e.what();
            }
        }
    }

    void StratumServer::readLoop(ClientPtr client)
    {
        std::string buffer;
        std::vector<uint8_t> chunk(READ_CHUNK_BYTES);

        try
        {
            while (m_running && !client->closing)
            {
                const size_t received = client->connection.read(chunk.data(), chunk.size());

                if (received == 0)
                {
                    break;
                }

                buffer.append(reinterpret_cast<const char *>(chunk.data()), received);

                size_t newline = buffer.find('\n');

                while (newline != std::string::npos)
                {
                    std::string line = buffer.substr(0, newline);
                    buffer.erase(0, newline + 1);

                    if (!line.empty() && line.back() == '\r')
                    {
                        line.pop_back();
                    }

                    if (!line.empty())
                    {
                        handleLine(client, line);
                    }

                    newline = buffer.find('\n');
                }

                if (buffer.size() > MAX_LINE_BYTES)
                {
                    m_logger(WARNING) << "Stratum client " << client->peer << " sent an oversized request; closing";
                    break;
                }
            }
        }
        catch (const System::InterruptedException &)
        {
        }
        catch (const std::exception &e)
        {
            m_logger(DEBUGGING) << "Stratum client " << client->peer << " read ended: " << e.what();
        }

        dropClient(client);
    }

    void StratumServer::writeLoop(ClientPtr client)
    {
        try
        {
            while (m_running && !client->closing)
            {
                if (client->outbox.empty())
                {
                    client->outboxReady.clear();

                    if (client->outbox.empty())
                    {
                        client->outboxReady.wait();
                    }

                    continue;
                }

                const std::string payload = std::move(client->outbox.front());
                client->outbox.pop_front();

                size_t sent = 0;

                while (sent < payload.size())
                {
                    const size_t written = client->connection.write(
                        reinterpret_cast<const uint8_t *>(payload.data()) + sent, payload.size() - sent);

                    if (written == 0)
                    {
                        client->closing = true;
                        break;
                    }

                    sent += written;
                }
            }
        }
        catch (const System::InterruptedException &)
        {
        }
        catch (const std::exception &e)
        {
            m_logger(DEBUGGING) << "Stratum client " << client->peer << " write ended: " << e.what();
        }

        client->closing = true;
    }

    void StratumServer::blockLoop()
    {
        while (m_running)
        {
            try
            {
                const CryptoNote::BlockchainMessage message = m_queue.front();
                m_queue.pop();

                bool topChanged = false;

                message.match(
                    [&topChanged](const CryptoNote::Messages::NewBlock &) { topChanged = true; },
                    [](const CryptoNote::Messages::NewAlternativeBlock &) {},
                    [&topChanged](const CryptoNote::Messages::ChainSwitch &) { topChanged = true; },
                    [](const CryptoNote::Messages::AddTransaction &) {},
                    [](const CryptoNote::Messages::DeleteTransaction &) {});

                if (topChanged)
                {
                    broadcastJobs();
                }
            }
            catch (const System::InterruptedException &)
            {
                break;
            }
            catch (const std::exception &e)
            {
                m_logger(WARNING) << "Stratum job refresh failed: " << e.what();
            }
        }
    }

    void StratumServer::handleLine(const ClientPtr &client, const std::string &line)
    {
        const auto request = nlohmann::json::parse(line, nullptr, false);

        if (request.is_discarded() || !request.is_object())
        {
            m_logger(DEBUGGING) << "Stratum client " << client->peer << " sent malformed JSON";
            client->closing = true;
            client->outboxReady.set();
            return;
        }

        const std::string method = jsonString(request, "method");

        if (method == "login")
        {
            handleLogin(client, request);
        }
        else if (method == "getjob")
        {
            handleGetJob(client, request);
        }
        else if (method == "submit")
        {
            handleSubmit(client, request);
        }
        else if (method == "keepalived" || method == "keepalive")
        {
            nlohmann::json result;
            result["status"] = "KEEPALIVED";
            replyResult(client, request, result);
        }
        else
        {
            replyError(client, request, "Unknown method");
        }
    }

    void StratumServer::handleLogin(const ClientPtr &client, const nlohmann::json &request)
    {
        const nlohmann::json params =
            request.contains("params") && request.at("params").is_object() ? request.at("params") : nlohmann::json::object();

        const std::string address = jsonString(params, "login");

        if (const Error error = validateAddresses({address}, false))
        {
            m_logger(WARNING) << "Stratum login from " << client->peer << " refused: " << error.getErrorMessage();
            replyError(client, request, error.getErrorMessage());
            client->closing = true;
            client->outboxReady.set();
            return;
        }

        if (!chainReady())
        {
            const std::string message = "Node is still synchronizing (" + std::to_string(m_core.getTopBlockIndex() + 1)
                + " of " + std::to_string(m_protocol.getObservedHeight())
                + "); mining would only produce orphans. Retry once it has caught up.";

            m_logger(INFO) << "Turned away a stratum miner from " << client->peer << ": still synchronizing";
            replyError(client, request, message);
            return;
        }

        client->address = address;
        client->agent = jsonString(params, "agent");
        client->sessionId = std::to_string(++m_idCounter);
        client->loggedIn = true;
        client->jobs.clear();

        std::string error;

        if (!refreshJob(client, error))
        {
            replyError(client, request, error);
            return;
        }

        nlohmann::json result;
        result["id"] = client->sessionId;
        result["job"] = describeJob(*client);
        result["status"] = "OK";
        result["extensions"] = nlohmann::json::array();

        replyResult(client, request, result);

        m_logger(INFO) << "Stratum miner connected from " << client->peer
                       << (client->agent.empty() ? "" : " (" + client->agent + ")") << " mining to " << address;
    }

    void StratumServer::handleGetJob(const ClientPtr &client, const nlohmann::json &request)
    {
        if (!client->loggedIn)
        {
            replyError(client, request, "Unauthenticated");
            return;
        }

        if (!chainReady())
        {
            replyError(client, request, "Node is still synchronizing");
            return;
        }

        std::string error;

        if (!refreshJob(client, error))
        {
            replyError(client, request, error);
            return;
        }

        replyResult(client, request, describeJob(*client));
    }

    void StratumServer::handleSubmit(const ClientPtr &client, const nlohmann::json &request)
    {
        if (!client->loggedIn)
        {
            replyError(client, request, "Unauthenticated");
            return;
        }

        const nlohmann::json params =
            request.contains("params") && request.at("params").is_object() ? request.at("params") : nlohmann::json::object();

        const std::string jobId = jsonString(params, "job_id");

        Job *job = nullptr;

        for (auto &candidate : client->jobs)
        {
            if (candidate.id == jobId)
            {
                job = &candidate;
                break;
            }
        }

        if (job == nullptr)
        {
            replyError(client, request, "Invalid job id");
            return;
        }

        std::vector<uint8_t> nonceBytes;

        if (!Common::fromHex(jsonString(params, "nonce"), nonceBytes) || nonceBytes.size() != sizeof(uint32_t))
        {
            replyError(client, request, "Malformed nonce");
            return;
        }

        uint32_t nonce = 0;
        std::memcpy(&nonce, nonceBytes.data(), sizeof(nonce));

        if (!job->seenNonces.insert(nonce).second)
        {
            replyError(client, request, "Duplicate share");
            return;
        }

        /* The nonce the miner rolled is serialized inside the parent block,
           so setting it on the template is all it takes to rebuild both the
           hashing blob and the block itself. */
        CryptoNote::BlockTemplate candidate = job->blockTemplate;
        candidate.nonce = nonce;

        Crypto::Hash longHash;

        try
        {
            const CryptoNote::CachedBlock cached(candidate);
            longHash = cached.getBlockLongHash();
        }
        catch (const std::exception &e)
        {
            m_logger(WARNING) << "Stratum share from " << client->peer << " could not be hashed: " << e.what();
            replyError(client, request, "Could not hash share");
            return;
        }

        /* Miners send the hash they got. When it disagrees with ours the rig
           is on the wrong algorithm, which is worth saying plainly instead of
           letting it look like bad luck. */
        const std::string claimed = jsonString(params, "result");

        if (!claimed.empty() && claimed != Common::podToHex(longHash))
        {
            m_logger(WARNING) << "Stratum share from " << client->peer << " hashed to something else - check that the "
                              << "miner is running " << algorithmName(candidate.majorVersion);
            replyError(client, request, "Invalid result");
            return;
        }

        const uint64_t shareDifficulty = m_shareDifficulty == 0 ? job->difficulty : m_shareDifficulty;

        if (!CryptoNote::check_hash(longHash, shareDifficulty))
        {
            replyError(client, request, "Low difficulty share");
            return;
        }

        client->acceptedShares++;

        if (!CryptoNote::check_hash(longHash, job->difficulty))
        {
            /* Counted, but not a block. Only reachable when the operator set
               a share difficulty below the network's. */
            nlohmann::json result;
            result["status"] = "OK";
            replyResult(client, request, result);
            return;
        }

        CryptoNote::BinaryArray blockBlob;

        if (!CryptoNote::toBinaryArray(candidate, blockBlob))
        {
            replyError(client, request, "Could not serialize block");
            return;
        }

        const auto submitResult = m_core.submitBlock(blockBlob);

        if (submitResult != CryptoNote::error::AddBlockErrorCondition::BLOCK_ADDED)
        {
            m_logger(WARNING) << "Stratum block from " << client->peer << " at height " << job->height
                              << " was rejected: " << submitResult.message();
            replyError(client, request, "Block not accepted");
            return;
        }

        client->foundBlocks++;

        m_logger(INFO, BRIGHT_GREEN) << "Stratum miner " << client->peer << " found block " << Common::podToHex(longHash)
                                     << " at height " << job->height;

        nlohmann::json result;
        result["status"] = "OK";
        replyResult(client, request, result);

        /* The chain message will follow and re-job everyone, but the rig that
           found it should not spend that round trip on a dead template. */
        sendJob(client);
    }

    bool StratumServer::chainReady() const
    {
        /* isSynchronized() latches once a peer sync has finished. The second
           clause is what keeps an isolated node - a private chain, or one with
           nothing to talk to - from being locked out forever: with nothing
           observed above us, we are the tip. */
        return m_protocol.isSynchronized() || m_core.getTopBlockIndex() + 1 >= m_protocol.getObservedHeight();
    }

    bool StratumServer::refreshJob(const ClientPtr &client, std::string &error)
    {
        Crypto::PublicKey publicSpendKey;
        Crypto::PublicKey publicViewKey;

        try
        {
            std::tie(publicSpendKey, publicViewKey) = Utilities::addressToKeys(client->address);
        }
        catch (const std::exception &e)
        {
            error = std::string("Could not read the mining address: ") + e.what();
            return false;
        }

        CryptoNote::BinaryArray extraNonce(EXTRA_NONCE_BYTES, 0);
        const uint64_t counter = ++m_idCounter;
        std::memcpy(extraNonce.data(), &counter, sizeof(counter));

        CryptoNote::BlockTemplate blockTemplate;
        uint64_t difficulty = 0;
        uint32_t height = 0;

        try
        {
            const auto [created, reason] =
                m_core.getBlockTemplate(blockTemplate, publicViewKey, publicSpendKey, extraNonce, difficulty, height);

            if (!created)
            {
                error = "Could not create a block template: " + reason;
                return false;
            }
        }
        catch (const std::exception &e)
        {
            error = std::string("Could not create a block template: ") + e.what();
            return false;
        }

        if (difficulty == 0)
        {
            error = "The chain reported a zero difficulty";
            return false;
        }

        /* getBlockTemplate leaves the parent block's merge-mining tag as a
           placeholder and expects whoever mines it to fill in the commitment -
           the bundled miner does this in adjustMergeMiningTag() before it
           starts hashing. Skip it and checkProofOfWorkV2 recomputes the aux
           root, finds it does not match the zeroed tag, and throws the block
           out as "Proof of work is too weak", which is a thoroughly misleading
           way to say the commitment was never written. The work itself was
           perfectly good.

           This has to happen before the hashing blob is built: the tag lives
           in the parent coinbase, and the parent's own merkle root is part of
           what gets hashed. The aux hash covers only the outer header, miner
           transaction and transaction hashes, so writing the tag cannot change
           what the tag commits to. */
        if (blockTemplate.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_2)
        {
            try
            {
                CryptoNote::TransactionExtraMergeMiningTag mmTag;
                mmTag.depth = 0;

                {
                    const CryptoNote::CachedBlock unsealed(blockTemplate);
                    mmTag.merkleRoot = unsealed.getAuxiliaryBlockHeaderHash();
                }

                blockTemplate.parentBlock.baseTransaction.extra.clear();

                if (!CryptoNote::appendMergeMiningTagToExtra(blockTemplate.parentBlock.baseTransaction.extra, mmTag))
                {
                    error = "Could not write the merge mining tag";
                    return false;
                }
            }
            catch (const std::exception &e)
            {
                error = std::string("Could not write the merge mining tag: ") + e.what();
                return false;
            }
        }

        Job job;
        job.id = std::to_string(++m_idCounter);
        job.difficulty = difficulty;
        job.height = height;

        try
        {
            const CryptoNote::CachedBlock cached(blockTemplate);

            /* What the proof of work actually runs over. Before version 2 that
               is the block itself; after it, the merge-mining parent header,
               which is the plain CryptoNote blob a miner expects. */
            const CryptoNote::BinaryArray &blob = blockTemplate.majorVersion == CryptoNote::BLOCK_MAJOR_VERSION_1
                                                      ? cached.getBlockHashingBinaryArray()
                                                      : cached.getParentBlockHashingBinaryArray(true);

            job.blobHex = Common::toHex(blob);
        }
        catch (const std::exception &e)
        {
            error = std::string("Could not build the hashing blob: ") + e.what();
            return false;
        }

        job.blockTemplate = std::move(blockTemplate);

        client->jobs.push_back(std::move(job));

        while (client->jobs.size() > JOB_HISTORY)
        {
            client->jobs.pop_front();
        }

        return true;
    }

    nlohmann::json StratumServer::describeJob(const Client &client) const
    {
        nlohmann::json job = nlohmann::json::object();

        if (client.jobs.empty())
        {
            return job;
        }

        const Job &current = client.jobs.back();

        job["blob"] = current.blobHex;
        job["job_id"] = current.id;
        job["target"] = encodeTarget(m_shareDifficulty == 0 ? current.difficulty : m_shareDifficulty);
        job["height"] = current.height;
        job["algo"] = algorithmName(current.blockTemplate.majorVersion);

        return job;
    }

    void StratumServer::sendJob(const ClientPtr &client)
    {
        if (!client->loggedIn || client->closing)
        {
            return;
        }

        std::string error;

        if (!refreshJob(client, error))
        {
            m_logger(WARNING) << "Could not hand " << client->peer << " a new job: " << error;
            return;
        }

        nlohmann::json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = "job";
        notification["params"] = describeJob(*client);

        send(client, notification);
    }

    void StratumServer::broadcastJobs()
    {
        if (!chainReady())
        {
            return;
        }

        /* refreshJob() reaches into the core and can yield, so iterate a copy:
           a connection may drop while we are part way through. */
        const std::list<ClientPtr> clients = m_clients;

        for (const auto &client : clients)
        {
            sendJob(client);
        }
    }

    void StratumServer::send(const ClientPtr &client, const nlohmann::json &payload)
    {
        if (client->closing)
        {
            return;
        }

        client->outbox.push_back(payload.dump() + "\n");
        client->outboxReady.set();
    }

    void StratumServer::replyResult(
        const ClientPtr &client,
        const nlohmann::json &request,
        const nlohmann::json &result)
    {
        nlohmann::json response;
        response["id"] = request.contains("id") ? request.at("id") : nlohmann::json(nullptr);
        response["jsonrpc"] = "2.0";
        response["error"] = nullptr;
        response["result"] = result;

        send(client, response);
    }

    void StratumServer::replyError(const ClientPtr &client, const nlohmann::json &request, const std::string &message)
    {
        nlohmann::json error;
        error["code"] = -1;
        error["message"] = message;

        nlohmann::json response;
        response["id"] = request.contains("id") ? request.at("id") : nlohmann::json(nullptr);
        response["jsonrpc"] = "2.0";
        response["error"] = error;
        response["result"] = nullptr;

        send(client, response);
    }

    void StratumServer::dropClient(const ClientPtr &client)
    {
        client->closing = true;
        client->outboxReady.set();

        for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
        {
            if (*it == client)
            {
                m_clients.erase(it);
                break;
            }
        }

        if (client->loggedIn)
        {
            m_logger(INFO) << "Stratum miner " << client->peer << " disconnected after " << client->acceptedShares
                           << " share(s) and " << client->foundBlocks << " block(s)";
        }
    }
} // namespace Daemon
