// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "MasternodeQuorum.h"

#include <CryptoTypes.h>
#include <cstdint>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace CryptoNote
{
    // A single masternode's vote for a block at a given height.
    struct ChainLockVote
    {
        uint32_t height;
        Crypto::Hash blockHash;
        Crypto::Hash masternodeId;
        Crypto::PublicKey signingKey;
        Crypto::Signature signature; // sig(signingKey, "CLV1|height_LE4|blockHash32")
    };

    // An assembled ChainLock: >= threshold votes for the same (height, blockHash).
    struct ChainLock
    {
        uint32_t height;
        Crypto::Hash blockHash;
        std::vector<ChainLockVote> votes;
    };

    // Manages ChainLock vote collection, assembly, storage, and enforcement queries.
    //
    // Thread-safety: NOT internally synchronized. The caller (Core) must hold its own lock
    // before calling these methods.
    //
    // Trust model: this class only verifies signatures against the keys embedded in the votes.
    // Whether a vote's masternodeId / signingKey is a registered, active, in-quorum masternode
    // is the caller's responsibility (Core::addChainLockVote / Core::addChainLock) — the manager
    // must never be fed unvalidated peer data directly.
    class ChainLockManager
    {
      public:
        // Build the signing preimage for a ChainLock vote: "CLV1" || height_LE4 || blockHash32
        static std::vector<uint8_t> buildVotePreimage(uint32_t height, const Crypto::Hash &blockHash);

        // Verify a ChainLock vote's signature.
        static bool verifyVote(const ChainLockVote &vote);

        // Add an incoming (already membership-validated) vote.
        // `threshold` is the minimum number of votes required to assemble a lock.
        // `maxPendingPerHeight` bounds the number of pending votes kept for one height.
        // `now` is the wall-clock time used for the pending-lock liveness valve.
        MasternodeVoteResult
            addVote(const ChainLockVote &vote, uint64_t threshold, uint64_t maxPendingPerHeight, std::time_t now);

        // Returns true if a ChainLock exists for the given height.
        bool hasLock(uint32_t height) const;

        // Returns the ChainLock for the given height, if one exists.
        std::optional<ChainLock> getLock(uint32_t height) const;

        // Returns true if height is locked to a DIFFERENT block hash than `blockHash` AND the lock
        // is still enforceable: a lock whose block has never been seen locally stops being enforced
        // `pendingExpirySeconds` after it was received (liveness valve against bad/forged locks).
        // Returns false if there is no lock, the lock matches `blockHash`, or the lock expired.
        bool isConflict(
            uint32_t height,
            const Crypto::Hash &blockHash,
            std::time_t now,
            uint64_t pendingExpirySeconds) const;

        // Call when the block matching the lock at `height` has been accepted locally: the lock is
        // then backed by a real block and no longer subject to the pending expiry.
        void markLockSatisfied(uint32_t height);

        // Returns the highest locked height, or 0 if none.
        uint32_t highestLockedHeight() const;

        // Directly store an assembled ChainLock (received from a peer broadcast). The caller MUST
        // have validated quorum membership / distinctness of every vote already; this only checks
        // the signatures and the (height, blockHash) consistency.
        // Returns true if stored, false if already locked / malformed.
        bool addChainLock(const ChainLock &cl, uint64_t threshold, std::time_t now);

        // Serialize to JSON string for persistence.
        std::string toJson() const;

        // Deserialize from JSON string. Returns false on parse error. Every restored lock is treated
        // as "received now" for the pending-expiry valve.
        bool fromJson(const std::string &json, std::time_t now);

        // Remove all pending votes for heights < `height` that never reached threshold.
        void pruneBelow(uint32_t height);

        // Drop assembled locks for heights < `height` (locks are only enforced at the tip).
        void pruneLocksBelow(uint32_t height);

        // Drop locks (and pending votes) for heights > `height` (used on rewind).
        void removeAbove(uint32_t height);

      private:
        // Assembled (finalized) locks by height.
        std::map<uint32_t, ChainLock> m_locks;

        // Wall-clock receive time for locks whose block has not been seen locally yet.
        std::map<uint32_t, std::time_t> m_pendingLockReceivedAt;

        // Pending votes: height -> list of votes (before threshold is reached).
        std::map<uint32_t, std::vector<ChainLockVote>> m_pendingVotes;
    };

} // namespace CryptoNote
