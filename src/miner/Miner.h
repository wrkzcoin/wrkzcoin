// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNote.h"

#include <atomic>
#include <system/Dispatcher.h>
#include <system/Event.h>
#include <system/RemoteContext.h>
#include <thread>

namespace CryptoNote
{
    struct BlockMiningParameters
    {
        BlockTemplate blockTemplate;
        uint64_t difficulty;
        uint32_t height;
    };

    class Miner
    {
      public:
        Miner(System::Dispatcher &dispatcher);

        BlockTemplate mine(const BlockMiningParameters &blockMiningParameters, size_t threadCount);

        uint64_t getHashCount();

        /* Nanoseconds the workers have actually been running for, so a hash
           rate can be quoted against the time spent hashing rather than
           against wall clock that also covers fetching templates and waiting
           on an unreachable daemon. */
        uint64_t getActiveMiningNanoseconds() const;

        // NOTE! this is blocking method
        void stop();

      private:
        System::Dispatcher &m_dispatcher;

        System::Event m_miningStopped;

        enum class MiningState : uint8_t
        {
            MINING_STOPPED,
            BLOCK_FOUND,
            MINING_IN_PROGRESS
        };

        std::atomic<MiningState> m_state;

        std::vector<std::unique_ptr<System::RemoteContext<void>>> m_workers;

        BlockTemplate m_block;

        std::atomic<uint64_t> m_hash_count = 0;

        /* Nanoseconds of finished jobs, plus the start of the job running now
           (0 when none is), rather than one timestamp pair guarded by a lock. */
        std::atomic<uint64_t> m_activeNanoseconds {0};

        std::atomic<uint64_t> m_jobStartedAt {0};

        void runWorkers(BlockMiningParameters blockMiningParameters, size_t threadCount);

        void workerFunc(const BlockTemplate &blockTemplate, uint64_t difficulty, uint32_t nonceStep);

        bool setStateBlockFound();
    };

} // namespace CryptoNote
