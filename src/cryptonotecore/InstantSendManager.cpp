// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "InstantSendManager.h"

#include <algorithm>
#include <common/StringTools.h>
#include <crypto/crypto.h>
#include <crypto/hash.h>
#include "json.hpp"

namespace CryptoNote
{
    std::vector<uint8_t> InstantSendManager::buildVotePreimage(const Crypto::Hash &txHash)
    {
        // "ISV1" || txHash32
        std::vector<uint8_t> preimage;
        preimage.reserve(4 + sizeof(Crypto::Hash));
        preimage.push_back('I');
        preimage.push_back('S');
        preimage.push_back('V');
        preimage.push_back('1');
        preimage.insert(preimage.end(), txHash.data, txHash.data + sizeof(Crypto::Hash));
        return preimage;
    }

    bool InstantSendManager::verifyVote(const InstantSendVote &vote)
    {
        const auto preimage = buildVotePreimage(vote.txHash);
        const Crypto::Hash sigHash = Crypto::cn_fast_hash(preimage.data(), preimage.size());
        return Crypto::check_signature(sigHash, vote.signingKey, vote.signature);
    }

    bool InstantSendManager::addVote(
        const InstantSendVote &vote,
        const std::vector<Crypto::KeyImage> &keyImages,
        uint64_t threshold,
        uint32_t currentHeight)
    {
        // If any key image is already locked to a different tx, reject.
        for (const auto &ki : keyImages)
        {
            if (isConflict(ki, vote.txHash))
            {
                return false;
            }
        }

        if (!verifyVote(vote))
        {
            return false;
        }

        auto &pending = m_pendingVotes[vote.txHash];

        // Deduplicate: one vote per masternodeId per txHash.
        for (const auto &existing : pending)
        {
            if (existing.masternodeId == vote.masternodeId)
            {
                return false;
            }
        }

        pending.push_back(vote);

        // Record the key images for this tx if not already tracked.
        if (m_pendingKeyImages.find(vote.txHash) == m_pendingKeyImages.end())
        {
            m_pendingKeyImages[vote.txHash] = keyImages;
            m_lockHeight[vote.txHash] = currentHeight;
        }

        if (pending.size() >= threshold)
        {
            InstantSendLock lock;
            lock.txHash = vote.txHash;
            lock.keyImages = keyImages;
            lock.votes = pending;
            lock.lockedAtHeight = currentHeight;
            storeLock(lock);
            m_pendingVotes.erase(vote.txHash);
            m_pendingKeyImages.erase(vote.txHash);
            m_lockHeight.erase(vote.txHash);
            return true;
        }

        return false;
    }

    bool InstantSendManager::isLocked(const Crypto::KeyImage &keyImage) const
    {
        return m_locks.count(keyImage) != 0;
    }

    bool InstantSendManager::isConflict(const Crypto::KeyImage &keyImage, const Crypto::Hash &txHash) const
    {
        const auto it = m_locks.find(keyImage);
        if (it == m_locks.end())
        {
            return false;
        }
        return it->second.txHash != txHash;
    }

