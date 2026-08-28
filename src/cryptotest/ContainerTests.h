// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>

namespace ContainerTests
{
    /* Drives the container under test and a naive reference model through one
       identical randomised operation sequence, comparing their observable
       state after every step. Calls exit(1) on the first divergence, printing
       the seed so the run can be reproduced.

       The model is a plain vector walked by linear scan, so it shares no
       machinery with the hash maps and ordered maps it is checking. */
    void testTransactionPoolContainer(uint64_t seed, uint64_t iterations);

    /* BlockchainCache::spentKeyImages - an ordered_non_unique index on block
       index beside a hashed_unique index on key image. The ordered index is
       used for the segment split (lower_bound, range insert, range erase), so
       the test exercises that as well as the point lookups. */
    void testSpentKeyImagesContainer(uint64_t seed, uint64_t iterations);

    /* BlockchainCache::paymentIds - a hashed_non_unique index on payment id
       beside a hashed_unique index on transaction hash. */
    void testPaymentIdContainer(uint64_t seed, uint64_t iterations);

    /* BlockchainCache::transactions - a hashed_unique composite index on
       (block index, transaction index), an ordered_non_unique index on block
       index, and a hashed_unique index on transaction hash. The replacement
       serves the first two from a single ordered map, so the test checks the
       composite lookups and the block-ordered split together. */
    void testTransactionsCacheContainer(uint64_t seed, uint64_t iterations);

    /* BlockchainCache::blockInfos - a random_access index beside hashed_unique
       on block hash and ordered_non_unique on timestamp. Exercises positional
       access, the hash-to-position lookup that replaced project<>(), the
       timestamp range query, and the tail split. */
    void testBlockInfoContainer(uint64_t seed, uint64_t iterations);

    /* Runs every container test with the default seed and iteration count. */
    void runAll();
} // namespace ContainerTests
