// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "version.h"

#include <cstdint>
#include <string>

namespace CryptoNote
{
    struct MiningConfig
    {
        MiningConfig();

        void parse(int argc, char **argv);

        std::string miningAddress;

        std::string daemonAddress;

        std::string daemonHost;

        uint16_t daemonPort;

        size_t threadCount;

        size_t scanPeriod;

        size_t blocksLimit;

        /* Seconds between hash rate reports. 0 turns the reporter off. */
        size_t hashRateInterval;

        /* Seconds to hash locally and then exit, without needing a daemon.
           0 means mine normally. */
        size_t benchmarkSeconds;

        /* Connect, read and write timeout for daemon requests, in seconds. */
        size_t daemonTimeout;

        /* Seconds to wait before asking an unreachable daemon again. */
        size_t retryInterval;

        uint64_t firstBlockTimestamp;

        int64_t blockTimestampInterval;

        bool help;

        bool version;
    };

} // namespace CryptoNote
