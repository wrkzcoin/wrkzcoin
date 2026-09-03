// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "json.hpp"

#include <cryptonotecore/TransactionPoW.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

enum class PowJobState
{
    Queued,
    Running,
    Done,
    Failed,
    Cancelled,
};

const char *powJobStateName(PowJobState state);

struct PowJob
{
    std::string id;

    /* Serialized transaction prefix, nonce field included. The nonce is the
       trailing 8 bytes; see CryptoNote::TX_POW_NONCE_SIZE. */
    std::vector<uint8_t> prefix;

    uint64_t difficulty = 0;

    size_t inputs = 0;

    size_t outputs = 0;

    std::chrono::steady_clock::time_point submittedAt;

    std::chrono::steady_clock::time_point startedAt;

    std::chrono::steady_clock::time_point finishedAt;

    std::atomic<PowJobState> state{PowJobState::Queued};

    std::atomic<bool> cancelRequested{false};

    std::atomic<uint64_t> hashes{0};

    std::array<uint8_t, CryptoNote::TX_POW_NONCE_SIZE> nonce{};

    std::string error;

    bool finished() const;

    /* Blocks until the job reaches a terminal state or `timeout` elapses.
       Returns finished(). */
    bool waitFor(std::chrono::milliseconds timeout);

    /* Milliseconds from submission to now, or to completion once finished. */
    uint64_t elapsedMs() const;

  private:
    friend class PowService;

    void setState(PowJobState newState, std::string errorMessage = {});

    std::mutex m_mutex;

    std::condition_variable m_cv;
};

struct PowServiceLimits
{
    unsigned int threads = 1;

    uint32_t maxQueue = 64;

    uint64_t maxDifficulty = 1'000'000;

    uint32_t jobTimeoutSeconds = 600;

    uint32_t resultTtlSeconds = 300;
};

/* Owns the job queue and the hashing threads. One job is solved at a time,
   all threads on it, so a single wallet gets its nonce as fast as this
   machine can produce one; the queue keeps everyone else in order. */
class PowService
{
  public:
    explicit PowService(const PowServiceLimits &limits);

    ~PowService();

    void start();

    void stop();

    struct SubmitResult
    {
        std::shared_ptr<PowJob> job;

        /* 200 when `job` is set, otherwise the HTTP status to answer with. */
        int httpStatus = 200;

        std::string error;
    };

    /* Validates the prefix, computes its difficulty and queues it.
       `reserveSlot` is consulted after validation and before queueing, so only
       jobs that would actually run count against the caller's jobs-per-minute
       budget; when it returns false the job is refused with 429. */
    SubmitResult submit(
        const std::vector<uint8_t> &prefix,
        std::optional<uint64_t> height,
        const std::function<bool()> &reserveSlot = {});

    std::shared_ptr<PowJob> find(const std::string &id) const;

    /* True when the job existed and was still cancellable. */
    bool cancel(const std::string &id);

    size_t queued() const;

    const PowServiceLimits &limits() const;

    nlohmann::json statsJson() const;

    /* Counters the HTTP layer keeps, so /stats has everything in one place. */
    std::atomic<uint64_t> requests{0};

    std::atomic<uint64_t> rateLimited{0};

    std::atomic<uint64_t> globalLimited{0};

    std::atomic<uint64_t> unauthorized{0};

  private:
    void coordinatorLoop();

    void solve(const std::shared_ptr<PowJob> &job);

    void housekeeping();

    static std::string newJobId();

    PowServiceLimits m_limits;

    std::atomic<bool> m_stop{false};

    std::thread m_coordinator;

    mutable std::mutex m_mutex;

    std::condition_variable m_cv;

    std::deque<std::shared_ptr<PowJob>> m_queue;

    std::unordered_map<std::string, std::shared_ptr<PowJob>> m_jobs;

    std::shared_ptr<PowJob> m_active;

    /* Statistics since start-up. */
    std::chrono::steady_clock::time_point m_startedAt;

    std::time_t m_startedAtEpoch = 0;

    std::atomic<uint64_t> m_received{0};

    std::atomic<uint64_t> m_accepted{0};

    std::atomic<uint64_t> m_rejectedInvalid{0};

    std::atomic<uint64_t> m_rejectedDifficulty{0};

    std::atomic<uint64_t> m_rejectedQueueFull{0};

    std::atomic<uint64_t> m_completed{0};

    std::atomic<uint64_t> m_failed{0};

    std::atomic<uint64_t> m_cancelled{0};

    std::atomic<uint64_t> m_expired{0};

    std::atomic<uint64_t> m_hashes{0};

    std::atomic<uint64_t> m_busyMs{0};

    std::atomic<uint64_t> m_solveMsTotal{0};

    std::atomic<uint64_t> m_solveMsMax{0};

    std::atomic<uint64_t> m_queueWaitMsTotal{0};
};
