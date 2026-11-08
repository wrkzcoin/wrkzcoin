// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace GlobalIndexPlanTests
{
    /* Checks how a wallet groups the global index requests for a chunk of
       blocks: one 10-block window per block with an output of ours, each
       window asked for once, adjacent windows joined while the range stays
       within what every daemon accepts, and never a height that none of the
       windows named - joining must not reveal more than asking one at a time.
       Calls exit(1) on the first failure. */
    void runAll();
} // namespace GlobalIndexPlanTests
