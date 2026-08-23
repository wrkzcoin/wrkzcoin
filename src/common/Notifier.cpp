// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "Notifier.h"

#if !defined(__EMSCRIPTEN__)
/* httplib pulls in winsock2.h and must precede <windows.h>. */
#include "httplib.h"
#endif

#if defined(_WIN32)
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if !(defined(__ANDROID__) && __ANDROID_API__ < 28)
#include <spawn.h>
#define WRKZ_NOTIFIER_HAVE_POSIX_SPAWN 1
#if defined(__APPLE__)
#include <crt_externs.h>
#define WRKZ_NOTIFIER_ENVIRON (*_NSGetEnviron())
#else
extern char **environ;
#define WRKZ_NOTIFIER_ENVIRON environ
#endif
#endif
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace Tools
{
    namespace
    {
        bool startsWithNoCase(const std::string &value, const char *prefix)
        {
            size_t i = 0;
            for (; prefix[i] != '\0'; ++i)
            {
                if (i >= value.size()
                    || std::tolower(static_cast<unsigned char>(value[i]))
                           != std::tolower(static_cast<unsigned char>(prefix[i])))
                {
                    return false;
                }
            }
            return true;
        }

        /* Split "scheme://host[:port]/path?query" into "scheme://host[:port]"
           and "/path?query" (path defaults to "/"). */
        bool splitUrl(const std::string &url, std::string &base, std::string &path)
        {
            const auto schemeEnd = url.find("://");
            if (schemeEnd == std::string::npos)
            {
                return false;
            }

            const size_t hostStart = schemeEnd + 3;
            if (hostStart >= url.size())
            {
                return false;
            }

            size_t pathStart = std::string::npos;
            if (url[hostStart] == '[')
            {
                const auto close = url.find(']', hostStart);
                if (close == std::string::npos)
                {
                    return false;
                }
                pathStart = url.find_first_of("/?", close);
            }
            else
            {
                pathStart = url.find_first_of("/?", hostStart);
            }

            if (pathStart == std::string::npos)
            {
                base = url;
                path = "/";
            }
            else
            {
                base = url.substr(0, pathStart);
                path = url.substr(pathStart);
                if (path[0] == '?')
                {
                    path = "/" + path;
                }
            }

            return base.size() > hostStart;
        }

#if defined(_WIN32)
        /* Quote one argument so CreateProcess/CommandLineToArgv reproduce it
           exactly (the classic "everyone quotes command line arguments the
           wrong way" algorithm). */
        std::string quoteWindowsArg(const std::string &arg)
        {
            if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos)
            {
                return arg;
            }

            std::string out = "\"";
            for (auto it = arg.begin();; ++it)
            {
                size_t backslashes = 0;
                while (it != arg.end() && *it == '\\')
                {
                    ++it;
                    ++backslashes;
                }

                if (it == arg.end())
                {
                    out.append(backslashes * 2, '\\');
                    break;
                }
                else if (*it == '"')
                {
                    out.append(backslashes * 2 + 1, '\\');
                    out.push_back('"');
                }
                else
                {
                    out.append(backslashes, '\\');
                    out.push_back(*it);
                }
            }
            out.push_back('"');
            return out;
        }
#endif
    } // namespace

    Notifier::Notifier(
        std::string name,
        std::string spec,
        LogFn logFn,
        std::chrono::seconds timeout,
        size_t maxQueue):
        m_name(std::move(name)),
        m_spec(std::move(spec)),
        m_log(std::move(logFn)),
        m_timeout(timeout),
        m_maxQueue(maxQueue == 0 ? 1 : maxQueue),
        m_enabled(false),
        m_isWebhook(false),
        m_stopping(false),
        m_sent(0),
        m_failed(0),
        m_dropped(0)
    {
        /* Trim surrounding whitespace; an all-blank spec means "disabled". */
        const auto first = m_spec.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            m_spec.clear();
            return;
        }
        const auto last = m_spec.find_last_not_of(" \t\r\n");
        m_spec = m_spec.substr(first, last - first + 1);

#if defined(__EMSCRIPTEN__)
        log(LogLevel::Warning, "notifications are not supported in this build; ignoring " + m_spec);
        return;
