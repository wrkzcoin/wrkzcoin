// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainMonitor.h"
#include "Miner.h"
#include "MinerEvent.h"
#include "MiningConfig.h"
#include "logging/LoggerRef.h"

#include <atomic>
#include <optional>
#include <queue>
#include <system/ContextGroup.h>
#include <system/Event.h>

namespace System
{
    class Dispatcher;
}

namespace Miner
{
    class MinerManager
    {
      public:
        MinerManager(
            System::Dispatcher &dispatcher,
            const CryptoNote::MiningConfig &config,
            const std::shared_ptr<httplib::Client> httpClient);

        void start();

        /* Safe to call from a signal handler thread: it only hands the work
           over to the dispatcher, which is the only thread allowed to touch
           the miner and the event queue. */
        void requestShutdown();

      private:
        System::Dispatcher &m_dispatcher;

        System::ContextGroup m_contextGroup;

        CryptoNote::MiningConfig m_config;

        CryptoNote::Miner m_miner;

        BlockchainMonitor m_blockchainMonitor;

        System::Event m_eventOccurred;

        std::queue<MinerEvent> m_events;

        /* Written by the mining thread, read by the hash rate reporter. */
        std::atomic<bool> isRunning {false};

        /* Set from whichever thread the signal arrived on, so the retry loops
           can give up instead of holding a Ctrl+C until the daemon answers. */
        std::atomic<bool> m_shutdownRequested {false};

        CryptoNote::BlockTemplate m_minedBlock;

        uint64_t m_lastBlockTimestamp;

        std::shared_ptr<httplib::Client> m_httpClient = nullptr;

        void eventLoop();

        MinerEvent waitEvent();

        void pushEvent(MinerEvent &&event);

        void printHashRate();

        void printStartupSummary() const;

        /* Sleeps on the dispatcher rather than on the thread, so the fibers
           behind the miner and the blockchain monitor keep running while a
           retry or a poll is waiting. */
        void sleepSeconds(const size_t seconds);

        void startMining(const CryptoNote::BlockMiningParameters &params);

        void stopMining();

        void startBlockchainMonitoring();

        void stopBlockchainMonitoring();

        bool submitBlock(const CryptoNote::BlockTemplate &minedBlock);

        /* Empty when a shutdown was asked for while it was retrying. */
        std::optional<CryptoNote::BlockMiningParameters> requestMiningParameters();

        void adjustBlockTemplate(CryptoNote::BlockTemplate &blockTemplate) const;
    };

} // namespace Miner
