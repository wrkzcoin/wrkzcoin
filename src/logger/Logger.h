// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace Logger
{
    enum LogLevel
    {
        TRACE = 5,
        DEBUG = 4,
        INFO = 3,
        WARNING = 2,
        FATAL = 1,
        DISABLED = 0,
    };

    enum LogCategory
    {
        SYNC,
        TRANSACTIONS,
        FILESYSTEM,
        SAVE,
        DAEMON,
        DAEMON_RPC,
        DATABASE,
    };

    std::string logLevelToString(const LogLevel level);

    LogLevel stringToLogLevel(std::string level);

    std::string logCategoryToString(const LogCategory category);

    class Logger
    {
      public:
        Logger() {};

        void log(const std::string &message, const LogLevel level, const std::vector<LogCategory> &categories)
            const;

        /* Cheap enough to call from a hot loop. Building the message and the
           category vector for a line that will be discarded costs several
           allocations, so per block and per transaction call sites should
           guard on this rather than relying on log() to drop the line. */
        bool shouldLog(const LogLevel level) const
        {
            return level != DISABLED && level <= m_logLevel;
        }

        void setLogLevel(const LogLevel level);

        void setLogCallback(std::function<void(
                                const std::string prettyMessage,
                                const std::string message,
                                const LogLevel level,
                                const std::vector<LogCategory> categories)> callback);

      private:
        /* Logging disabled by default. Atomic because shouldLog() and log()
           are read from every sync thread while setLogLevel() may be called
           from the main thread. */
        std::atomic<LogLevel> m_logLevel = DISABLED;

        std::function<void(
            const std::string prettyMessage,
            const std::string message,
            const LogLevel level,
            const std::vector<LogCategory> categories)>
            m_callback;
    };

    /* Global logger instance */
    extern Logger logger;
} // namespace Logger
