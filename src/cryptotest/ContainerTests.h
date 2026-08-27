// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>

namespace ContainerTests
{
    /* Differential test: drives a boost::multi_index container and the
       hand-rolled replacement that supersedes it through one identical
       randomised operation sequence, and compares their observable state
       after every step. Calls exit(1) on the first divergence, printing the
       seed so the run can be reproduced.

       This can only be written while Boost is still a dependency, which is
       precisely why it is being written now. */
    void testTransactionPoolContainer(uint64_t seed, uint64_t iterations);

    /* BlockchainCache::spentKeyImages - an ordered_non_unique index on block
       index beside a hashed_unique index on key image. The ordered index is
       used for the segment split (lower_bound, range insert, range erase), so
       the test exercises that as well as the point lookups. */
    void testSpentKeyImagesContainer(uint64_t seed, uint64_t iterations);

    /* BlockchainCache::paymentIds - a hashed_non_unique index on payment id
       beside a hashed_unique index on transaction hash. */
    void testPaymentIdContainer(uint64_t seed, uint64_t iterations);

    /* Runs every container test with the default seed and iteration count. */
    void runAll();
} // namespace ContainerTests
