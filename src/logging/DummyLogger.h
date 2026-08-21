// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <logging/ILogger.h>

namespace Logging
{
    class DummyLogger : public ILogger
    {
      public:
        virtual ~DummyLogger() {};

        virtual void
            operator()(const std::string &category, Level level, std::chrono::system_clock::time_point time, const std::string &body)
                override
        {
            // do nothing
        }
    };

} // namespace Logging
