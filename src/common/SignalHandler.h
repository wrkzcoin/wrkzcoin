// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <functional>

namespace Tools
{
    class SignalHandler
    {
      public:
        /* Blocks SIGINT/SIGTERM on the calling thread so that every thread
           created afterwards inherits the mask and the handler installed by
           install() is the only place they are consumed. Call this before
           starting any thread; without it Linux hands the signal to whichever
           thread has not blocked it, and the default action ends the process
           before the graceful shutdown gets a chance to run. A no-op on
           Windows, which has no such inheritance. */
        static bool blockSignals();

        static bool install(std::function<void(void)> t);
    };
} // namespace Tools
