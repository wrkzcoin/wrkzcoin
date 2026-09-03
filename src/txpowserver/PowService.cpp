// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "PowService.h"

#include <common/CheckDifficulty.h>
#include <common/CryptoNoteTools.h>
#include <common/StringTools.h>
#include <config/Constants.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/hash.h>
#include <logger/Logger.h>
#include <serialization/SerializationTools.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <random>
#include <variant>

namespace
{
    uint64_t msBetween(
        const std::chrono::steady_clock::time_point &from,
        const std::chrono::steady_clock::time_point &to)
    {
        if (to <= from)
        {
            return 0;
        }

        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count());
    }

    /* Anything above this is not a ring any wallet builds; it is a way to
       make the server hash a huge prefix. */
    constexpr size_t MAX_RING_SIZE = 128;

    /* Fills the job's shape fields. Returns an empty string when the prefix is
       acceptable, otherwise the reason it is not. */
    std::string validatePrefix(
        const std::vector<uint8_t> &bytes,
        const std::optional<uint64_t> height,
        const uint64_t maxDifficulty,
        PowJob &job,
        bool &tooDifficult)
    {
        tooDifficult = false;

        if (bytes.size() < 1 + CryptoNote::TX_POW_NONCE_SIZE)
        {
            return "prefix is too short";
        }

        CryptoNote::TransactionPrefix prefix;

        if (!CryptoNote::fromBinaryArray(prefix, bytes))
        {
            return "prefix does not deserialize as a transaction prefix";
        }

        /* The daemon hashes the bytes it receives, which is the canonical
           serialization of the prefix. A prefix that round-trips differently
           would be a different transaction. */
        if (CryptoNote::toBinaryArray(prefix) != bytes)
        {
            return "prefix is not canonically serialized";
        }

        if (prefix.inputs.empty())
        {
            return "prefix has no inputs";
        }

        if (prefix.outputs.empty())
        {
            return "prefix has no outputs";
        }

        if (prefix.outputs.size() > CryptoNote::parameters::NORMAL_TX_MAX_OUTPUT_COUNT_V1)
        {
            return "prefix has more outputs than the network allows";
        }

        uint64_t inputSum = 0;

        for (const auto &input : prefix.inputs)
        {
            const auto *keyInput = std::get_if<CryptoNote::KeyInput>(&input);

            if (keyInput == nullptr)
            {
                return "prefix contains an input that is not a key input";
            }

            if (keyInput->outputIndexes.empty() || keyInput->outputIndexes.size() > MAX_RING_SIZE)
            {
                return "prefix contains an input with an unreasonable ring size";
            }

            if (inputSum > std::numeric_limits<uint64_t>::max() - keyInput->amount)
            {
                return "input amounts overflow";
            }

            inputSum += keyInput->amount;
        }

        uint64_t outputSum = 0;

        for (const auto &output : prefix.outputs)
        {
            if (outputSum > std::numeric_limits<uint64_t>::max() - output.amount)
            {
                return "output amounts overflow";
            }

            outputSum += output.amount;
        }

        if (outputSum > inputSum)
        {
            return "outputs exceed inputs";
        }

        /* Zero fee means fusion, which the daemon holds to a different
           difficulty than the one computed here. No current wallet asks for
           it, so refuse rather than hand back a nonce that would be rejected. */
        if (outputSum == inputSum)
        {
            return "zero-fee transactions are not served";
        }

        const auto &extra = prefix.extra;

        if (extra.size() < 1 + CryptoNote::TX_POW_NONCE_SIZE
            || extra[extra.size() - 1 - CryptoNote::TX_POW_NONCE_SIZE] != Constants::TX_EXTRA_TRANSACTION_POW_NONCE_IDENTIFIER)
        {
            return "extra must end with the PoW nonce tag followed by 8 nonce bytes";
        }

        job.inputs = prefix.inputs.size();
        job.outputs = prefix.outputs.size();
        job.difficulty = CryptoNote::transactionPoWDifficulty(
            height.value_or(std::numeric_limits<uint64_t>::max()), job.inputs, job.outputs);

        if (job.difficulty == 0)
        {
            return "no proof of work is required at that height";
        }

        if (job.difficulty > maxDifficulty)
        {
            tooDifficult = true;
            return "difficulty " + std::to_string(job.difficulty) + " is above this server's limit of "
                   + std::to_string(maxDifficulty);
        }

        return {};
    }
} // namespace

