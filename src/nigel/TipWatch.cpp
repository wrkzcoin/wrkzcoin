// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <nigel/TipWatch.h>
//////////////////////////

#if !defined(__EMSCRIPTEN__)

#include "httplib.h"
#include "json.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <logger/Logger.h>
#include <mutex>
#include <optional>
#include <thread>
#include <utilities/Utilities.h>

namespace
{
    /* No frame for this long - and the daemon sends a heartbeat every 30
       seconds - means the stream is gone even if the socket has not noticed,
       and the wallet should go back to its ordinary polling. Also the read
       timeout, so a half open connection is given up on at the same point. */
    constexpr auto LIVE_WINDOW = std::chrono::seconds(75);

    constexpr time_t READ_TIMEOUT_SECONDS = 75;

    constexpr time_t WRITE_TIMEOUT_SECONDS = 10;

    /* A daemon that does not serve /ws answers the upgrade with a 404, and
       httplib reports that exactly as it reports a refused connection. After
       this many failures in a row from a daemon that has never delivered a
       frame, take it to be one of those and stop asking so often. */
    constexpr unsigned UNSUPPORTED_AFTER_FAILURES = 3;

    constexpr auto UNSUPPORTED_RETRY = std::chrono::seconds(600);

    constexpr auto RECONNECT_MIN = std::chrono::seconds(1);

    constexpr auto RECONNECT_MAX = std::chrono::seconds(60);

    /* How long the destructor waits for an idle thread to wind down. */
    constexpr auto STOP_WAIT = std::chrono::seconds(2);

    int64_t nowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    void logDebug(const std::string &message)
    {
        Logger::logger.log(message, Logger::DEBUG, {Logger::SYNC, Logger::DAEMON});
    }

    /* Same shape as Nigel's daemonBaseUrl(): the host may carry a base path
       ("example.com/daemon") for a daemon behind a reverse proxy, and a bare
       IPv6 address needs its brackets. The port is always spelled out, which
       the URL parser is happy with whatever the scheme. */
    std::string eventStreamUrl(const std::string &host, const uint16_t port, const bool ssl)
    {
        std::string hostname = host;
        std::string basePath;

        const auto slashPos = host.find('/');

        if (slashPos != std::string::npos)
        {
            hostname = host.substr(0, slashPos);
            basePath = host.substr(slashPos);
        }

        while (!basePath.empty() && basePath.back() == '/')
        {
            basePath.pop_back();
        }

        const bool needsIpv6Brackets =
            hostname.find(':') != std::string::npos && (hostname.empty() || hostname.front() != '[');

        const std::string formattedHost = needsIpv6Brackets ? ("[" + hostname + "]") : hostname;

        return std::string(ssl ? "wss://" : "ws://") + formattedHost + ":" + std::to_string(port) + basePath + "/ws";
    }

    enum class Outcome
    {
        /* The URL cannot be followed at all; retrying will not change that. */
        Invalid,
        /* No stream: refused, unreachable, or the daemon answered with
           anything but a 101. */
        ConnectFailed,
        /* Upgraded, then dropped before a single frame came through. */
        Dropped,
        /* Delivered at least one frame before it ended. */
        Delivered,
    };
} // namespace

struct TipWatchState
{
    std::mutex mutex;

    std::condition_variable cv;

    /* The members down to lastFrameMs are written under mutex. */
    std::atomic<bool> stop = false;

    bool haveTarget = false;

    std::string host;

    uint16_t port = 0;

    bool ssl = false;

    /* The thread is inside a connect or a read, where nothing can wake it. */
    bool inIo = false;

    bool threadExited = false;

    /* Bumped whenever the target changes, under mutex and callbackMutex both,
       so a connection can tell it has been superseded and a callback cannot
       slip in for a daemon that has already been left. */
    std::atomic<uint64_t> generation = 0;

    /* Steady clock milliseconds of the last frame from the current target;
       zero when there is no live stream. */
    std::atomic<int64_t> lastFrameMs = 0;

    std::mutex callbackMutex;

    /* Guarded by callbackMutex. */
    std::function<void(uint64_t)> onBlock;

    std::atomic<uint64_t> events = 0;

    std::atomic<uint64_t> blockEvents = 0;

    /* Only the owning TipWatch touches this. */
    std::thread thread;

    /* Only the watch thread touches this. The stream going live is worth one
       line at INFO per daemon, not one per reconnect. */
    std::string loggedLiveUrl;
};

