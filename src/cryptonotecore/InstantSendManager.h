// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "MasternodeQuorum.h"

#include <CryptoTypes.h>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CryptoNote
{
    // A single masternode's vote to IS-lock a transaction.
    struct InstantSendVote
    {
        Crypto::Hash txHash;
        Crypto::Hash masternodeId;
        Crypto::PublicKey signingKey;
        Crypto::Signature signature; // sig(signingKey, "ISV1|txHash32")
    };

    // An assembled InstantSend lock: >= threshold votes for txHash, covering all its key images.
    struct InstantSendLock
    {
        Crypto::Hash txHash;
        std::vector<Crypto::KeyImage> keyImages; // inputs being locked
        std::vector<InstantSendVote> votes;
        uint32_t lockedAtHeight; // block height when the lock was assembled
    };

    // Manages InstantSend vote collection, lock assembly, and double-spend detection.
    //
    // Thread-safety: NOT internally synchronized (same as ChainLockManager).
    //
    // Trust model: only signatures against the embedded keys are verified here. Masternode
    // membership / quorum checks are the caller's (Core) responsibility.
    class InstantSendManager
    {
      public:
        // Build the signing preimage: "ISV1" || txHash32
        static std::vector<uint8_t> buildVotePreimage(const Crypto::Hash &txHash);

        // Verify an InstantSend vote's signature.
        static bool verifyVote(const InstantSendVote &vote);

        // Add an incoming (already membership-validated) vote.
        // `keyImages` is the list of input key images in the transaction (must be non-empty).
        // `threshold` is the minimum votes required.
        // `currentHeight` is the current blockchain tip height.
        MasternodeVoteResult addVote(
            const InstantSendVote &vote,
            const std::vector<Crypto::KeyImage> &keyImages,
            uint64_t threshold,
            uint32_t currentHeight);

        // Returns true if `keyImage` is IS-locked (to any tx).
        bool isLocked(const Crypto::KeyImage &keyImage) const;

        // Returns true if `keyImage` is locked to a DIFFERENT tx than `txHash`.
        bool isConflict(const Crypto::KeyImage &keyImage, const Crypto::Hash &txHash) const;

        // Returns the IS lock for the given key image, if one exists.
        std::optional<InstantSendLock> getLock(const Crypto::KeyImage &keyImage) const;

        // Returns the IS lock for the given transaction hash, if one exists.
        std::optional<InstantSendLock> getLockByTxHash(const Crypto::Hash &txHash) const;

        // Directly store an assembled lock (received from a peer broadcast). The caller MUST have
        // validated quorum membership of every vote already. Validates signatures, distinctness,
        // non-empty key images and conflicts. Returns true if stored (false if malformed,
        // conflicting, or already known).
        bool addInstantSendLock(const InstantSendLock &lock, uint64_t threshold);

        // Clear IS locks for a confirmed transaction.
        void onTxConfirmed(const Crypto::Hash &txHash);

        // A block spent `keyImage` with some transaction: drop whatever lock covered it (the chain
        // is authoritative; a local IS lock must never make a PoW-valid block unacceptable).
        void onKeyImageSpent(const Crypto::KeyImage &keyImage);

        // Remove locks and pending votes for txs that have not confirmed within expiry.
        void pruneExpired(uint32_t currentHeight, uint64_t expiryBlocks);

        // Serialize finalized locks to JSON string for persistence.
        std::string toJson() const;

        // Deserialize finalized locks from JSON string. Returns false on parse error.
        bool fromJson(const std::string &json);

      private:
        // Finalized locks: keyImage -> InstantSendLock
        // Multiple keyImages may point to the same lock (one lock covers all inputs).
        std::unordered_map<Crypto::KeyImage, InstantSendLock> m_locks;

        // Pending votes before threshold: txHash -> votes
        std::unordered_map<Crypto::Hash, std::vector<InstantSendVote>> m_pendingVotes;

        // Track which keyImages belong to which pending txHash (so we can check conflicts before lock).
        std::unordered_map<Crypto::Hash, std::vector<Crypto::KeyImage>> m_pendingKeyImages;

        // Pending lock height tracking for expiry: txHash -> lockedAtHeight
        std::unordered_map<Crypto::Hash, uint32_t> m_lockHeight;

        void storeLock(const InstantSendLock &lock);
    };

} // namespace CryptoNote