const char *powJobStateName(const PowJobState state)
{
    switch (state)
    {
        case PowJobState::Queued:
            return "queued";
        case PowJobState::Running:
            return "running";
        case PowJobState::Done:
            return "done";
        case PowJobState::Failed:
            return "failed";
        case PowJobState::Cancelled:
            return "cancelled";
    }

    return "unknown";
}

/* ------------------------------------------------------------------------ */

bool PowJob::finished() const
{
    const auto s = state.load(std::memory_order_acquire);

    return s == PowJobState::Done || s == PowJobState::Failed || s == PowJobState::Cancelled;
}

bool PowJob::waitFor(const std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_cv.wait_for(lock, timeout, [this] { return finished(); });

    return finished();
}

uint64_t PowJob::elapsedMs() const
{
    return msBetween(submittedAt, finished() ? finishedAt : std::chrono::steady_clock::now());
}

void PowJob::setState(const PowJobState newState, std::string errorMessage)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        error = std::move(errorMessage);

        if (newState == PowJobState::Done || newState == PowJobState::Failed || newState == PowJobState::Cancelled)
        {
            finishedAt = std::chrono::steady_clock::now();
        }

        state.store(newState, std::memory_order_release);
    }

    m_cv.notify_all();
}

/* ------------------------------------------------------------------------ */

PowService::PowService(const PowServiceLimits &limits): m_limits(limits)
{
    if (m_limits.threads == 0)
    {
        m_limits.threads = 1;
    }
}

PowService::~PowService()
{
    stop();
}

void PowService::start()
{
    m_startedAt = std::chrono::steady_clock::now();
    m_startedAtEpoch = std::time(nullptr);
    m_stop = false;
    m_coordinator = std::thread(&PowService::coordinatorLoop, this);
}

void PowService::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_stop.exchange(true))
        {
            return;
        }
    }

    m_cv.notify_all();

    if (m_coordinator.joinable())
    {
        m_coordinator.join();
    }

    /* Anything still queued will never run. */
    std::deque<std::shared_ptr<PowJob>> leftovers;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        leftovers.swap(m_queue);
    }

    for (const auto &job : leftovers)
    {
        job->setState(PowJobState::Failed, "server shutting down");
        m_failed++;
    }
}

const PowServiceLimits &PowService::limits() const
{
    return m_limits;
}

std::string PowService::newJobId()
{
    static std::mutex mutex;
    static std::mt19937_64 generator{std::random_device{}()};

    std::lock_guard<std::mutex> lock(mutex);

    uint64_t words[2] = {generator(), generator()};

    return Common::toHex(words, sizeof(words));
}

PowService::SubmitResult PowService::submit(
    const std::vector<uint8_t> &prefix,
    const std::optional<uint64_t> height,
    const std::function<bool()> &reserveSlot)
{
    m_received++;

    SubmitResult result;

    auto job = std::make_shared<PowJob>();

    bool tooDifficult = false;

    const std::string problem = validatePrefix(prefix, height, m_limits.maxDifficulty, *job, tooDifficult);

    if (!problem.empty())
    {
        (tooDifficult ? m_rejectedDifficulty : m_rejectedInvalid)++;

        result.httpStatus = 400;
        result.error = problem;

        return result;
    }

    if (reserveSlot && !reserveSlot())
    {
        result.httpStatus = 429;
        result.error = "the server has reached its jobs-per-minute limit, retry later";
        return result;
    }

    job->prefix = prefix;
    job->submittedAt = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_stop)
        {
            result.httpStatus = 503;
            result.error = "server is shutting down";
            return result;
        }

        if (m_queue.size() >= m_limits.maxQueue)
        {
            m_rejectedQueueFull++;

            result.httpStatus = 503;
            result.error = "queue is full, try again later";
            return result;
        }

        job->id = newJobId();

        m_queue.push_back(job);
        m_jobs.emplace(job->id, job);
    }

    m_accepted++;

    m_cv.notify_one();

    Logger::logger.log(
        "Job " + job->id + " queued: " + std::to_string(job->inputs) + " inputs, " + std::to_string(job->outputs)
            + " outputs, difficulty " + std::to_string(job->difficulty),
        Logger::DEBUG,
        {Logger::TRANSACTIONS});

    result.job = job;

    return result;
}

