// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

///////////////////////////
#include "Benchmark.h"
///////////////////////////

#include <CryptoTypes.h>
#include <atomic>
#include <chrono>
#include <config/CryptoNoteConfig.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <vector>

namespace Miner
{
    namespace
    {
        /* Same batching as the mining workers, for the same reason: one shared
           atomic touched by every thread on every hash would be measuring the
           cache line rather than the hash. */
        const uint64_t HASH_COUNT_FLUSH_INTERVAL = 64;

        /* A parent block hashing blob is 76 bytes, so the benchmark feeds the
           hash the same shape of input a real job would. */
        const size_t HASHING_BLOB_SIZE = 76;

        /* Where the nonce sits in that blob: two version bytes, a varint
           timestamp and the 32 byte previous block hash ahead of it. Only the
           benchmark cares, since nothing verifies these hashes - it just has
           to move, so the input is not constant. */
        const size_t NONCE_OFFSET = 39;

        uint8_t latestBlockMajorVersion()
        {
            uint8_t latest = 0;

            for (const auto &algorithm : CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION)
            {
                if (algorithm.first > latest)
                {
                    latest = algorithm.first;
                }
            }

            return latest;
        }
    } // namespace

    void runBenchmark(const size_t seconds, const size_t threadCount)
    {
        const uint8_t blockVersion = latestBlockMajorVersion();

        const auto entry = CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.find(blockVersion);

        if (entry == CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.end())
        {
            std::cout << WarningMsg("No hashing algorithm is configured to benchmark.\n");
            return;
        }

        const auto algorithm = entry->second;

        /* Widened, or it streams as the unprintable character with that code
           rather than as the version number. */
        std::cout << InformationMsg("Benchmarking the block version ")
                  << InformationMsg(static_cast<uint32_t>(blockVersion))
                  << InformationMsg(" proof of work on ") << InformationMsg(threadCount)
                  << InformationMsg(threadCount == 1 ? " thread" : " threads") << InformationMsg(" for ")
                  << InformationMsg(seconds) << InformationMsg(seconds == 1 ? " second" : " seconds")
                  << InformationMsg("...\n\n");

        std::atomic<uint64_t> totalHashes {0};

        std::atomic<bool> running {true};

        std::vector<std::thread> workers;

        const auto started = std::chrono::steady_clock::now();

        for (size_t i = 0; i < threadCount; i++)
        {
            workers.emplace_back([&algorithm, &totalHashes, &running, i]() {
                /* Seeded per thread so no two are hashing identical input,
                   in case anything downstream ever caches on it. */
                std::vector<uint8_t> blob(HASHING_BLOB_SIZE, static_cast<uint8_t>(i + 1));

                uint32_t nonce = 0;

                uint64_t pendingHashes = 0;

                Crypto::Hash hash;

                while (running)
                {
                    std::memcpy(blob.data() + NONCE_OFFSET, &nonce, sizeof(nonce));

                    algorithm(blob.data(), blob.size(), hash);

                    nonce++;

                    if (++pendingHashes == HASH_COUNT_FLUSH_INTERVAL)
                    {
                        totalHashes += pendingHashes;
                        pendingHashes = 0;
                    }
                }

                totalHashes += pendingHashes;
            });
        }

        /* Woken every second so the run can be watched rather than just
           waited out, and so a long benchmark still prints something. */
        for (size_t elapsed = 1; elapsed <= seconds; elapsed++)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            const double soFar = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

            if (soFar > 0)
            {
                std::cout << InformationMsg("  ") << InformationMsg(elapsed) << InformationMsg("s: ")
                          << InformationMsg(Utilities::get_mining_speed(totalHashes.load() / soFar)) << "\n";
            }
        }

        running = false;

        for (auto &worker : workers)
        {
            worker.join();
        }

        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

        const uint64_t hashes = totalHashes.load();

        if (elapsed <= 0)
        {
            std::cout << WarningMsg("\nThe benchmark did not run for long enough to measure.\n");
            return;
        }

        const double hashRate = hashes / elapsed;

        std::cout << SuccessMsg("\n") << SuccessMsg(hashes) << SuccessMsg(" hashes in ") << SuccessMsg(elapsed)
                  << SuccessMsg("s: ") << SuccessMsg(Utilities::get_mining_speed(hashRate))
                  << SuccessMsg(" total, ") << SuccessMsg(Utilities::get_mining_speed(hashRate / threadCount))
                  << SuccessMsg(" per thread.\n");
    }

} // namespace Miner
