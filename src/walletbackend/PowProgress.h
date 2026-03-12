// PowProgress.h — shared inline atomics for TX PoW progress reporting.
// Transfer.cpp updates these; wallet_capi.cpp reads them.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

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
