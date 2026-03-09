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

    bool ChainLockManager::addVote(const ChainLockVote &vote, uint64_t threshold)
    {
        // Ignore if already locked at this height.
        if (m_locks.count(vote.height))
        {
            return false;
        }

        if (!verifyVote(vote))
        {
            return false;
        }

        auto &pending = m_pendingVotes[vote.height];

        // Deduplicate: one vote per masternodeId per (height, blockHash).
        for (const auto &existing : pending)
        {
            if (existing.masternodeId == vote.masternodeId && existing.blockHash == vote.blockHash)
            {
                return false; // duplicate
            }
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
            m_pendingVotes.erase(vote.height);
            return true;
        }

        return false;
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

    bool ChainLockManager::isConflict(uint32_t height, const Crypto::Hash &blockHash) const
    {
        const auto it = m_locks.find(height);
        if (it == m_locks.end())
        {
            return false;
        }
        return it->second.blockHash != blockHash;
    }

    uint32_t ChainLockManager::highestLockedHeight() const
    {
        if (m_locks.empty())
        {
            return 0;
        }
        return m_locks.rbegin()->first;
    }

    bool ChainLockManager::addChainLock(const ChainLock &cl, uint64_t threshold)
    {
        if (m_locks.count(cl.height))
        {
            return false; // already locked
        }

        if (cl.votes.size() < threshold)
        {
            return false;
        }

        // Verify all votes refer to the same (height, blockHash) and have valid sigs.
        for (const auto &vote : cl.votes)
        {
            if (vote.height != cl.height || vote.blockHash != cl.blockHash)
            {
                return false;
            }
            if (!verifyVote(vote))
            {
                return false;
            }
        }

        m_locks.emplace(cl.height, cl);
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

    bool ChainLockManager::fromJson(const std::string &json)
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
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

} // namespace CryptoNote
