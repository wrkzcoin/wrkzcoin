// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cryptonotecore/BlockchainMessages.h>
#include <cryptonotecore/ICore.h>
#include <cryptonotecore/MessageQueue.h>
#include <cstdint>
#include <logging/ILogger.h>
#include <logging/LoggerRef.h>
#include <memory>
#include <string>
#include <system/ContextGroup.h>
#include <system/Dispatcher.h>
#include <vector>

namespace Daemon
{
    class ZmqPublisher
    {
      public:
        /* liteHeight is 0 for a full node. Above it, blocks below that height have
           no stored body, so the chain_main prefetch payload cannot be built for
           them and is not attempted. See LITENODE.md. */
        ZmqPublisher(
            System::Dispatcher &dispatcher,
            CryptoNote::ICore &core,
            std::shared_ptr<Logging::ILogger> logger,
            std::string endpoint,
            uint32_t liteHeight = 0);

        ~ZmqPublisher();

        bool start();

        void stop();

      private:
        using QueueGuard = CryptoNote::MesageQueueGuard<CryptoNote::ICore, CryptoNote::BlockchainMessage>;

        void consumeLoop();

        void publishMessage(const CryptoNote::BlockchainMessage &message);

        bool sendMultipart(const std::string &topic, const std::string &payload);

        static std::string hashToString(const Crypto::Hash &hash);

        static std::string hashesToJsonArray(const std::vector<Crypto::Hash> &hashes);

        static bool isNonLoopbackTcpEndpoint(const std::string &endpoint);

        static const char *deleteReasonToString(CryptoNote::Messages::DeleteTransaction::Reason reason);

        System::Dispatcher &m_dispatcher;
        CryptoNote::ICore &m_core;
        Logging::LoggerRef m_logger;
        std::string m_endpoint;
        /* 0 = full node; otherwise the height full block data starts at. */
        uint32_t m_liteHeight = 0;

        CryptoNote::MessageQueue<CryptoNote::BlockchainMessage> m_queue;
        std::unique_ptr<QueueGuard> m_queueGuard;
        System::ContextGroup m_contextGroup;

        bool m_running;
        uint64_t m_published;
        uint64_t m_dropped;

#ifdef WRKZ_ENABLE_ZMQ
        void *m_zmqContext;
        void *m_zmqSocket;
#endif
    };
} // namespace Daemon
