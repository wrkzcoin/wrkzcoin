// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "SignalHandler.h"

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#else
#include <pthread.h>
#include <cstring>
#include <signal.h>
#endif

namespace
{
    std::function<void(void)> m_handler;

    void handleSignal()
    {
        static std::mutex m_mutex;
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        if (!lock.owns_lock())
        {
            return;
        }

        if (m_handler)
        {
            m_handler();
        }
    }

#if defined(WIN32)

    BOOL WINAPI winHandler(DWORD type)
    {
        if (CTRL_C_EVENT == type || CTRL_BREAK_EVENT == type)
        {
            handleSignal();
            return TRUE;
        }
        else
        {
            std::cerr << "Got control signal " << type << ". Exiting without saving...";
            return FALSE;
        }
        return TRUE;
    }

#else

    std::atomic<bool> signalThreadStarted(false);
#endif

} // namespace

namespace Tools
{
    bool SignalHandler::install(std::function<void(void)> t)
    {
#if defined(WIN32)
        bool r = TRUE == ::SetConsoleCtrlHandler(&winHandler, TRUE);
        if (r)
        {
            m_handler = t;
        }
        return r;
#else
        m_handler = t;

        struct sigaction ignoreMask;
        std::memset(&ignoreMask, 0, sizeof(struct sigaction));
        ignoreMask.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &ignoreMask, nullptr) != 0)
        {
            return false;
        }

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);

        if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0)
        {
            return false;
        }

        if (!signalThreadStarted.exchange(true))
        {
            std::thread([set]() mutable {
                while (true)
                {
                    int signalNumber = 0;
                    const int rc = sigwait(&set, &signalNumber);
                    if (rc == 0 && (signalNumber == SIGINT || signalNumber == SIGTERM))
                    {
                        handleSignal();
                    }
                }
            }).detach();
        }

        return true;
#endif
    }
} // namespace Tools
