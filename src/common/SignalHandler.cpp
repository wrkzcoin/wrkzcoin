// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "SignalHandler.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

    std::atomic<uint32_t> interruptCount(0);

    /* The second interrupt is the one a user reaches for when the first has not
       worked, so it has to be the one that cannot get stuck. It is answered
       here, before the handler is consulted at all: the previous code took a
       mutex with try_lock and returned when it could not get it, so every
       interrupt after the first was discarded precisely while the shutdown it
       was meant to escape was still running. */
    void handleSignal()
    {
        if (interruptCount.fetch_add(1) > 0)
        {
            std::cerr << "Second interrupt received. Forcing immediate exit without waiting for shutdown."
                      << std::endl;
            std::_Exit(1);
        }

        if (m_handler)
        {
            /* On its own thread, so whichever thread delivered this signal is
               free again immediately. A shutdown can take a long time, and can
               wedge; running it here would keep the POSIX sigwait loop out of
               sigwait for the whole of it, leaving the next interrupt pending
               and unread - the force exit above could then never be reached. */
            std::thread(m_handler).detach();
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
    bool SignalHandler::blockSignals()
    {
#if defined(WIN32)
        return true;
#else
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);

        return pthread_sigmask(SIG_BLOCK, &set, nullptr) == 0;
#endif
    }

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
