// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <cstdint>
#include <vector>

namespace CryptoNote
{
    // Outcome of feeding a ChainLock / InstantSend vote into the local collectors.
    // Only `Added` and `Assembled` votes should be relayed to peers; `Rejected` and
    // `Duplicate` votes must be dropped to prevent relay loops and amplification.
    enum class MasternodeVoteResult : uint8_t
    {
        Rejected = 0,  // invalid signature / unknown MN / not in quorum / out of window
        Duplicate = 1, // already known (or height/tx already locked)
        Added = 2,     // new valid vote stored, threshold not yet reached
        Assembled = 3  // new valid vote stored and it completed a lock
    };

    // Deterministic quorum selection shared by ChainLock and InstantSend.
    //
    // Algorithm:
    //   For each masternode ID in the active set, compute score = H(mnId || seed).
    //   Sort by score ascending, take the first `quorumSize` entries.
    //
    // The `seed` must be something the party producing the data being locked cannot choose:
    //   - For ChainLock at height H:   the hash of block H-1 (see Core::getChainLockQuorum)
    //   - For InstantSend:             the hash of the block at the start of the current
    //                                  INSTANTSEND_QUORUM_CYCLE_BLOCKS cycle
    //                                  (see Core::getInstantSendQuorum)
    //
    // Seeding with the voted block hash / tx hash itself would let an attacker grind that hash
    // until their own masternodes fill the quorum.

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
