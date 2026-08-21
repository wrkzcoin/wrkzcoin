// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "CommonLogger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace Logging
{
    namespace
    {
        /* Reproduce the boost::posix_time stream format this logger has always
           emitted ("2026-Aug-21" / "05:17:30.123456", local time) without
           pulling Boost.DateTime into every translation unit that logs. */
        std::tm toLocalTm(const std::chrono::system_clock::time_point time)
        {
            const std::time_t tt = std::chrono::system_clock::to_time_t(time);
            std::tm tm {};
#if defined(_MSC_VER)
            localtime_s(&tm, &tt);
#elif defined(_WIN32)
            /* MinGW: the Windows CRT localtime() uses a per-thread buffer. */
            if (const std::tm *local = std::localtime(&tt))
            {
                tm = *local;
            }
#else
            localtime_r(&tt, &tm);
#endif
            return tm;
        }

        std::string formatDate(const std::chrono::system_clock::time_point time)
        {
            static const char *const MONTHS[12] =
                {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            const std::tm tm = toLocalTm(time);
            std::ostringstream s;
            s << (tm.tm_year + 1900) << '-' << MONTHS[tm.tm_mon] << '-' << std::setw(2) << std::setfill('0')
              << tm.tm_mday;
            return s.str();
        }

        std::string formatTime(const std::chrono::system_clock::time_point time)
        {
            const std::tm tm = toLocalTm(time);
            const auto micros =
                std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count() % 1000000;
            std::ostringstream s;
            s << std::setfill('0') << std::setw(2) << tm.tm_hour << ':' << std::setw(2) << tm.tm_min << ':'
              << std::setw(2) << tm.tm_sec << '.' << std::setw(6) << micros;
            return s.str();
        }

        std::string formatPattern(
            const std::string &pattern,
            const std::string &category,
            Level level,
            std::chrono::system_clock::time_point time)
        {
            std::stringstream s;

            for (const char *p = pattern.c_str(); p && *p != 0; ++p)
            {
                if (*p == '%')
                {
                    ++p;
                    switch (*p)
                    {
                        case 0:
                            break;
                        case 'C':
                            s << category;
                            break;
                        case 'D':
                            s << formatDate(time);
                            break;
                        case 'T':
                            s << formatTime(time);
                            break;
                        case 'L':
                            s << std::setw(7) << std::left << ILogger::LEVEL_NAMES[level];
                            break;
                        default:
                            s << *p;
                    }
                }
                else
                {
                    s << *p;
                }
            }

            return s.str();
        }

    } // namespace

    void CommonLogger::
        operator()(const std::string &category, Level level, std::chrono::system_clock::time_point time, const std::string &body)
    {
        if (level <= logLevel && disabledCategories.count(category) == 0)
        {
            std::string body2 = body;
            if (!pattern.empty())
            {
                size_t insertPos = 0;
                if (!body2.empty() && body2[0] == ILogger::COLOR_DELIMETER)
                {
                    size_t delimPos = body2.find(ILogger::COLOR_DELIMETER, 1);
                    if (delimPos != std::string::npos)
                    {
                        insertPos = delimPos + 1;
                    }
                }

                body2.insert(insertPos, formatPattern(pattern, category, level, time));
            }

            doLogString(body2);
        }
    }

    void CommonLogger::setPattern(const std::string &pattern)
    {
        this->pattern = pattern;
    }

    void CommonLogger::disableCategory(const std::string &category)
    {
        disabledCategories.insert(category);
    }

    void CommonLogger::setMaxLevel(Level level)
    {
        logLevel = level;
    }

    CommonLogger::CommonLogger(Level level): logLevel(level), pattern("%D %T %L [%C] ") {}

    void CommonLogger::doLogString(const std::string &message) {}

} // namespace Logging
