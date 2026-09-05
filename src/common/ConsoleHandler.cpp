// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.
#include "ConsoleHandler.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "linenoise.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <io.h>
#include <stdio.h>
#include <windows.h>

#else
#include <stdio.h>
#include <unistd.h>
#endif

#include <vector>

using Common::Console::Color;

namespace
{
    /* Splits on 'delimiter', collapsing runs of consecutive delimiters into
       one. This is the boost::token_compress_on behaviour the console has
       always had, so "set_log  4" stays a two-token command. Input is
       trimmed before it reaches here, so no empty leading/trailing token is
       produced. */
    std::vector<std::string> splitCompressed(const std::string &text, char delimiter)
    {
        std::vector<std::string> tokens;
        size_t pos = 0;

        while (pos < text.size())
        {
            const size_t next = text.find(delimiter, pos);

            if (next == std::string::npos)
            {
                tokens.push_back(text.substr(pos));
                break;
            }

            tokens.push_back(text.substr(pos, next - pos));
            pos = text.find_first_not_of(delimiter, next);

            if (pos == std::string::npos)
            {
                break;
            }
        }

        return tokens;
    }

    /* Strips leading and trailing whitespace in place, over the same
       character set boost::algorithm::trim used. */
    void trimInPlace(std::string &text)
    {
        const char *whitespace = " \t\n\v\f\r";

        const size_t last = text.find_last_not_of(whitespace);

        if (last == std::string::npos)
        {
            text.clear();
            return;
        }

        text.erase(last + 1);
        text.erase(0, text.find_first_not_of(whitespace));
    }
} // namespace

namespace Common
{
    bool readConsoleLine(const std::string &prompt, std::string &line)
    {
        line.clear();

        if (linenoise::Readline(prompt.c_str(), line))
        {
            return false;
        }

        /* Not a terminal and out of input: see consoleThread below for why
           linenoise does not report this itself. */
        if (!std::cin.good())
        {
            return false;
        }

        if (!line.empty())
        {
            linenoise::AddHistory(line.c_str());
        }

        return true;
    }

    /////////////////////////////////////////////////////////////////////////////
    // AsyncConsoleReader
    /////////////////////////////////////////////////////////////////////////////
    AsyncConsoleReader::AsyncConsoleReader(): m_stop(true) {}

    AsyncConsoleReader::~AsyncConsoleReader()
    {
        stop();
    }

    void AsyncConsoleReader::start()
    {
        m_stop = false;
        m_thread = std::thread(std::bind(&AsyncConsoleReader::consoleThread, this));
    }

    bool AsyncConsoleReader::getline(std::string &line)
    {
        return m_queue.pop(line);
    }

    void AsyncConsoleReader::pause()
    {
        if (m_stop)
        {
            return;
        }

        m_stop = true;

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        m_thread = std::thread();
    }

    void AsyncConsoleReader::unpause()
    {
        start();
    }

    void AsyncConsoleReader::stop()
    {
        if (m_stop)
        {
            return; // already stopping/stopped
        }

        m_stop = true;
        m_queue.close();
#ifdef _WIN32
        ::CloseHandle(::GetStdHandle(STD_INPUT_HANDLE));
#else
        ::close(STDIN_FILENO);
#endif

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        m_thread = std::thread();
    }

    bool AsyncConsoleReader::stopped() const
    {
        return m_stop;
    }

    void AsyncConsoleReader::consoleThread()
    {
#ifdef _WIN32
        /* On Windows the NUL device counts as a character device, so a
           daemon started with no stdin passes _isatty and linenoise takes its
           raw-mode path. That path asks the console mode of stdout, and when
           stdout is a file or a pipe it closes the stdout handle it was given
           before giving up - taking the process's own stdout with it, and
           then whichever socket or file next reuses that handle value, on
           every spin of the loop below. Neither a NUL stdin nor a redirected
           stdout is a console we can read from, so behave as --no-console.
           A pipe on stdin is not a character device and still reads lines. */
        if (_isatty(_fileno(stdin)))
        {
            DWORD mode = 0;

            const bool stdinIsConsole = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
            const bool stdoutIsConsole = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) != 0;

            if (!stdinIsConsole || !stdoutIsConsole)
            {
                std::cout << "Console input is not an interactive console; no commands will be read from stdin."
                          << std::endl;
                return;
            }
        }
#endif

        linenoise::SetHistoryMaxLen(256);

