// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ChainLockManager.h"

#include <algorithm>
#include <common/StringTools.h>
#include <crypto/crypto.h>
#include <json.hpp>

namespace CryptoNote
{
    std::vector<uint8_t> ChainLockManager::buildVotePreimage(uint32_t height, const Crypto::Hash &blockHash)
    {
        // "CLV1" || height (4 bytes LE) || blockHash (32 bytes)
        std::vector<uint8_t> preimage;
        preimage.reserve(4 + 4 + sizeof(Crypto::Hash));
        preimage.push_back('C');
        preimage.push_back('L');
        preimage.push_back('V');
        preimage.push_back('1');
        preimage.push_back(static_cast<uint8_t>(height & 0xff));
        preimage.push_back(static_cast<uint8_t>((height >> 8) & 0xff));
        preimage.push_back(static_cast<uint8_t>((height >> 16) & 0xff));
        preimage.push_back(static_cast<uint8_t>((height >> 24) & 0xff));
        preimage.insert(preimage.end(), blockHash.data, blockHash.data + sizeof(Crypto::Hash));
        return preimage;
    }

    bool ChainLockManager::verifyVote(const ChainLockVote &vote)
    {
        const auto preimage = buildVotePreimage(vote.height, vote.blockHash);
        const Crypto::Hash sigHash = Crypto::cn_fast_hash(preimage.data(), preimage.size());
        return Crypto::check_signature(sigHash, vote.signingKey, vote.signature);
    }

    MasternodeVoteResult ChainLockManager::addVote(
        const ChainLockVote &vote,
        uint64_t threshold,
        uint64_t maxPendingPerHeight,
        std::time_t now)
    {
        // Ignore if already locked at this height.
        if (m_locks.count(vote.height))
        {
            return MasternodeVoteResult::Duplicate;
        }

        auto &pending = m_pendingVotes[vote.height];

        // One vote per masternode per height. A second vote with a different block hash is
        // equivocation: it is ignored too (and must not be relayed), which also bounds the
        // pending set to at most one entry per quorum member.
        for (const auto &existing : pending)
        {
            if (existing.masternodeId == vote.masternodeId)
            {
                return MasternodeVoteResult::Duplicate;
            }
        }

        if (pending.size() >= maxPendingPerHeight)
        {
            return MasternodeVoteResult::Rejected;
        }

        // Signature check last: it is the expensive step.
        if (!verifyVote(vote))
        {
            if (pending.empty())
            {
                m_pendingVotes.erase(vote.height);
            }
            return MasternodeVoteResult::Rejected;
        }

        pending.push_back(vote);

        // Count votes for this blockHash.
        uint64_t count = 0;
        for (const auto &v : pending)
        {
            if (v.blockHash == vote.blockHash)
            {
                ++count;
            }
        }

        if (count >= threshold)
        {
            // Assemble ChainLock.
            ChainLock cl;
            cl.height = vote.height;
            cl.blockHash = vote.blockHash;
            for (const auto &v : pending)
            {
                if (v.blockHash == vote.blockHash)
                {
                    cl.votes.push_back(v);
                }
            }
            m_locks.emplace(vote.height, std::move(cl));
            m_pendingLockReceivedAt[vote.height] = now;
            m_pendingVotes.erase(vote.height);
            return MasternodeVoteResult::Assembled;
        }

        return MasternodeVoteResult::Added;
    }

    bool ChainLockManager::hasLock(uint32_t height) const
    {
        return m_locks.count(height) != 0;
    }

