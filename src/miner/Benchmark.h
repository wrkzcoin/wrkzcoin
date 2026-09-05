// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstddef>

namespace Miner
{
    /* Hashes for the given number of seconds with the proof of work the
       network is currently on, and prints the rate. Needs no daemon, no block
       template and no mining address, so it measures the hashing alone. */
    void runBenchmark(const size_t seconds, const size_t threadCount);

} // namespace Miner