std::shared_ptr<PowJob> PowService::find(const std::string &id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto it = m_jobs.find(id);

    return it == m_jobs.end() ? nullptr : it->second;
}

bool PowService::cancel(const std::string &id)
{
    std::shared_ptr<PowJob> job;

    bool wasQueued = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const auto it = m_jobs.find(id);

        if (it == m_jobs.end() || it->second->finished())
        {
            return false;
        }

        job = it->second;

        job->cancelRequested = true;

        const auto queued = std::find(m_queue.begin(), m_queue.end(), job);

        if (queued != m_queue.end())
        {
            m_queue.erase(queued);
            wasQueued = true;
        }
    }

    /* A running job is stopped by its workers noticing the flag; a queued
       one can be finished right here so its slot frees immediately. */
    if (wasQueued)
    {
        job->setState(PowJobState::Cancelled, "cancelled before it started");
        m_cancelled++;
    }

    return true;
}

size_t PowService::queued() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_queue.size();
}

void PowService::coordinatorLoop()
{
    while (true)
    {
        std::shared_ptr<PowJob> job;

        bool expired = false;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_cv.wait(lock, [this] { return m_stop.load() || !m_queue.empty(); });

            if (m_stop)
            {
                return;
            }

            job = m_queue.front();
            m_queue.pop_front();

            expired = msBetween(job->submittedAt, std::chrono::steady_clock::now())
                      > static_cast<uint64_t>(m_limits.jobTimeoutSeconds) * 1000;

            if (!expired)
            {
                job->startedAt = std::chrono::steady_clock::now();
                job->state.store(PowJobState::Running, std::memory_order_release);
                m_active = job;
            }
        }

        if (expired)
        {
            job->setState(PowJobState::Failed, "expired before a worker was free");
            m_expired++;
        }
        else
        {
            solve(job);

            std::lock_guard<std::mutex> lock(m_mutex);
            m_active.reset();
        }

        housekeeping();
    }
}

