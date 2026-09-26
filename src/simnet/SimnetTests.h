// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <logging/ILogger.h>
#include <memory>
#include <string>

namespace Simnet
{
    /* wrkz-simnet test: whole nodes in this process, checked end to end.
       Each scenario gets its own directory under workDir. Returns the number
       of scenarios that failed. */
    int runTests(std::shared_ptr<Logging::ILogger> logger, const std::string &workDir);
} // namespace Simnet
