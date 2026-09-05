// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

//////////////////
#include "Miner.h"
//////////////////

#include <chrono>
#include <common/CheckDifficulty.h>
#include <common/StringTools.h>
#include <crypto/crypto.h>
#include <crypto/random.h>
#include <iostream>
#include <miner/BlockUtilities.h>
#include <system/InterruptedException.h>
#include <utilities/ColouredMsg.h>

namespace CryptoNote
{
    namespace
    {
        /* Every worker incrementing one shared atomic on every hash turns the
           counter into a cache line the whole rig fights over, so each worker
           counts locally and only publishes in batches. The reporter reads the
           total once a minute, so being this far behind never shows. */
        const uint64_t HASH_COUNT_FLUSH_INTERVAL = 64;

        uint64_t steadyNanoseconds()
        {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();

            const uint64_t nanoseconds =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());

            /* 0 is the "no job running" marker, so never hand it back as a
               timestamp - one nanosecond of error is not worth a second atomic
               to disambiguate it. */
            return nanoseconds == 0 ? 1 : nanoseconds;
        }
    } // namespace

    Miner::Miner(System::Dispatcher &dispatcher):
        m_dispatcher(dispatcher),
        m_miningStopped(dispatcher),
        m_state(MiningState::MINING_STOPPED)
    {
    }

    BlockTemplate Miner::mine(const BlockMiningParameters &blockMiningParameters, size_t threadCount)
    {
        if (threadCount == 0)
        {
            throw std::runtime_error("Miner requires at least one thread");
        }

        if (m_state == MiningState::MINING_IN_PROGRESS)
        {
            throw std::runtime_error("Mining is already in progress");
        }

        m_state = MiningState::MINING_IN_PROGRESS;
        m_miningStopped.clear();

        runWorkers(blockMiningParameters, threadCount);

        if (m_state == MiningState::MINING_STOPPED)
        {
            throw System::InterruptedException();
        }

        return m_block;
    }

    void Miner::stop()
    {
        MiningState state = MiningState::MINING_IN_PROGRESS;

        /* Strong, because there is no retry loop around this one: a spurious
           failure of the weak form would leave the workers running and still
           return as though they had been stopped. */
        if (m_state.compare_exchange_strong(state, MiningState::MINING_STOPPED))
        {
            m_miningStopped.wait();
            m_miningStopped.clear();
        }
    }

    void Miner::runWorkers(BlockMiningParameters blockMiningParameters, size_t threadCount)
    {
        std::cout << InformationMsg("Started mining block ") << InformationMsg(blockMiningParameters.height)
                  << InformationMsg(" at a difficulty of ") << InformationMsg(blockMiningParameters.difficulty)
                  << InformationMsg(". Good luck! ;)\n");

        m_jobStartedAt = steadyNanoseconds();

        try
        {
            blockMiningParameters.blockTemplate.nonce = Random::randomValue<uint32_t>();

            for (size_t i = 0; i < threadCount; ++i)
            {
                m_workers.emplace_back(std::unique_ptr<System::RemoteContext<void>>(new System::RemoteContext<void>(
                    m_dispatcher,
                    std::bind(
                        &Miner::workerFunc,
                        this,
                        blockMiningParameters.blockTemplate,
                        blockMiningParameters.difficulty,
                        static_cast<uint32_t>(threadCount)))));

                blockMiningParameters.blockTemplate.nonce++;
            }
        }
        catch (const std::exception &e)
        {
            std::cout << WarningMsg("Error occured whilst mining: ") << WarningMsg(e.what()) << std::endl;

            m_state = MiningState::MINING_STOPPED;
        }

        /* Destroying the contexts is what waits for the worker threads, so this
           has to happen on the error path too - otherwise the workers of a
           failed job are still in the vector when the next one starts. */
        m_workers.clear();

        const uint64_t startedAt = m_jobStartedAt.exchange(0);

        if (startedAt != 0)
        {
            const uint64_t finishedAt = steadyNanoseconds();

            if (finishedAt > startedAt)
            {
                m_activeNanoseconds += finishedAt - startedAt;
            }
        }

        m_miningStopped.set();
    }

    void Miner::workerFunc(const BlockTemplate &blockTemplate, uint64_t difficulty, uint32_t nonceStep)
    {
        uint64_t pendingHashes = 0;

        try
        {
            BlockTemplate block = blockTemplate;

            /* Serialized once for the whole job. Only the four nonce bytes
               change between attempts, so re-serializing the block per hash
               was paying for a keccak of the base transaction, a merkle
               branch hash and a handful of allocations to arrive at the same
               bytes every time. */
            PreparedBlockHashingBlob prepared = prepareBlockHashingBlob(block);

            while (m_state == MiningState::MINING_IN_PROGRESS)
            {
                const Crypto::Hash hash = hashPreparedBlob(prepared, block.nonce);

                pendingHashes++;

                if (pendingHashes == HASH_COUNT_FLUSH_INTERVAL)
                {
                    m_hash_count += pendingHashes;
                    pendingHashes = 0;
                }

                if (check_hash(hash, difficulty))
                {
                    m_hash_count += pendingHashes;
                    pendingHashes = 0;

                    if (!setStateBlockFound())
                    {
                        return;
                    }

                    m_block = block;
                    return;
                }

                block.nonce += nonceStep;
            }
        }
        catch (const std::exception &e)
        {
            std::cout << WarningMsg("Error occured whilst mining: ") << WarningMsg(e.what()) << std::endl;

            m_state = MiningState::MINING_STOPPED;
        }

        m_hash_count += pendingHashes;
    }

    bool Miner::setStateBlockFound()
    {
        auto state = m_state.load();

        while (true)
        {
            switch (state)
            {
                case MiningState::BLOCK_FOUND:
                {
                    return false;
                }
                case MiningState::MINING_IN_PROGRESS:
                {
                    if (m_state.compare_exchange_weak(state, MiningState::BLOCK_FOUND))
                    {
                        return true;
                    }

                    break;
                }
                case MiningState::MINING_STOPPED:
                {
                    return false;
                }
                default:
                {
                    return false;
                }
            }
        }
    }

    uint64_t Miner::getHashCount()
    {
        return m_hash_count.load();
    }

    uint64_t Miner::getActiveMiningNanoseconds() const
    {
        uint64_t total = m_activeNanoseconds.load();

        const uint64_t startedAt = m_jobStartedAt.load();

        if (startedAt != 0)
        {
            const uint64_t now = steadyNanoseconds();

            if (now > startedAt)
            {
                total += now - startedAt;
            }
        }

        return total;
    }

} // namespace CryptoNote
