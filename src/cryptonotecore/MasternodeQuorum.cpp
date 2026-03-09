// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeQuorum.h"

#include <algorithm>
#include <crypto/hash.h>

namespace CryptoNote
{
    namespace MasternodeQuorum
    {
        Crypto::Hash quorumSortKey(const Crypto::Hash &mnId, const Crypto::Hash &seed)
        {
            // Concatenate mnId || seed and hash it.
            uint8_t buf[sizeof(Crypto::Hash) * 2];
            std::copy_n(mnId.data, sizeof(Crypto::Hash), buf);
            std::copy_n(seed.data, sizeof(Crypto::Hash), buf + sizeof(Crypto::Hash));
            return Crypto::cn_fast_hash(buf, sizeof(buf));
        }

        std::vector<Crypto::Hash> selectQuorum(
            const std::vector<Crypto::Hash> &activeSet,
            const Crypto::Hash &seed,
            uint64_t quorumSize)
        {
            if (activeSet.empty())
            {
                return {};
            }

            // Build (sortKey, mnId) pairs.
            std::vector<std::pair<Crypto::Hash, Crypto::Hash>> scored;
            scored.reserve(activeSet.size());
            for (const auto &mnId : activeSet)
            {
                scored.emplace_back(quorumSortKey(mnId, seed), mnId);
            }

            // Sort ascending by sort key (lexicographic on bytes).
            std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) {
                return std::lexicographical_compare(
                    std::begin(a.first.data), std::end(a.first.data),
                    std::begin(b.first.data), std::end(b.first.data));
            });

            const size_t count = std::min(static_cast<size_t>(quorumSize), scored.size());
            std::vector<Crypto::Hash> result;
            result.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                result.push_back(scored[i].second);
            }
            return result;
        }

        bool isInQuorum(const Crypto::Hash &mnId, const std::vector<Crypto::Hash> &quorum)
        {
            return std::find(quorum.begin(), quorum.end(), mnId) != quorum.end();
        }

    } // namespace MasternodeQuorum

} // namespace CryptoNote
