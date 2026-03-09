// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <cstdint>
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
    // before calling these methods, which is consistent with how masternodeStateTracker is used.
    class ChainLockManager
    {
      public:
        // Build the signing preimage for a ChainLock vote: "CLV1" || height_LE4 || blockHash32
        static std::vector<uint8_t> buildVotePreimage(uint32_t height, const Crypto::Hash &blockHash);

        // Verify a ChainLock vote's signature.
        static bool verifyVote(const ChainLockVote &vote);

        // Add an incoming vote.
        // Returns true if this vote pushed the height to >= threshold and a ChainLock was assembled.
        // `threshold` is the minimum number of votes required.
        bool addVote(const ChainLockVote &vote, uint64_t threshold);

        // Returns true if a ChainLock exists for the given height.
        bool hasLock(uint32_t height) const;

        // Returns the ChainLock for the given height, if one exists.
        std::optional<ChainLock> getLock(uint32_t height) const;

        // Returns true if height is locked to a DIFFERENT block hash than `blockHash`.
        // Returns false if there is no lock, or the lock matches `blockHash`.
        bool isConflict(uint32_t height, const Crypto::Hash &blockHash) const;

        // Returns the highest locked height, or 0 if none.
        uint32_t highestLockedHeight() const;

        // Directly store an assembled ChainLock (received from a peer broadcast).
        // Validates all signatures before storing. Returns true if stored successfully.
        bool addChainLock(const ChainLock &cl, uint64_t threshold);

        // Serialize to JSON string for persistence.
        std::string toJson() const;

        // Deserialize from JSON string. Returns false on parse error.
        bool fromJson(const std::string &json);

        // Remove all pending votes for heights <= `height` that never reached threshold.
        void pruneBelow(uint32_t height);

      private:
        // Assembled (finalized) locks by height.
        std::map<uint32_t, ChainLock> m_locks;

        // Pending votes: height -> list of votes (before threshold is reached).
        std::map<uint32_t, std::vector<ChainLockVote>> m_pendingVotes;
    };

} // namespace CryptoNote
