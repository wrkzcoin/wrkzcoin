// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <cstdint>
#include <vector>

namespace CryptoNote
{
    // Deterministic quorum selection shared by ChainLock and InstantSend.
    //
    // Algorithm:
    //   For each masternode ID in the active set, compute score = sha256(mnId || seed).
    //   Sort by score ascending, take the first `quorumSize` entries.
    //
    // The `seed` is:
    //   - For ChainLock:   blockHash at the height being locked
    //   - For InstantSend: txHash of the transaction being locked
    //
    // This ensures each block / each transaction gets an independently shuffled quorum,
    // preventing a single compromised quorum from affecting all locks.

    namespace MasternodeQuorum
    {
        // Select at most `quorumSize` masternode IDs from `activeSet` using `seed`.
        // Returns the selected IDs in deterministic order (same result on every node).
        std::vector<Crypto::Hash> selectQuorum(
            const std::vector<Crypto::Hash> &activeSet,
            const Crypto::Hash &seed,
            uint64_t quorumSize);

        // Returns true if `mnId` is a member of `quorum`.
        bool isInQuorum(const Crypto::Hash &mnId, const std::vector<Crypto::Hash> &quorum);

        // Compute the sort key for a masternode in a given quorum: sha256(mnId || seed).
        Crypto::Hash quorumSortKey(const Crypto::Hash &mnId, const Crypto::Hash &seed);

    } // namespace MasternodeQuorum

} // namespace CryptoNote