        while (!m_stop)
        {
            std::string line;
            const bool quit = linenoise::Readline("", line);
            if (quit)
            {
                break;
            }

            /* With stdin at end of file - a process manager that handed us
               /dev/null, or a piped script that has run out - the non-tty
               path in linenoise returns an empty line with quit unset, and
               does so again instantly on the next call. Without this check
               the reader and the handler thread ping-pong empty lines at full
               speed. Stop reading instead: the process keeps running with no
               console, which is what --no-console would have given it. */
            if (!std::cin.good())
            {
                if (!m_stop)
                {
                    std::cout << "Console input closed; no further commands will be read from stdin." << std::endl;
                }

                break;
            }

            if (!line.empty())
            {
                linenoise::AddHistory(line.c_str());
            }

            if (!m_queue.push(line))
            {
                break;
            }
        }
    }

    bool AsyncConsoleReader::waitInput()
    {
#ifndef _WIN32
        int stdin_fileno = ::fileno(stdin);

        while (!m_stop)
        {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(stdin_fileno, &read_set);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100 * 1000;

            int retval = ::select(stdin_fileno + 1, &read_set, NULL, NULL, &tv);

            if (retval == -1 && errno == EINTR)
            {
                continue;
            }

            if (retval < 0)
            {
                return false;
            }

            if (retval > 0)
            {
                return true;
            }
        }
#endif

        return !m_stop;
    }

    /////////////////////////////////////////////////////////////////////////////
    // ConsoleHandler
    /////////////////////////////////////////////////////////////////////////////
    ConsoleHandler::~ConsoleHandler()
    {
        stop();
    }

    void ConsoleHandler::start(bool startThread, const std::string &prompt, Console::Color promptColor)
    {
        m_prompt = prompt;
        m_promptColor = promptColor;
        m_consoleReader.start();

        if (startThread)
        {
            m_thread = std::thread(std::bind(&ConsoleHandler::handlerThread, this));
        }
        else
        {
            handlerThread();
        }
    }

    void ConsoleHandler::stop()
    {
        requestStop();
        wait();
    }

    void ConsoleHandler::pause()
    {
        m_consoleReader.pause();
    }

    void ConsoleHandler::unpause()
    {
        m_consoleReader.unpause();
    }

    void ConsoleHandler::wait()
    {
        try
        {
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }
        catch (std::exception &e)
        {
            std::cerr << "Exception in ConsoleHandler::wait - " << e.what() << std::endl;
        }
    }

    void ConsoleHandler::requestStop()
    {
        m_consoleReader.stop();
    }

    std::string ConsoleHandler::getUsage() const
    {
        if (m_handlers.empty())
        {
            return std::string();
        }

        std::stringstream ss;

        uint64_t maxlen = std::max_element(
                              m_handlers.begin(),
                              m_handlers.end(),
                              [](CommandHandlersMap::const_reference &a, CommandHandlersMap::const_reference &b) {
                                  return a.first.size() < b.first.size();
                              })
                              ->first.size();

        for (auto &x : m_handlers)
        {
            ss << std::left << std::setw(maxlen + 3) << x.first << x.second.second << std::endl;
        }

        return ss.str();
    }

    void ConsoleHandler::setHandler(
        const std::string &command,
        const ConsoleCommandHandler &handler,
        const std::string &usage)
    {
        m_handlers[command] = std::make_pair(handler, usage);
    }

    bool ConsoleHandler::runCommand(const std::vector<std::string> &cmdAndArgs)
    {
        if (cmdAndArgs.size() == 0)
        {
            return false;
        }

        const auto &cmd = cmdAndArgs.front();
        auto hIter = m_handlers.find(cmd);

        if (hIter == m_handlers.end())
        {
            std::cout << "Unknown command: " << cmd << std::endl;
            return false;
        }

        std::vector<std::string> args(cmdAndArgs.begin() + 1, cmdAndArgs.end());
        hIter->second.first(args);
        return true;
    }

    std::vector<std::string> ConsoleHandler::splitCommandLine(const std::string &line)
    {
        std::string trimmed = line;
        trimInPlace(trimmed);

        if (trimmed.empty())
        {
            return {};
        }

        return splitCompressed(trimmed, ' ');
    }

    bool ConsoleHandler::hasCommand(const std::string &command) const
    {
        return m_handlers.find(command) != m_handlers.end();
    }

    void ConsoleHandler::handleCommand(const std::string &cmd)
    {
        runCommand(splitCommandLine(cmd));
    }

    void ConsoleHandler::handlerThread()
    {
        std::string line;

        while (!m_consoleReader.stopped())
        {
            try
            {
                if (!m_prompt.empty())
                {
                    if (m_promptColor != Color::Default)
                    {
                        Console::setTextColor(m_promptColor);
                    }

                    std::cout << m_prompt;
                    std::cout.flush();

                    if (m_promptColor != Color::Default)
                    {
                        Console::setTextColor(Color::Default);
                    }
                }

                if (!m_consoleReader.getline(line))
                {
                    break;
                }

                trimInPlace(line);
                if (!line.empty())
                {
                    handleCommand(line);
                }
            }
            catch (std::exception &)
            {
                // ignore errors
            }
        }
    }
} // namespace Common
