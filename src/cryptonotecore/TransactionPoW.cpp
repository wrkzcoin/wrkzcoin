// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TransactionPoW.h"

#include <common/CheckDifficulty.h>
#include <common/CryptoNoteTools.h>
#include <config/Constants.h>
#include <crypto/hash.h>
#include <logger/Logger.h>
#include <serialization/SerializationTools.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <vector>

namespace CryptoNote
{
    namespace
    {
        bool nonceSatisfies(
            std::vector<uint8_t> prefix,
            const std::array<uint8_t, TX_POW_NONCE_SIZE> &nonce,
            const uint64_t difficulty)
        {
            std::memcpy(prefix.data() + prefix.size() - TX_POW_NONCE_SIZE, nonce.data(), TX_POW_NONCE_SIZE);

            Crypto::Hash hash;
            Crypto::cn_upx(prefix.data(), prefix.size(), hash);

            return CryptoNote::check_hash(hash, difficulty);
        }

        /* Searches the nonce residue class `startNonce` mod `threadCount`.
           The prefix is serialized once by the caller; each iteration only
           rewrites the trailing nonce bytes, which is byte for byte what
           re-serializing the transaction with that nonce would produce. */
        void generateTransactionPowWorker(
            const std::vector<uint8_t> &basePrefix,
            const uint64_t difficulty,
            const int threadCount,
            uint64_t nonce,
            std::atomic<bool> &shouldStop,
            std::array<uint8_t, TX_POW_NONCE_SIZE> &result)
        {
            std::vector<uint8_t> prefix = basePrefix;

            uint8_t *const noncePosition = prefix.data() + prefix.size() - TX_POW_NONCE_SIZE;

            uint64_t sinceReport = 0;

            while (!shouldStop.load(std::memory_order_relaxed))
            {
                std::memcpy(noncePosition, &nonce, sizeof(nonce));

                Crypto::Hash hash;
                Crypto::cn_upx(prefix.data(), prefix.size(), hash);

                if (CryptoNote::check_hash(hash, difficulty))
                {
                    /* First thread to find a nonce wins; the others stop. */
                    if (!shouldStop.exchange(true))
                    {
                        std::memcpy(result.data(), noncePosition, TX_POW_NONCE_SIZE);
                    }

                    return;
                }

                nonce += threadCount;

                /* Report progress every 256 hashes to avoid atomic contention */
                if (++sinceReport == 256)
                {
                    PowProgress::nonces.fetch_add(256, std::memory_order_relaxed);
                    sinceReport = 0;
                }
            }
        }
    } // namespace

    std::vector<uint8_t> generateTransactionPoWHeight(
        CryptoNote::Transaction tx,
        std::vector<uint8_t> extra,
        const uint64_t height,
        const RemotePoWSolver &remote)
    {
        PowProgress::Guard powGuard; // sets active=true, resets on return

        /* Add the nonce identifier */
        extra.push_back(Constants::TX_EXTRA_TRANSACTION_POW_NONCE_IDENTIFIER);

        /* Add extra room for the nonce */
        extra.resize(extra.size() + TX_POW_NONCE_SIZE);

        tx.extra = extra;

        const uint64_t difficulty = transactionPoWDifficulty(height, tx.inputs.size(), tx.outputs.size());

        const std::vector<uint8_t> prefix = toBinaryArray(static_cast<CryptoNote::TransactionPrefix>(tx));

        std::array<uint8_t, TX_POW_NONCE_SIZE> nonce{};

        bool solved = false;

        if (remote)
        {
            try
            {
                if (remote(prefix, difficulty, nonce))
                {
                    solved = nonceSatisfies(prefix, nonce, difficulty);

                    if (!solved)
                    {
                        Logger::logger.log(
                            "Tx PoW server returned a nonce that does not meet difficulty "
                                + std::to_string(difficulty) + ", computing locally instead",
                            Logger::WARNING,
                            {Logger::TRANSACTIONS});
                    }
                }
            }
            catch (const std::exception &e)
            {
                Logger::logger.log(
                    std::string("Tx PoW server request failed: ") + e.what() + ", computing locally instead",
                    Logger::WARNING,
                    {Logger::TRANSACTIONS});
            }
        }

        if (!solved)
        {
            std::atomic<bool> shouldStop = false;

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
            /* WASM single-threaded mode: std::thread is unavailable.
               Run the PoW worker inline on the calling thread (threadCount=1, nonce=0). */
            generateTransactionPowWorker(prefix, difficulty, 1, 0, shouldStop, nonce);
#else
            std::vector<std::thread> threads;

            const int threadCount = std::max(1u, std::thread::hardware_concurrency());

            for (int i = 0; i < threadCount; i++)
            {
                threads.push_back(std::thread(
                    generateTransactionPowWorker,
                    std::cref(prefix),
                    difficulty,
                    threadCount,
                    static_cast<uint64_t>(i),
                    std::ref(shouldStop),
                    std::ref(nonce)));
            }

            for (auto &thread : threads)
            {
                thread.join();
            }
#endif
        }

        Logger::logger.log(
            std::string("Making Tx PoW with difficulty ") + std::to_string(difficulty)
                + (solved ? " (solved by Tx PoW server)" : " (solved locally)"),
            Logger::DEBUG,
            {Logger::TRANSACTIONS});

        std::memcpy(extra.data() + extra.size() - TX_POW_NONCE_SIZE, nonce.data(), TX_POW_NONCE_SIZE);

        return extra;
    }
} // namespace CryptoNote