namespace
{
    void handleMessage(TipWatchState &state, const std::string &message, const uint64_t generation)
    {
        const auto j = nlohmann::json::parse(message, nullptr, false);

        if (j.is_discarded() || !j.is_object())
        {
            return;
        }

        try
        {
            const auto topicIt = j.find("topic");

            if (topicIt == j.end() || !topicIt->is_string())
            {
                return;
            }

            const std::string topic = topicIt->get<std::string>();

            /* "hello" counts as well: whatever happened while we were not
               connected is only caught up by syncing. */
            const bool isBlock =
                topic == "hello" || topic == "hashblock" || topic == "chain_main" || topic == "chainswitch";

            const bool isWake = isBlock || topic == "txpool_add" || topic == "txpool_del";

            /* heartbeat, hashblock_alt, and anything a newer daemon adds */
            if (!isWake)
            {
                return;
            }

            std::optional<uint64_t> height;

            const auto dataIt = j.find("data");

            if (isBlock && dataIt != j.end() && dataIt->is_object())
            {
                if (topic == "chainswitch")
                {
                    /* The hashes run from the common root to the new tip. */
                    const auto rootIt = dataIt->find("common_root_height");
                    const auto hashesIt = dataIt->find("hashes");

                    if (rootIt != dataIt->end() && rootIt->is_number_unsigned() && hashesIt != dataIt->end()
                        && hashesIt->is_array() && !hashesIt->empty())
                    {
                        height = rootIt->get<uint64_t>() + hashesIt->size() - 1;
                    }
                }
                else
                {
                    const auto heightIt = dataIt->find("height");

                    if (heightIt != dataIt->end() && heightIt->is_number_unsigned())
                    {
                        height = heightIt->get<uint64_t>();
                    }
                }
            }

            /* Height first, counters second: a loop woken by the count must
               already see the daemon as holding the new block, or it goes
               straight back to sleep thinking it is level with it. */
            if (height)
            {
                std::lock_guard<std::mutex> lock(state.callbackMutex);

                if (state.generation == generation && state.onBlock)
                {
                    state.onBlock(*height);
                }
            }

            if (isBlock)
            {
                state.blockEvents++;
            }

            state.events++;
        }
        catch (const nlohmann::json::exception &)
        {
        }
    }

    Outcome followOnce(
        TipWatchState &state,
        const std::string &host,
        const uint16_t port,
        const bool ssl,
        const uint64_t generation)
    {
        const std::string url = eventStreamUrl(host, port, ssl);

        std::unique_ptr<httplib::ws::WebSocketClient> client;

        try
        {
            client = std::make_unique<httplib::ws::WebSocketClient>(url);
        }
        catch (const std::exception &)
        {
            return Outcome::Invalid;
        }

        if (!client->is_valid())
        {
            return Outcome::Invalid;
        }

        /* Both only take effect for the next connect(). */
        client->set_read_timeout(READ_TIMEOUT_SECONDS);
        client->set_write_timeout(WRITE_TIMEOUT_SECONDS);

        if (!client->connect())
        {
            return Outcome::ConnectFailed;
        }

        bool delivered = false;

        std::string message;

        while (true)
        {
            const auto result = client->read(message);

            if (result == httplib::ws::Fail)
            {
                break;
            }

            {
                std::lock_guard<std::mutex> lock(state.mutex);

                if (state.stop || state.generation != generation)
                {
                    break;
                }

                /* Every frame counts towards liveness, heartbeats included. */
                state.lastFrameMs = nowMs();
            }

            if (!delivered)
            {
                delivered = true;

                if (state.loggedLiveUrl != url)
                {
                    state.loggedLiveUrl = url;

                    Logger::logger.log(
                        "Following the daemon's event stream at " + url,
                        Logger::INFO,
                        {Logger::SYNC, Logger::DAEMON});
                }
            }

            if (result == httplib::ws::Text)
            {
                handleMessage(state, message, generation);
            }
        }

        {
            std::lock_guard<std::mutex> lock(state.mutex);

            if (state.generation == generation)
            {
                state.lastFrameMs = 0;
            }
        }

        /* The client's destructor closes the socket, on this thread. */
        return delivered ? Outcome::Delivered : Outcome::Dropped;
    }

    void watchLoop(const std::shared_ptr<TipWatchState> state)
    {
        std::chrono::seconds backoff = RECONNECT_MIN;

        unsigned failures = 0;

        /* Whether the current target has ever delivered a frame. One that has
           evidently serves /ws, so a failure is an outage and not a reason to
           leave it alone for ten minutes. */
        bool targetDelivered = false;

        std::optional<uint64_t> lastGeneration;

        while (true)
        {
            std::string host;
            uint16_t port = 0;
            bool ssl = false;
            uint64_t generation = 0;

            {
                std::unique_lock<std::mutex> lock(state->mutex);

                state->cv.wait(lock, [&] { return state->stop || state->haveTarget; });

                if (state->stop)
                {
                    break;
                }

                host = state->host;
                port = state->port;
                ssl = state->ssl;
                generation = state->generation;

                state->inIo = true;
            }

            if (lastGeneration != generation)
            {
                lastGeneration = generation;
                backoff = RECONNECT_MIN;
                failures = 0;
                targetDelivered = false;
            }

            const Outcome outcome = followOnce(*state, host, port, ssl, generation);

            std::chrono::seconds wait = backoff;

            if (outcome == Outcome::Invalid)
            {
                wait = UNSUPPORTED_RETRY;
            }
            else if (outcome == Outcome::Delivered)
            {
                targetDelivered = true;
                failures = 0;
                wait = RECONNECT_MIN;
                backoff = RECONNECT_MIN * 2;
            }
            else
            {
                if (!targetDelivered && ++failures >= UNSUPPORTED_AFTER_FAILURES)
                {
                    failures = 0;
                    wait = UNSUPPORTED_RETRY;

                    if (!state->stop)
                    {
                        logDebug(
                            "The daemon at " + eventStreamUrl(host, port, ssl)
                            + " does not seem to serve an event stream; polling it instead and trying again in "
                              "ten minutes");
                    }
                }
                else
                {
                    backoff = std::min(backoff * 2, std::chrono::seconds(RECONNECT_MAX));
                }
            }

            std::unique_lock<std::mutex> lock(state->mutex);

            state->inIo = false;

            /* A new target or a stop cuts the wait short. */
            state->cv.wait_for(lock, wait, [&] { return state->stop || state->generation != generation; });
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->inIo = false;
            state->threadExited = true;
        }

        state->cv.notify_all();
    }
} // namespace

