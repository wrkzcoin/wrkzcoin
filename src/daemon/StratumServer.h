// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "json_fwd.hpp"

#include <cryptonotecore/BlockchainMessages.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/MessageQueue.h>
#include <cryptonoteprotocol/ICryptoNoteProtocolQuery.h>
#include <cstdint>
#include <deque>
#include <list>
#include <logging/ILogger.h>
#include <logging/LoggerRef.h>
#include <memory>
#include <set>
#include <string>
#include <system/ContextGroup.h>
#include <system/Dispatcher.h>
#include <system/Event.h>
#include <system/TcpConnection.h>
#include <system/TcpListener.h>

namespace Daemon
{
    /* A stratum server, so that a stock miner can point straight at this node.

       Our blocks are Forknote lineage: the outer header is major, minor and
       prev_id, and the real timestamp and nonce live inside a merge-mining
       parent block after it. A miner that expects Monero's flat header cannot
       read that, which is why xmrig's own --daemon mode rejects our templates.

       None of that matters over stratum. What a miner hashes is the parent
       block's hashing serialization, and that is an ordinary CryptoNote blob
       with the nonce four bytes wide at offset 39 - exactly where every
       stratum miner already writes it. So the node does the block assembly
       and hands out plain blobs, and the miner never sees a block at all. */
    class StratumServer
    {
      public:
        StratumServer(
            System::Dispatcher &dispatcher,
            CryptoNote::Core &core,
            const CryptoNote::ICryptoNoteProtocolQuery &protocol,
            std::shared_ptr<Logging::ILogger> logger,
            const std::string &bindAddress,
            uint16_t port,
            uint64_t shareDifficulty,
            size_t maxConnections);

        ~StratumServer();

        StratumServer(const StratumServer &) = delete;

        StratumServer &operator=(const StratumServer &) = delete;

        /* False when the listener could not be bound. Mining is optional, so
           the caller keeps the node running either way. */
        bool start();

        void stop();

      private:
        struct Job
        {
            std::string id;

            CryptoNote::BlockTemplate blockTemplate;

            /* The difficulty the chain wants. A share meeting this is a block. */
            uint64_t difficulty = 0;

            uint32_t height = 0;

            std::string blobHex;

            /* Nonces already submitted against this job, so a rig that resends
               one is answered rather than counted twice. */
            std::set<uint32_t> seenNonces;
        };

        struct Client
        {
            Client(System::Dispatcher &dispatcher, System::TcpConnection &&connection);

            System::TcpConnection connection;

            System::Event outboxReady;

            std::deque<std::string> outbox;

            /* Most recent jobs, newest last. A few are kept so a share that
               crosses a new block is still checked against the job it was
               found on. */
            std::deque<Job> jobs;

            std::string sessionId;

            std::string address;

            std::string agent;

            std::string peer;

            bool loggedIn = false;

            bool closing = false;

            uint64_t acceptedShares = 0;

            uint64_t foundBlocks = 0;
        };

        using ClientPtr = std::shared_ptr<Client>;

        using QueueGuard = CryptoNote::MesageQueueGuard<CryptoNote::ICore, CryptoNote::BlockchainMessage>;

        void acceptLoop();

        void readLoop(ClientPtr client);

        void writeLoop(ClientPtr client);

        void blockLoop();

        void handleLine(const ClientPtr &client, const std::string &line);

        void handleLogin(const ClientPtr &client, const nlohmann::json &request);

        void handleGetJob(const ClientPtr &client, const nlohmann::json &request);

        void handleSubmit(const ClientPtr &client, const nlohmann::json &request);

        /* Builds a fresh template for this client and appends it to its job
           history. Returns false with a reason the caller can report. */
        bool refreshJob(const ClientPtr &client, std::string &error);

        /* Whether this node is far enough along to be worth mining on. A node
           still pulling the chain would hand out templates built on a block
           the network left behind long ago, so every one of them would be
           orphaned - and re-jobbing on each of the thousands of blocks
           arriving per minute would evict a miner's job before it could
           answer. */
        bool chainReady() const;

        nlohmann::json describeJob(const Client &client) const;

        void sendJob(const ClientPtr &client);

        void broadcastJobs();

        void send(const ClientPtr &client, const nlohmann::json &payload);

        void replyResult(const ClientPtr &client, const nlohmann::json &request, const nlohmann::json &result);

        void replyError(const ClientPtr &client, const nlohmann::json &request, const std::string &message);

        void dropClient(const ClientPtr &client);

        System::Dispatcher &m_dispatcher;

        CryptoNote::Core &m_core;

        const CryptoNote::ICryptoNoteProtocolQuery &m_protocol;

        Logging::LoggerRef m_logger;

        std::string m_bindAddress;

        uint16_t m_port;

        /* 0 means "only tell me about shares that are actually blocks", which
           is what solo mining wants. Anything else lowers the miner's target
           so it reports progress, while a block still needs the real one. */
        uint64_t m_shareDifficulty;

        size_t m_maxConnections;

        System::TcpListener m_listener;

        System::ContextGroup m_contextGroup;

        CryptoNote::MessageQueue<CryptoNote::BlockchainMessage> m_queue;

        std::unique_ptr<QueueGuard> m_queueGuard;

        std::list<ClientPtr> m_clients;

        uint64_t m_idCounter = 0;

        bool m_running = false;

        /* Set once the connection cap has been reported, so a rig retrying in
           a loop does not fill the log with it. */
        bool m_reportedConnectionLimit = false;
    };
} // namespace Daemon
