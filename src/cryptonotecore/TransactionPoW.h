// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

/* Shared progress counters for the transaction proof-of-work search.
   generateTransactionPoWHeight() updates them; wallet front-ends (wallet_capi)
   read them to report progress. */
namespace PowProgress
{
    /// True while generateTransactionPoWHeight is running.
    inline std::atomic<bool> active{false};

    /// Millisecond timestamp (epoch) when the current PoW started.
    inline std::atomic<uint64_t> startMs{0};

    /// Cumulative nonces tried across all threads (updated periodically).
    inline std::atomic<uint64_t> nonces{0};

    /// RAII helper — sets active=true on construction, false on destruction.
    struct Guard
    {
        Guard()
        {
            nonces.store(0, std::memory_order_relaxed);
            startMs.store(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count()),
                std::memory_order_relaxed);
            active.store(true, std::memory_order_release);
        }

        ~Guard()
        {
            active.store(false, std::memory_order_release);
        }

        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;
    };
} // namespace PowProgress

namespace CryptoNote
{
    /* Appends the tx PoW nonce field to `extra` and searches, on all hardware
       threads, for a nonce satisfying the transaction PoW difficulty in force
       at `height`. Returns the completed extra. Used by the wallet backend when
       building transactions and by TransactionImpl::generateTxProofOfWork. */
    std::vector<uint8_t> generateTransactionPoWHeight(
        CryptoNote::Transaction tx,
        std::vector<uint8_t> extra,
        const uint64_t height);
} // namespace CryptoNote