    std::optional<ChainLock> ChainLockManager::getLock(uint32_t height) const
    {
        const auto it = m_locks.find(height);
        if (it == m_locks.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    bool ChainLockManager::isConflict(
        uint32_t height,
        const Crypto::Hash &blockHash,
        std::time_t now,
        uint64_t pendingExpirySeconds) const
    {
        const auto it = m_locks.find(height);
        if (it == m_locks.end())
        {
            return false;
        }

        if (it->second.blockHash == blockHash)
        {
            return false;
        }

        const auto pendingIt = m_pendingLockReceivedAt.find(height);
        if (pendingIt != m_pendingLockReceivedAt.end())
        {
            if (now >= pendingIt->second
                && static_cast<uint64_t>(now - pendingIt->second) > pendingExpirySeconds)
            {
                // Lock has been pending (block never seen) for too long: advisory only.
                return false;
            }
        }

        return true;
    }

    void ChainLockManager::markLockSatisfied(uint32_t height)
    {
        m_pendingLockReceivedAt.erase(height);
    }

    uint32_t ChainLockManager::highestLockedHeight() const
    {
        if (m_locks.empty())
        {
            return 0;
        }
        return m_locks.rbegin()->first;
    }

    bool ChainLockManager::addChainLock(const ChainLock &cl, uint64_t threshold, std::time_t now)
    {
        if (m_locks.count(cl.height))
        {
            return false; // already locked
        }

        if (cl.votes.size() < threshold)
        {
            return false;
        }

        // Verify all votes refer to the same (height, blockHash), come from distinct masternodes
        // and have valid signatures.
        std::vector<Crypto::Hash> seen;
        seen.reserve(cl.votes.size());
        for (const auto &vote : cl.votes)
        {
            if (vote.height != cl.height || vote.blockHash != cl.blockHash)
            {
                return false;
            }
            if (std::find(seen.begin(), seen.end(), vote.masternodeId) != seen.end())
            {
                return false;
            }
            seen.push_back(vote.masternodeId);
            if (!verifyVote(vote))
            {
                return false;
            }
        }

        m_locks.emplace(cl.height, cl);
        m_pendingLockReceivedAt[cl.height] = now;
        m_pendingVotes.erase(cl.height);
        return true;
    }

    void ChainLockManager::pruneBelow(uint32_t height)
    {
        auto it = m_pendingVotes.begin();
        while (it != m_pendingVotes.end() && it->first < height)
        {
            it = m_pendingVotes.erase(it);
        }
    }

    void ChainLockManager::pruneLocksBelow(uint32_t height)
    {
        m_locks.erase(m_locks.begin(), m_locks.lower_bound(height));
        m_pendingLockReceivedAt.erase(m_pendingLockReceivedAt.begin(), m_pendingLockReceivedAt.lower_bound(height));
    }

    void ChainLockManager::removeAbove(uint32_t height)
    {
        m_locks.erase(m_locks.upper_bound(height), m_locks.end());
        m_pendingLockReceivedAt.erase(m_pendingLockReceivedAt.upper_bound(height), m_pendingLockReceivedAt.end());
        m_pendingVotes.erase(m_pendingVotes.upper_bound(height), m_pendingVotes.end());
    }

    std::string ChainLockManager::toJson() const
    {
        nlohmann::json root = nlohmann::json::array();
        for (const auto &[h, cl] : m_locks)
        {
            nlohmann::json item;
            item["height"] = cl.height;
            item["block_hash"] = Common::podToHex(cl.blockHash);
            nlohmann::json votes = nlohmann::json::array();
            for (const auto &v : cl.votes)
            {
                votes.push_back({
                    {"mn_id", Common::podToHex(v.masternodeId)},
                    {"signing_key", Common::podToHex(v.signingKey)},
                    {"sig", Common::podToHex(v.signature)}
                });
            }
            item["votes"] = std::move(votes);
            root.push_back(std::move(item));
        }
        return root.dump();
    }

    bool ChainLockManager::fromJson(const std::string &json, std::time_t now)
    {
        try
        {
            const auto root = nlohmann::json::parse(json);
            if (!root.is_array())
            {
                return false;
            }

            std::map<uint32_t, ChainLock> loaded;
            for (const auto &item : root)
            {
                ChainLock cl;
                cl.height = item.at("height").get<uint32_t>();
                if (!Common::podFromHex(item.at("block_hash").get<std::string>(), cl.blockHash))
                {
                    return false;
                }

                for (const auto &v : item.at("votes"))
                {
                    ChainLockVote vote;
                    vote.height = cl.height;
                    vote.blockHash = cl.blockHash;
                    if (!Common::podFromHex(v.at("mn_id").get<std::string>(), vote.masternodeId)
                        || !Common::podFromHex(v.at("signing_key").get<std::string>(), vote.signingKey)
                        || !Common::podFromHex(v.at("sig").get<std::string>(), vote.signature))
                    {
                        return false;
                    }
                    cl.votes.push_back(vote);
                }

                loaded.emplace(cl.height, std::move(cl));
            }

            m_locks = std::move(loaded);
            m_pendingLockReceivedAt.clear();
            for (const auto &[h, _] : m_locks)
            {
                m_pendingLockReceivedAt[h] = now;
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

} // namespace CryptoNote
