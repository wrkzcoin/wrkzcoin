// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <common/Notifier.h>
#include <cryptonotecore/BlockchainMessages.h>
#include <cryptonotecore/ICore.h>
#include <cryptonotecore/MessageQueue.h>
#include <cryptonoteprotocol/ICryptoNoteProtocolQuery.h>
#include <cstdint>
#include <logging/ILogger.h>
#include <logging/LoggerRef.h>
#include <memory>
#include <string>
#include <system/ContextGroup.h>
#include <system/Dispatcher.h>

namespace Daemon
{
    /* Bridges Core's BlockchainMessage stream to the Monero-style
       --block-notify / --reorg-notify / --tx-notify sinks.

       Runs a consumer fiber on the dispatcher (like ZmqPublisher) that only
       formats and enqueues; all blocking work (process spawn, HTTP) happens on
       the Tools::Notifier worker threads, so the node is never stalled. */
    class ChainNotifier
    {
      public:
        ChainNotifier(
            System::Dispatcher &dispatcher,
            CryptoNote::ICore &core,
            const CryptoNote::ICryptoNoteProtocolQuery &protocol,
            std::shared_ptr<Logging::ILogger> logger,
            const std::string &blockNotify,
            const std::string &reorgNotify,
            const std::string &txNotify,
            bool notifyDuringSync);

        ~ChainNotifier();

        /* False when no sink is enabled (nothing to do). */
        bool start();

        void stop();

        bool anyEnabled() const;

      private:
        using QueueGuard = CryptoNote::MesageQueueGuard<CryptoNote::ICore, CryptoNote::BlockchainMessage>;

        void consumeLoop();

        void handleMessage(const CryptoNote::BlockchainMessage &message);

        bool shouldNotifyAt(uint32_t blockIndex) const;

        void notifyBlock(uint32_t blockIndex, const Crypto::Hash &hash);

        static std::string hashToString(const Crypto::Hash &hash);

        Tools::Notifier::LogFn makeLogFn();

        System::Dispatcher &m_dispatcher;
        CryptoNote::ICore &m_core;
        const CryptoNote::ICryptoNoteProtocolQuery &m_protocol;
        Logging::LoggerRef m_logger;
        bool m_notifyDuringSync;

        Tools::Notifier m_blockNotifier;
        Tools::Notifier m_reorgNotifier;
        Tools::Notifier m_txNotifier;

        CryptoNote::MessageQueue<CryptoNote::BlockchainMessage> m_queue;
        std::unique_ptr<QueueGuard> m_queueGuard;
        System::ContextGroup m_contextGroup;

        bool m_running;

        /* Last main-chain top index seen; used to size reorgs. */
        uint32_t m_topIndex;
    };
} // namespace Daemon
