// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockingQueue.h"
#include "ConsoleTools.h"

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/select.h>
#endif

namespace Common
{
    /* Reads one line from the terminal with editing and history, or from a
       pipe when stdin is not a terminal. Returns false at end of input or on
       Ctrl+C / Ctrl+D. Lives here because linenoise is header-only with a
       few non-static globals, so exactly one translation unit per binary may
       include it - this one. */
    bool readConsoleLine(const std::string &prompt, std::string &line);

    class AsyncConsoleReader
    {
      public:
        AsyncConsoleReader();

        ~AsyncConsoleReader();

        void start();

        bool getline(std::string &line);

        void stop();

        bool stopped() const;

        void pause();

        void unpause();

      private:
        void consoleThread();

        bool waitInput();

        std::atomic<bool> m_stop;

        std::thread m_thread;

        BlockingQueue<std::string> m_queue;
    };

    class ConsoleHandler
    {
      public:
        ~ConsoleHandler();

        typedef std::function<bool(const std::vector<std::string> &)> ConsoleCommandHandler;

        std::string getUsage() const;

        void
            setHandler(const std::string &command, const ConsoleCommandHandler &handler, const std::string &usage = "");

        void requestStop();

        bool runCommand(const std::vector<std::string> &cmdAndArgs);

        /* Tokenises a command line the way the interactive reader does -
           trimmed, split on spaces with runs collapsed - so a command that
           arrives over a socket is parsed exactly like one that was typed. */
        static std::vector<std::string> splitCommandLine(const std::string &line);

        bool hasCommand(const std::string &command) const;

        void start(
            bool startThread = true,
            const std::string &prompt = "",
            Console::Color promptColor = Console::Color::Default);

        void stop();

        void wait();

        void pause();

        void unpause();

      private:
        typedef std::map<std::string, std::pair<ConsoleCommandHandler, std::string>> CommandHandlersMap;

        virtual void handleCommand(const std::string &cmd);

        void handlerThread();

        std::thread m_thread;

        std::string m_prompt;

        Console::Color m_promptColor = Console::Color::Default;

        CommandHandlersMap m_handlers;

        AsyncConsoleReader m_consoleReader;
    };
} // namespace Common
