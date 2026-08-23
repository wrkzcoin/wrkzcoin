// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Tools
{
    /* Fire-and-forget notification sink shared by the daemon and the wallets
       (--block-notify / --reorg-notify / --tx-notify and friends).

       The spec is either:
         * an http:// or https:// URL  -> every notification is POSTed as a JSON
           object ({"event":"block","height":...,"hash":"..."}), or
         * a command line template     -> Monero-style: tokenised on whitespace
           (double/single quotes group), %-placeholders substituted inside each
           token (%s, %h, ... as documented per flag, %% for a literal percent)
           and executed WITHOUT a shell.

       Delivery happens on a dedicated worker thread with a bounded queue, so
       callers (dispatcher fibers, wallet sync threads) never block. Each
       delivery has a timeout; hung children are killed, slow webhooks abort. */
    class Notifier
    {
      public:
        enum class LogLevel
        {
            Info,
            Warning
        };

        using LogFn = std::function<void(LogLevel, const std::string &)>;

        /* One JSON field of the webhook body. `quoted` = render as string. */
        struct Field
        {
            std::string key;
            std::string value;
            bool quoted;
        };

        struct Notification
        {
            /* "block", "reorg", "tx", ... – becomes the "event" JSON field. */
            std::string event;

            /* %-placeholders for the command form: 's' -> "<hash>", ... */
            std::vector<std::pair<char, std::string>> placeholders;

            /* JSON fields for the webhook form (in order, after "event"). */
            std::vector<Field> fields;
        };

        static constexpr std::chrono::seconds DEFAULT_TIMEOUT {10};

        static constexpr size_t DEFAULT_MAX_QUEUE = 1024;

        /* name: short label used in log lines (e.g. "block-notify").
           spec: URL or command template; empty disables the notifier.
           log:  optional logging callback; may be empty. */
        Notifier(
            std::string name,
            std::string spec,
            LogFn log = {},
            std::chrono::seconds timeout = DEFAULT_TIMEOUT,
            size_t maxQueue = DEFAULT_MAX_QUEUE);

        ~Notifier();

        Notifier(const Notifier &) = delete;

        Notifier &operator=(const Notifier &) = delete;

        /* True when a usable spec was supplied (and, for https, the build
           supports it). Callers may skip building notifications otherwise. */
        bool enabled() const;

        bool isWebhook() const;

        const std::string &name() const;

        const std::string &spec() const;

        /* Enqueue a notification. Never blocks; drops (and counts) when the
           queue is full or the notifier is disabled/stopped. */
        void notify(Notification notification);

        /* Stop the worker. Queued notifications are discarded; an in-flight
           delivery is allowed to finish (bounded by the timeout). Idempotent. */
        void stop();

        uint64_t sent() const;

        uint64_t failed() const;

        uint64_t dropped() const;

        /* Helpers exposed for reuse/testing. */
        static bool isUrl(const std::string &spec);

        static std::vector<std::string> tokenize(const std::string &commandLine);

        static std::string substitute(
            const std::string &token,
            const std::vector<std::pair<char, std::string>> &placeholders);

        static std::string jsonEscape(const std::string &value);

        static std::string buildJson(const Notification &notification);

      private:
        void workerLoop();

        bool deliver(const Notification &notification);

        bool postWebhook(const std::string &body);

        bool runCommand(const std::vector<std::string> &argv);

        bool stopRequested() const;

        void log(LogLevel level, const std::string &message) const;

        std::string m_name;

        std::string m_spec;

        LogFn m_log;

        std::chrono::seconds m_timeout;

        size_t m_maxQueue;

        bool m_enabled;

        bool m_isWebhook;

        /* Webhook: scheme://host[:port] and /path?query split once up front. */
        std::string m_urlBase;

        std::string m_urlPath;

        /* Command: pre-tokenised template (placeholders still unsubstituted). */
        std::vector<std::string> m_template;

        mutable std::mutex m_mutex;

        std::condition_variable m_condition;

        std::deque<Notification> m_queue;

        std::thread m_worker;

        bool m_stopping;

        std::atomic<uint64_t> m_sent;

        std::atomic<uint64_t> m_failed;

        std::atomic<uint64_t> m_dropped;
    };
} // namespace Tools