    std::optional<InstantSendLock> InstantSendManager::getLock(const Crypto::KeyImage &keyImage) const
    {
        const auto it = m_locks.find(keyImage);
        if (it == m_locks.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    bool InstantSendManager::addInstantSendLock(const InstantSendLock &lock, uint64_t threshold)
    {
        if (lock.votes.size() < threshold)
        {
            return false;
        }

        // Verify all votes.
        for (const auto &vote : lock.votes)
        {
            if (vote.txHash != lock.txHash)
            {
                return false;
            }
            if (!verifyVote(vote))
            {
                return false;
            }
        }

        // Check for conflicts with existing locks.
        for (const auto &ki : lock.keyImages)
        {
            if (isConflict(ki, lock.txHash))
            {
                return false;
            }
        }

        storeLock(lock);
        m_pendingVotes.erase(lock.txHash);
        m_pendingKeyImages.erase(lock.txHash);
        m_lockHeight.erase(lock.txHash);
        return true;
    }

    void InstantSendManager::onTxConfirmed(const Crypto::Hash &txHash)
    {
        // Find any lock associated with this txHash and remove it.
        auto it = m_locks.begin();
        while (it != m_locks.end())
        {
            if (it->second.txHash == txHash)
            {
                it = m_locks.erase(it);
            }
            else
            {
                ++it;
            }
        }
        m_pendingVotes.erase(txHash);
        m_pendingKeyImages.erase(txHash);
        m_lockHeight.erase(txHash);
    }

    void InstantSendManager::pruneExpired(uint32_t currentHeight, uint64_t expiryBlocks)
    {
        // Prune finalized locks whose tx was not confirmed within expiryBlocks.
        std::vector<Crypto::Hash> expiredTxHashes;
        for (const auto &[ki, lock] : m_locks)
        {
            if (currentHeight > lock.lockedAtHeight + expiryBlocks)
            {
                expiredTxHashes.push_back(lock.txHash);
            }
        }
        for (const auto &txHash : expiredTxHashes)
        {
            onTxConfirmed(txHash); // reuse cleanup logic
        }

        // Prune pending votes that have aged out.
        auto it = m_lockHeight.begin();
        while (it != m_lockHeight.end())
        {
            if (currentHeight > it->second + expiryBlocks)
            {
                m_pendingVotes.erase(it->first);
                m_pendingKeyImages.erase(it->first);
                it = m_lockHeight.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void InstantSendManager::storeLock(const InstantSendLock &lock)
    {
        for (const auto &ki : lock.keyImages)
        {
            m_locks[ki] = lock;
        }
    }

    std::string InstantSendManager::toJson() const
    {
        // Deduplicate locks (multiple keyImages may point to the same lock).
        std::unordered_map<Crypto::Hash, const InstantSendLock *> uniqueLocks;
        for (const auto &[ki, lock] : m_locks)
        {
            uniqueLocks[lock.txHash] = &lock;
        }

        nlohmann::json arr = nlohmann::json::array();
        for (const auto &[txHash, lockPtr] : uniqueLocks)
        {
            nlohmann::json jLock;
            jLock["tx_hash"] = Common::podToHex(lockPtr->txHash);
            jLock["locked_at_height"] = lockPtr->lockedAtHeight;

            nlohmann::json kiArr = nlohmann::json::array();
            for (const auto &ki : lockPtr->keyImages)
            {
                kiArr.push_back(Common::podToHex(ki));
            }
            jLock["key_images"] = kiArr;

            nlohmann::json voteArr = nlohmann::json::array();
            for (const auto &vote : lockPtr->votes)
            {
                nlohmann::json jVote;
                jVote["tx_hash"] = Common::podToHex(vote.txHash);
                jVote["mn_id"] = Common::podToHex(vote.masternodeId);
                jVote["signing_key"] = Common::podToHex(vote.signingKey);
                jVote["signature"] = Common::podToHex(vote.signature);
                voteArr.push_back(jVote);
            }
            jLock["votes"] = voteArr;
            arr.push_back(jLock);
        }

        return arr.dump();
    }

    bool InstantSendManager::fromJson(const std::string &json)
    {
        nlohmann::json arr;
        try
        {
            arr = nlohmann::json::parse(json);
        }
        catch (const std::exception &)
        {
            return false;
        }

        if (!arr.is_array())
        {
            return false;
        }

        // Clear existing finalized locks (pending state is not persisted).
        m_locks.clear();

        for (const auto &jLock : arr)
        {
            InstantSendLock lock;
            if (!Common::podFromHex(jLock.at("tx_hash").get<std::string>(), lock.txHash))
            {
                return false;
            }
            lock.lockedAtHeight = jLock.at("locked_at_height").get<uint32_t>();

            for (const auto &kiHex : jLock.at("key_images"))
            {
                Crypto::KeyImage ki;
                if (!Common::podFromHex(kiHex.get<std::string>(), ki))
                {
                    return false;
                }
                lock.keyImages.push_back(ki);
            }

            for (const auto &jVote : jLock.at("votes"))
            {
                InstantSendVote vote;
                if (!Common::podFromHex(jVote.at("tx_hash").get<std::string>(), vote.txHash)
                    || !Common::podFromHex(jVote.at("mn_id").get<std::string>(), vote.masternodeId)
                    || !Common::podFromHex(jVote.at("signing_key").get<std::string>(), vote.signingKey)
                    || !Common::podFromHex(jVote.at("signature").get<std::string>(), vote.signature))
                {
                    return false;
                }
                lock.votes.push_back(vote);
            }

            storeLock(lock);
        }

        return true;
    }

} // namespace CryptoNote