#else
        if (isUrl(m_spec))
        {
            m_isWebhook = true;

            if (startsWithNoCase(m_spec, "https://"))
            {
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
                log(LogLevel::Warning,
                    "https webhook configured but this build has no OpenSSL support; notifier disabled: " + m_spec);
                return;
#endif
            }

            if (!splitUrl(m_spec, m_urlBase, m_urlPath))
            {
                log(LogLevel::Warning, "invalid webhook URL; notifier disabled: " + m_spec);
                return;
            }
        }
        else
        {
            m_template = tokenize(m_spec);
            if (m_template.empty())
            {
                log(LogLevel::Warning, "empty command; notifier disabled");
                return;
            }
        }

        m_enabled = true;
        m_worker = std::thread([this] { workerLoop(); });

        log(LogLevel::Info,
            std::string(m_isWebhook ? "webhook" : "command") + " notifier enabled: " + m_spec);
#endif
    }

    Notifier::~Notifier()
    {
        stop();
    }

    bool Notifier::enabled() const
    {
        return m_enabled;
    }

    bool Notifier::isWebhook() const
    {
        return m_isWebhook;
    }

    const std::string &Notifier::name() const
    {
        return m_name;
    }

    const std::string &Notifier::spec() const
    {
        return m_spec;
    }

    void Notifier::notify(Notification notification)
    {
        if (!m_enabled)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_stopping)
            {
                return;
            }

            if (m_queue.size() >= m_maxQueue)
            {
                const uint64_t dropped = ++m_dropped;
                if (dropped == 1 || dropped % 1000 == 0)
                {
                    log(LogLevel::Warning,
                        "queue full (" + std::to_string(m_maxQueue) + "), dropping notifications. Dropped so far: "
                            + std::to_string(dropped));
                }
                return;
            }

            m_queue.push_back(std::move(notification));
        }

        m_condition.notify_one();
    }

    void Notifier::stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping)
            {
                return;
            }
            m_stopping = true;
            m_queue.clear();
        }

        m_condition.notify_all();

        if (m_worker.joinable())
        {
            m_worker.join();
        }

        if (m_enabled)
        {
            log(LogLevel::Info,
                "stopped. Sent=" + std::to_string(m_sent.load()) + ", failed=" + std::to_string(m_failed.load())
                    + ", dropped=" + std::to_string(m_dropped.load()));
        }
    }

    uint64_t Notifier::sent() const
    {
        return m_sent.load();
    }

    uint64_t Notifier::failed() const
    {
        return m_failed.load();
    }

    uint64_t Notifier::dropped() const
    {
        return m_dropped.load();
    }

    bool Notifier::isUrl(const std::string &spec)
    {
        return startsWithNoCase(spec, "http://") || startsWithNoCase(spec, "https://");
    }

    /* Whitespace-separated tokens; single or double quotes group characters
       (including whitespace) into one token. No escape sequences, so Windows
       paths with backslashes pass through untouched. */
    std::vector<std::string> Notifier::tokenize(const std::string &commandLine)
    {
        std::vector<std::string> tokens;
        std::string current;
        bool inToken = false;
        char quote = '\0';

        for (const char c : commandLine)
        {
            if (quote != '\0')
            {
                if (c == quote)
                {
                    quote = '\0';
                }
                else
                {
                    current.push_back(c);
                }
                continue;
            }

            if (c == '"' || c == '\'')
            {
                quote = c;
                inToken = true;
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(c)))
            {
                if (inToken)
                {
                    tokens.push_back(current);
                    current.clear();
                    inToken = false;
                }
                continue;
            }

            current.push_back(c);
            inToken = true;
        }

        if (inToken)
        {
            tokens.push_back(current);
        }

        return tokens;
    }

    std::string Notifier::substitute(
        const std::string &token,
        const std::vector<std::pair<char, std::string>> &placeholders)
    {
        std::string out;
        out.reserve(token.size());

        for (size_t i = 0; i < token.size(); ++i)
        {
            const char c = token[i];
            if (c != '%' || i + 1 >= token.size())
            {
                out.push_back(c);
                continue;
            }

            const char key = token[i + 1];
            if (key == '%')
            {
                out.push_back('%');
                ++i;
                continue;
            }

            const auto it = std::find_if(
                placeholders.begin(), placeholders.end(), [key](const auto &entry) { return entry.first == key; });

            if (it == placeholders.end())
            {
                /* Unknown placeholder: leave it verbatim. */
                out.push_back(c);
                continue;
            }

            out += it->second;
            ++i;
        }

        return out;
    }

    std::string Notifier::jsonEscape(const std::string &value)
    {
        std::string out;
        out.reserve(value.size() + 2);

        for (const unsigned char c : value)
        {
            switch (c)
            {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\b':
                    out += "\\b";
                    break;
                case '\f':
                    out += "\\f";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    if (c < 0x20)
                    {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                        out += buf;
                    }
                    else
                    {
                        out.push_back(static_cast<char>(c));
                    }
                    break;
            }
        }

        return out;
    }

    std::string Notifier::buildJson(const Notification &notification)
    {
        std::string out = "{\"event\":\"" + jsonEscape(notification.event) + "\"";

        for (const auto &field : notification.fields)
        {
            out += ",\"" + jsonEscape(field.key) + "\":";
            if (field.quoted)
            {
                out += "\"" + jsonEscape(field.value) + "\"";
            }
            else
            {
                out += field.value.empty() ? "null" : field.value;
            }
        }

        out += "}";
        return out;
    }

    void Notifier::workerLoop()
    {
        for (;;)
        {
            Notification notification;

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || !m_queue.empty(); });

                if (m_stopping)
                {
                    return;
                }

                notification = std::move(m_queue.front());
                m_queue.pop_front();
            }

            bool ok = false;
            try
            {
                ok = deliver(notification);
            }
            catch (const std::exception &e)
            {
                log(LogLevel::Warning, std::string("delivery threw: ") + e.what());
            }
            catch (...)
            {
                log(LogLevel::Warning, "delivery threw an unknown exception");
            }

            if (ok)
            {
                ++m_sent;
            }
            else
            {
                ++m_failed;
            }
        }
    }

    bool Notifier::deliver(const Notification &notification)
    {
        if (m_isWebhook)
        {
            return postWebhook(buildJson(notification));
        }

        std::vector<std::string> argv;
        argv.reserve(m_template.size());
        for (const auto &token : m_template)
        {
            argv.push_back(substitute(token, notification.placeholders));
        }

        return runCommand(argv);
    }

    bool Notifier::postWebhook(const std::string &body)
    {
#if defined(__EMSCRIPTEN__)
        (void)body;
        return false;
#else
        std::unique_ptr<httplib::Client> client;
        try
        {
            client = std::make_unique<httplib::Client>(m_urlBase);
        }
        catch (const std::exception &e)
        {
            log(LogLevel::Warning, "cannot create HTTP client for " + m_urlBase + ": " + e.what());
            return false;
        }

        if (!client->is_valid())
        {
            log(LogLevel::Warning, "invalid webhook base URL: " + m_urlBase);
            return false;
        }

        client->set_connection_timeout(m_timeout);
        client->set_read_timeout(m_timeout);
        client->set_write_timeout(m_timeout);
        client->set_follow_location(false);

        /* One retry on transport-level failure only; HTTP error statuses are
           final (the receiver answered). */
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            auto result = client->Post(m_urlPath, body, "application/json");

            if (result)
            {
                if (result->status >= 200 && result->status < 300)
                {
                    return true;
                }

                log(LogLevel::Warning,
                    "webhook " + m_spec + " answered HTTP " + std::to_string(result->status));
                return false;
            }

            if (attempt == 1)
            {
                log(LogLevel::Warning, "webhook " + m_spec + " failed: " + httplib::to_string(result.error()));
            }
        }

        return false;