TipWatch::TipWatch(): m_state(std::make_shared<TipWatchState>()) {}

TipWatch::~TipWatch()
{
    std::unique_lock<std::mutex> lock(m_state->mutex);

    m_state->stop = true;
    m_state->haveTarget = false;
    m_state->lastFrameMs = 0;

    {
        std::lock_guard<std::mutex> callbackLock(m_state->callbackMutex);
        m_state->generation++;
        m_state->onBlock = nullptr;
    }

    m_state->cv.notify_all();

    if (!m_state->thread.joinable())
    {
        return;
    }

    /* An idle thread - waiting out a backoff - wakes on the notify and is
       gone in moments, so join it. One inside a connect or a read cannot be
       woken from here without racing its socket; it holds its own reference
       to the state, notices the stop when its read returns, and exits then,
       so let it go rather than hold up whoever is destroying us. */
    const bool exited = !m_state->inIo
        && m_state->cv.wait_for(lock, STOP_WAIT, [&] { return m_state->threadExited; });

    lock.unlock();

    if (exited)
    {
        m_state->thread.join();
    }
    else
    {
        m_state->thread.detach();
    }
}

void TipWatch::follow(const std::string &host, const uint16_t port, const bool ssl)
{
    /* There is no /ws on a local socket, and nothing to gain from one: the
       daemon is on the same machine and the poll costs next to nothing. */
    if (host.empty() || Utilities::isIpcDaemonAddress(host))
    {
        unfollow();
        return;
    }

#if !defined(CPPHTTPLIB_SSL_ENABLED)
    /* No TLS in this build, so no wss:// either. */
    if (ssl)
    {
        unfollow();
        return;
    }
#elif defined(__ANDROID__)
    /* Android's trust store is a directory (see PlatformCaCerts.h), and the
       WebSocket client only takes a CA *file*. Without one it would fail
       verification against every daemon, so https daemons are not followed
       here and keep being polled as before. */
    if (ssl)
    {
        unfollow();
        return;
    }
#endif

    std::lock_guard<std::mutex> lock(m_state->mutex);

    if (m_state->stop)
    {
        return;
    }

    m_state->host = host;
    m_state->port = port;
    m_state->ssl = ssl;
    m_state->haveTarget = true;
    m_state->lastFrameMs = 0;

    {
        std::lock_guard<std::mutex> callbackLock(m_state->callbackMutex);
        m_state->generation++;
    }

    /* Started on first use, so a wallet that never follows anything never
       has the thread. */
    if (!m_state->thread.joinable())
    {
        m_state->thread = std::thread(watchLoop, m_state);
    }

    m_state->cv.notify_all();
}

void TipWatch::unfollow()
{
    std::lock_guard<std::mutex> lock(m_state->mutex);

    if (!m_state->haveTarget)
    {
        return;
    }

    m_state->haveTarget = false;
    m_state->lastFrameMs = 0;

    {
        std::lock_guard<std::mutex> callbackLock(m_state->callbackMutex);
        m_state->generation++;
    }

    m_state->cv.notify_all();
}

bool TipWatch::isLive() const
{
    const int64_t lastFrame = m_state->lastFrameMs;

    return lastFrame != 0
           && nowMs() - lastFrame < std::chrono::duration_cast<std::chrono::milliseconds>(LIVE_WINDOW).count();
}

uint64_t TipWatch::events() const
{
    return m_state->events;
}

uint64_t TipWatch::blockEvents() const
{
    return m_state->blockEvents;
}

void TipWatch::setOnBlock(std::function<void(uint64_t)> onBlock)
{
    std::lock_guard<std::mutex> lock(m_state->callbackMutex);
    m_state->onBlock = std::move(onBlock);
}

#else

/* The web wallet has no threads to run a watch on, and its JavaScript worker
   follows the daemon's stream itself (see wallet_worker.js), so here the
   watch is inert: never live, never counts anything. */
struct TipWatchState
{
};

TipWatch::TipWatch() {}

TipWatch::~TipWatch() {}

void TipWatch::follow(const std::string &, const uint16_t, const bool) {}

void TipWatch::unfollow() {}

bool TipWatch::isLive() const
{
    return false;
}

uint64_t TipWatch::events() const
{
    return 0;
}

uint64_t TipWatch::blockEvents() const
{
    return 0;
}

void TipWatch::setOnBlock(std::function<void(uint64_t)>) {}

#endif
