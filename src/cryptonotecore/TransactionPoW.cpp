// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TransactionPoW.h"

#include <common/CheckDifficulty.h>
#include <common/CryptoNoteTools.h>
#include <config/Constants.h>
#include <config/CryptoNoteConfig.h>
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
    /* Moved verbatim from walletbackend/Transfer.cpp. */
    void generateTransactionPowWorker(
        std::vector<uint8_t> &finalExtra,
        const int threadCount,
        uint64_t nonce,
        std::atomic<bool> &shouldStop,
        CryptoNote::Transaction tx,
        const uint64_t height)
    {
        /* Make a thread local copy */
        auto extra = finalExtra;

        /* Get a pointer to the start of where we want to insert our nonce */
        const auto noncePosition = &extra[extra.size() - 8];

        while (true)
        {
            if (shouldStop)
            {
                return;
            }

            /* Copy in the nonce */
            std::memcpy(noncePosition, &nonce, sizeof(nonce));

            Crypto::Hash hash;

            tx.extra = extra;

            std::vector<uint8_t> data = toBinaryArray(static_cast<CryptoNote::TransactionPrefix>(tx));

            Crypto::cn_upx(data.data(), data.size(), hash);

            uint64_t diff = CryptoNote::parameters::TRANSACTION_POW_DIFFICULTY_DYN_V1;

            uint64_t txInputSize = 0;
            try
            {
                txInputSize = tx.inputs.size();
            }
            catch (const std::exception &e)
            {
            }

            uint64_t txOutputSize = 0;
            try
            {
                txOutputSize = tx.outputs.size();
            }
            catch (const std::exception &e)
            {
            }

            if (height >= CryptoNote::parameters::TRANSACTION_POW_HEIGHT &&
            height < CryptoNote::parameters::TRANSACTION_POW_HEIGHT_DYN_V1)
            {
                diff = CryptoNote::parameters::TRANSACTION_POW_DIFFICULTY;
            } else if (height >= CryptoNote::parameters::TRANSACTION_POW_HEIGHT_DYN_V1)
            {
                diff = CryptoNote::parameters::TRANSACTION_POW_DIFFICULTY_DYN_V1
                + (txInputSize + txOutputSize * CryptoNote::parameters::MULTIPLIER_TRANSACTION_POW_DIFFICULTY_FACTORED_OUT_V1)
                * CryptoNote::parameters::MULTIPLIER_TRANSACTION_POW_DIFFICULTY_PER_IO_V1;
            }

            if (CryptoNote::check_hash(hash, diff))
            {
                Logger::logger.log(
                    "Making Tx PoW with difficulty " + std::to_string(diff),
                    Logger::DEBUG,
                    { Logger::TRANSACTIONS }
                );
                finalExtra = extra;
                shouldStop = true;

                return;
            }

            nonce += threadCount;

            /* Report progress every 256 nonces to avoid atomic contention */
            if ((nonce & 0xFF) == 0)
            {
                PowProgress::nonces.fetch_add(256, std::memory_order_relaxed);
            }
        }
    }
    } // namespace

    std::vector<uint8_t> generateTransactionPoWHeight(
        CryptoNote::Transaction tx,
        std::vector<uint8_t> extra,
        const uint64_t height)
    {
        PowProgress::Guard powGuard; // sets active=true, resets on return

        /* Add the nonce identifier */
        extra.push_back(Constants::TX_EXTRA_TRANSACTION_POW_NONCE_IDENTIFIER);

        /* Add extra room for the nonce */
        extra.resize(extra.size() + 8);

        std::atomic<bool> shouldStop = false;

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        /* WASM single-threaded mode: std::thread is unavailable.
           Run the PoW worker inline on the calling thread (threadCount=1, nonce=0). */
        generateTransactionPowWorker(extra, 1, 0, shouldStop, tx, height);
#else
        std::vector<std::thread> threads;

        const int threadCount = std::max(1u, std::thread::hardware_concurrency());

        for (int i = 0; i < threadCount; i++)
        {
            threads.push_back(std::thread(
                generateTransactionPowWorker,
                std::ref(extra),
                threadCount,
                i,
                std::ref(shouldStop),
                tx,
                height
            ));
        }

        for (auto &thread : threads)
        {
            thread.join();
        }
#endif

        return extra;
    }
} // namespace CryptoNote