#endif
    }

    bool Notifier::runCommand(const std::vector<std::string> &argv)
    {
        if (argv.empty())
        {
            return false;
        }

#if defined(__EMSCRIPTEN__)
        return false;
#elif defined(_WIN32)
        std::string commandLine;
        for (size_t i = 0; i < argv.size(); ++i)
        {
            if (i != 0)
            {
                commandLine.push_back(' ');
            }
            commandLine += quoteWindowsArg(argv[i]);
        }

        std::vector<char> buffer(commandLine.begin(), commandLine.end());
        buffer.push_back('\0');

        STARTUPINFOA startupInfo {};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo {};

        if (!CreateProcessA(
                nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo, &processInfo))
        {
            log(LogLevel::Warning,
                "failed to start '" + argv[0] + "': error " + std::to_string(GetLastError()));
            return false;
        }

        CloseHandle(processInfo.hThread);

        /* Poll in short slices so a shutdown request can cut the wait short. */
        const auto deadline = std::chrono::steady_clock::now() + m_timeout;
        DWORD waitResult = WAIT_TIMEOUT;
        while (waitResult == WAIT_TIMEOUT && !stopRequested() && std::chrono::steady_clock::now() < deadline)
        {
            waitResult = WaitForSingleObject(processInfo.hProcess, 50);
        }

        bool ok = false;
        if (waitResult == WAIT_OBJECT_0)
        {
            DWORD exitCode = 1;
            GetExitCodeProcess(processInfo.hProcess, &exitCode);
            ok = (exitCode == 0);
            if (!ok)
            {
                log(LogLevel::Warning, "'" + argv[0] + "' exited with code " + std::to_string(exitCode));
            }
        }
        else
        {
            TerminateProcess(processInfo.hProcess, 1);
            WaitForSingleObject(processInfo.hProcess, 1000);
            log(LogLevel::Warning,
                "'" + argv[0] + "' did not finish within " + std::to_string(m_timeout.count()) + "s; killed");
        }

        CloseHandle(processInfo.hProcess);
        return ok;
#else
        std::vector<char *> args;
        args.reserve(argv.size() + 1);
        for (const auto &arg : argv)
        {
            args.push_back(const_cast<char *>(arg.c_str()));
        }
        args.push_back(nullptr);

        pid_t pid = -1;

#if defined(WRKZ_NOTIFIER_HAVE_POSIX_SPAWN)
        /* posix_spawn avoids duplicating the (potentially multi-GB) daemon
           address space that a plain fork() would touch. */
        const int spawnError = posix_spawnp(&pid, args[0], nullptr, nullptr, args.data(), WRKZ_NOTIFIER_ENVIRON);
        if (spawnError != 0)
        {
            log(LogLevel::Warning, "failed to start '" + argv[0] + "': " + std::strerror(spawnError));
            return false;
        }
#else
        pid = fork();
        if (pid < 0)
        {
            log(LogLevel::Warning, std::string("fork failed: ") + std::strerror(errno));
            return false;
        }

        if (pid == 0)
        {
            execvp(args[0], args.data());
            _exit(127);
        }
#endif

        const auto deadline = std::chrono::steady_clock::now() + m_timeout;
        int status = 0;

        for (;;)
        {
            const pid_t waited = waitpid(pid, &status, WNOHANG);
            if (waited == pid)
            {
                break;
            }

            if (waited < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                log(LogLevel::Warning, std::string("waitpid failed: ") + std::strerror(errno));
                return false;
            }

            if (std::chrono::steady_clock::now() >= deadline || stopRequested())
            {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                log(LogLevel::Warning,
                    "'" + argv[0] + "' did not finish within " + std::to_string(m_timeout.count())
                        + "s or shutdown requested; killed");
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (WIFEXITED(status))
        {
            const int exitCode = WEXITSTATUS(status);
            if (exitCode == 0)
            {
                return true;
            }

            log(LogLevel::Warning, "'" + argv[0] + "' exited with code " + std::to_string(exitCode));
            return false;
        }

        if (WIFSIGNALED(status))
        {
            log(LogLevel::Warning,
                "'" + argv[0] + "' terminated by signal " + std::to_string(WTERMSIG(status)));
        }

        return false;
#endif
    }

    bool Notifier::stopRequested() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stopping;
    }

    void Notifier::log(LogLevel level, const std::string &message) const
    {
        if (m_log)
        {
            m_log(level, "[" + m_name + "] " + message);
        }
    }
} // namespace Tools