void PowService::solve(const std::shared_ptr<PowJob> &job)
{
    std::atomic<bool> found{false};

    const unsigned int threadCount = m_limits.threads;

    auto worker = [&, this](const uint64_t startNonce) {
        std::vector<uint8_t> prefix = job->prefix;

        uint8_t *const noncePosition = prefix.data() + prefix.size() - CryptoNote::TX_POW_NONCE_SIZE;

        uint64_t nonce = startNonce;

        uint64_t sinceReport = 0;

        while (!found.load(std::memory_order_relaxed) && !job->cancelRequested.load(std::memory_order_relaxed)
               && !m_stop.load(std::memory_order_relaxed))
        {
            std::memcpy(noncePosition, &nonce, sizeof(nonce));

            Crypto::Hash hash;
            Crypto::cn_upx(prefix.data(), prefix.size(), hash);

            if (CryptoNote::check_hash(hash, job->difficulty))
            {
                if (!found.exchange(true))
                {
                    std::memcpy(job->nonce.data(), noncePosition, CryptoNote::TX_POW_NONCE_SIZE);
                }

                job->hashes.fetch_add(sinceReport + 1, std::memory_order_relaxed);

                return;
            }

            nonce += threadCount;

            if (++sinceReport == 256)
            {
                job->hashes.fetch_add(256, std::memory_order_relaxed);
                sinceReport = 0;
            }
        }

        job->hashes.fetch_add(sinceReport, std::memory_order_relaxed);
    };

    std::vector<std::thread> threads;
    threads.reserve(threadCount);

    for (unsigned int i = 0; i < threadCount; i++)
    {
        threads.emplace_back(worker, static_cast<uint64_t>(i));
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    const uint64_t solveMs = msBetween(job->startedAt, std::chrono::steady_clock::now());

    m_hashes += job->hashes.load();
    m_busyMs += solveMs;

    if (found)
    {
        job->setState(PowJobState::Done);

        m_completed++;
        m_solveMsTotal += solveMs;
        m_queueWaitMsTotal += msBetween(job->submittedAt, job->startedAt);

        uint64_t previousMax = m_solveMsMax.load();

        while (solveMs > previousMax && !m_solveMsMax.compare_exchange_weak(previousMax, solveMs))
        {
        }

        Logger::logger.log(
            "Job " + job->id + " solved in " + std::to_string(solveMs) + " ms after "
                + std::to_string(job->hashes.load()) + " hashes",
            Logger::INFO,
            {Logger::TRANSACTIONS});
    }
    else if (job->cancelRequested)
    {
        job->setState(PowJobState::Cancelled, "cancelled by the client");
        m_cancelled++;

        Logger::logger.log("Job " + job->id + " cancelled", Logger::INFO, {Logger::TRANSACTIONS});
    }
    else
    {
        job->setState(PowJobState::Failed, "server shutting down");
        m_failed++;
    }
}

void PowService::housekeeping()
{
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_jobs.begin(); it != m_jobs.end();)
    {
        const auto &job = it->second;

        if (job->finished() && msBetween(job->finishedAt, now) > static_cast<uint64_t>(m_limits.resultTtlSeconds) * 1000)
        {
            it = m_jobs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

nlohmann::json PowService::statsJson() const
{
    const auto now = std::chrono::steady_clock::now();

    nlohmann::json active = nullptr;

    size_t queued = 0;

    size_t tracked = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        queued = m_queue.size();
        tracked = m_jobs.size();

        if (m_active)
        {
            /* No job id here: /stats is public and the id is what DELETE
               /pow/<id> takes, so listing it would let anyone cancel the
               job that is running. */
            active = {
                {"difficulty", m_active->difficulty},
                {"inputs", m_active->inputs},
                {"outputs", m_active->outputs},
                {"hashes", m_active->hashes.load()},
                {"running_ms", msBetween(m_active->startedAt, now)},
            };
        }
    }

    const uint64_t completed = m_completed.load();
    const uint64_t busyMs = m_busyMs.load();
    const uint64_t hashes = m_hashes.load();

    return {
        {"started_at", static_cast<int64_t>(m_startedAtEpoch)},
        {"uptime_seconds", msBetween(m_startedAt, now) / 1000},
        {"threads", m_limits.threads},
        {"queue",
         {
             {"waiting", queued},
             {"capacity", m_limits.maxQueue},
             {"tracked_jobs", tracked},
         }},
        {"active", active},
        {"jobs",
         {
             {"received", m_received.load()},
             {"accepted", m_accepted.load()},
             {"completed", completed},
             {"failed", m_failed.load()},
             {"cancelled", m_cancelled.load()},
             {"expired", m_expired.load()},
             {"rejected_invalid", m_rejectedInvalid.load()},
             {"rejected_difficulty", m_rejectedDifficulty.load()},
             {"rejected_queue_full", m_rejectedQueueFull.load()},
             {"rejected_rate_limited", rateLimited.load()},
             {"rejected_global_limit", globalLimited.load()},
         }},
        {"work",
         {
             {"hashes", hashes},
             {"busy_ms", busyMs},
             {"hashrate", busyMs == 0 ? 0.0 : static_cast<double>(hashes) * 1000.0 / static_cast<double>(busyMs)},
             {"solve_ms_avg", completed == 0 ? 0 : m_solveMsTotal.load() / completed},
             {"solve_ms_max", m_solveMsMax.load()},
             {"queue_wait_ms_avg", completed == 0 ? 0 : m_queueWaitMsTotal.load() / completed},
         }},
        {"http",
         {
             {"requests", requests.load()},
             {"unauthorized", unauthorized.load()},
         }},
    };
}
