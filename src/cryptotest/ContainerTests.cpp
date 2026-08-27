// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ContainerTests.h"

#include "CryptoTypes.h"
#include "common/StringTools.h"

#include <algorithm>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

namespace ContainerTests
{
    namespace
    {
        /* ------------------------------------------------------------------
           A stand-in for TransactionPool::PendingTransactionInfo carrying only
           the fields the indices key on. The real type is a private nested
           class holding a CachedTransaction, which cannot be constructed
           cheaply here; the point of this test is the container behaviour, not
           the transaction contents.
           ------------------------------------------------------------------ */
        struct TestTx
        {
            uint64_t receiveTime;

            Crypto::Hash txHash;

            std::optional<Crypto::Hash> paymentId;

            uint64_t fee;

            uint64_t size;

            const Crypto::Hash &getTransactionHash() const
            {
                return txHash;
            }
        };

        /* Mirrors TransactionPriorityComparator: fee per byte descending, then
           size ascending, then receive time ascending. Written as a strict weak
           ordering - equivalent elements return false, which is the property
           the real comparator was missing. */
        struct TestPriorityComparator
        {
            bool operator()(const TestTx &lhs, const TestTx &rhs) const
            {
                /* fee/size comparison rearranged to avoid division */
                const uint64_t left = lhs.fee * rhs.size;
                const uint64_t right = rhs.fee * lhs.size;

                if (left != right)
                {
                    return left > right;
                }

                if (lhs.size != rhs.size)
                {
                    return lhs.size < rhs.size;
                }

                if (lhs.receiveTime != rhs.receiveTime)
                {
                    return lhs.receiveTime < rhs.receiveTime;
                }

                return false;
            }
        };

        /* Mirrors TransactionPool::PaymentIdHasher. */
        struct TestPaymentIdHasher
        {
            size_t operator()(const std::optional<Crypto::Hash> &paymentId) const
            {
                if (!paymentId)
                {
                    return std::numeric_limits<size_t>::max();
                }

                return std::hash<Crypto::Hash> {}(*paymentId);
            }
        };

        struct HashTag
        {
        };
        struct CostTag
        {
        };
        struct PaymentIdTag
        {
        };

        /* The original container, index for index. */
        typedef boost::multi_index_container<
            TestTx,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<HashTag>,
                    boost::multi_index::
                        const_mem_fun<TestTx, const Crypto::Hash &, &TestTx::getTransactionHash>>,
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<CostTag>,
                    boost::multi_index::identity<TestTx>,
                    TestPriorityComparator>,
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<PaymentIdTag>,
                    BOOST_MULTI_INDEX_MEMBER(TestTx, std::optional<Crypto::Hash>, paymentId),
                    TestPaymentIdHasher>>>
            BoostContainer;

        /* ------------------------------------------------------------------
           The replacement shape.

           Elements live in a list so that the index maps can hold iterators
           that stay valid across insert and erase. The priority ordering is
           materialised on demand with a stable sort rather than maintained
           live: the list is in insertion order, and a stable sort therefore
           breaks ties by insertion order - which is what
           ordered_non_unique does, since its insert places an element at the
           upper bound of its equivalent range.
           ------------------------------------------------------------------ */
        class ReplacementContainer
        {
          public:
            bool insert(TestTx tx)
            {
                if (m_byHash.count(tx.getTransactionHash()) > 0)
                {
                    return false;
                }

                const Crypto::Hash hash = tx.getTransactionHash();
                const std::optional<Crypto::Hash> paymentId = tx.paymentId;

                const auto it = m_items.insert(m_items.end(), std::move(tx));

                m_byHash.emplace(hash, it);

                if (paymentId)
                {
                    m_byPaymentId.emplace(*paymentId, it);
                }

                return true;
            }

            const TestTx *find(const Crypto::Hash &hash) const
            {
                const auto it = m_byHash.find(hash);

                return it == m_byHash.end() ? nullptr : &*it->second;
            }

            bool erase(const Crypto::Hash &hash)
            {
                const auto it = m_byHash.find(hash);

                if (it == m_byHash.end())
                {
                    return false;
                }

                const auto item = it->second;

                if (item->paymentId)
                {
                    auto range = m_byPaymentId.equal_range(*item->paymentId);

                    for (auto pit = range.first; pit != range.second; ++pit)
                    {
                        if (pit->second == item)
                        {
                            m_byPaymentId.erase(pit);
                            break;
                        }
                    }
                }

                m_items.erase(item);
                m_byHash.erase(it);

                return true;
            }

            size_t size() const
            {
                return m_byHash.size();
            }

            std::vector<const TestTx *> byPriority() const
            {
                std::vector<const TestTx *> ordered;
                ordered.reserve(m_items.size());

                for (const auto &item : m_items)
                {
                    ordered.push_back(&item);
                }

                std::stable_sort(ordered.begin(), ordered.end(), [](const TestTx *a, const TestTx *b) {
                    return TestPriorityComparator {}(*a, *b);
                });

                return ordered;
            }

            std::vector<Crypto::Hash> hashesByPaymentId(const Crypto::Hash &paymentId) const
            {
                std::vector<Crypto::Hash> hashes;

                auto range = m_byPaymentId.equal_range(paymentId);

                for (auto it = range.first; it != range.second; ++it)
                {
                    hashes.push_back(it->second->getTransactionHash());
                }

                return hashes;
            }

          private:
            typedef std::list<TestTx> Items;

            Items m_items;

            std::unordered_map<Crypto::Hash, Items::iterator> m_byHash;

            std::unordered_multimap<Crypto::Hash, Items::iterator> m_byPaymentId;
        };

        void fail(const std::string &what, uint64_t seed, uint64_t step)
        {
            std::cout << std::endl
                      << "Container differential test FAILED at step " << step << " (seed " << seed << ")" << std::endl
                      << "  " << what << std::endl
                      << "Terminating." << std::endl;

            exit(1);
        }

        Crypto::Hash makeHash(std::mt19937_64 &rng)
        {
            Crypto::Hash hash {};

            for (size_t i = 0; i < sizeof(hash.data); i++)
            {
                hash.data[i] = static_cast<uint8_t>(rng() & 0xff);
            }

            return hash;
        }
    } // namespace

    void testTransactionPoolContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::transactionPool (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        BoostContainer original;
        ReplacementContainer replacement;

        /* A small pool of payment ids so the non-unique index actually holds
           several entries per key. */
        std::vector<Crypto::Hash> paymentIds;
        for (int i = 0; i < 4; i++)
        {
            paymentIds.push_back(makeHash(rng));
        }

        std::vector<Crypto::Hash> live;

        for (uint64_t step = 0; step < iterations; step++)
        {
            const uint64_t roll = rng() % 100;

            if (roll < 65 || live.empty())
            {
                TestTx tx {};

                /* Deliberately tiny ranges so that ties on every criterion are
                   common - tie ordering is the property most likely to differ
                   between a live ordered index and an on-demand sort. */
                tx.receiveTime = rng() % 3;
                tx.fee = 1 + rng() % 3;
                tx.size = 1 + rng() % 3;

                /* Occasionally reuse an existing hash to exercise the unique
                   constraint on the primary index. */
                if (!live.empty() && (rng() % 10) == 0)
                {
                    tx.txHash = live[rng() % live.size()];
                }
                else
                {
                    tx.txHash = makeHash(rng);
                }

                if ((rng() % 4) != 0)
                {
                    tx.paymentId = paymentIds[rng() % paymentIds.size()];
                }

                const bool insertedOriginal = original.insert(tx).second;
                const bool insertedReplacement = replacement.insert(tx);

                if (insertedOriginal != insertedReplacement)
                {
                    fail("insert() disagreed on whether the element was new", seed, step);
                }

                if (insertedOriginal)
                {
                    live.push_back(tx.txHash);
                }
            }
            else
            {
                const size_t victim = rng() % live.size();
                const Crypto::Hash hash = live[victim];

                const bool erasedOriginal = original.get<HashTag>().erase(hash) > 0;
                const bool erasedReplacement = replacement.erase(hash);

                if (erasedOriginal != erasedReplacement)
                {
                    fail("erase() disagreed on whether the element was present", seed, step);
                }

                live.erase(live.begin() + victim);
            }

            /* ---- compare full observable state ---- */

            if (original.size() != replacement.size())
            {
                fail("size() diverged", seed, step);
            }

            /* Ordered index: the sequence is fully specified, including ties,
               so compare it element for element. */
            {
                std::vector<Crypto::Hash> originalOrder;

                for (const auto &tx : original.get<CostTag>())
                {
                    originalOrder.push_back(tx.getTransactionHash());
                }

                std::vector<Crypto::Hash> replacementOrder;

                for (const TestTx *tx : replacement.byPriority())
                {
                    replacementOrder.push_back(tx->getTransactionHash());
                }

                if (originalOrder != replacementOrder)
                {
                    fail("priority ordering diverged", seed, step);
                }
            }

            /* Hash index lookups, for a present and an absent key. */
            for (const Crypto::Hash &hash : live)
            {
                const auto originalIt = original.get<HashTag>().find(hash);
                const TestTx *replacementTx = replacement.find(hash);

                if ((originalIt == original.get<HashTag>().end()) != (replacementTx == nullptr))
                {
                    fail("find() disagreed on presence", seed, step);
                }

                if (replacementTx != nullptr && originalIt->receiveTime != replacementTx->receiveTime)
                {
                    fail("find() returned a different element", seed, step);
                }
            }

            /* Payment id index. A hashed_non_unique index does not specify the
               order within an equal_range, so compare these as sets. */
            for (const Crypto::Hash &paymentId : paymentIds)
            {
                std::vector<Crypto::Hash> originalHashes;

                auto range = original.get<PaymentIdTag>().equal_range(std::optional<Crypto::Hash>(paymentId));

                for (auto it = range.first; it != range.second; ++it)
                {
                    originalHashes.push_back(it->getTransactionHash());
                }

                std::vector<Crypto::Hash> replacementHashes = replacement.hashesByPaymentId(paymentId);

                const auto byBytes = [](const Crypto::Hash &a, const Crypto::Hash &b) {
                    return std::memcmp(a.data, b.data, sizeof(a.data)) < 0;
                };

                std::sort(originalHashes.begin(), originalHashes.end(), byBytes);
                std::sort(replacementHashes.begin(), replacementHashes.end(), byBytes);

                if (originalHashes != replacementHashes)
                {
                    fail("payment id index diverged", seed, step);
                }
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::spentKeyImages
       ---------------------------------------------------------------------- */
    namespace
    {
        struct SpentKeyImageEntry
        {
            uint32_t blockIndex;

            Crypto::KeyImage keyImage;
        };

        struct BlockIndexTag
        {
        };
        struct KeyImageTag
        {
        };

        typedef boost::multi_index_container<
            SpentKeyImageEntry,
            boost::multi_index::indexed_by<
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<BlockIndexTag>,
                    BOOST_MULTI_INDEX_MEMBER(SpentKeyImageEntry, uint32_t, blockIndex)>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<KeyImageTag>,
                    BOOST_MULTI_INDEX_MEMBER(SpentKeyImageEntry, Crypto::KeyImage, keyImage)>>>
            BoostSpentKeyImages;

        /* An ordered multimap keyed on block index carries the ordered index;
           a hash map carries the unique key image index. std::multimap and
           ordered_non_unique agree on the placement of equivalent keys (both
           insert at the upper bound), so iteration order matches including
           blocks that spend several key images. */
        class ReplacementSpentKeyImages
        {
          public:
            bool insert(uint32_t blockIndex, const Crypto::KeyImage &keyImage)
            {
                /* The key image index is unique, so a duplicate is rejected
                   however it is inserted. */
                if (m_byKeyImage.count(keyImage) > 0)
                {
                    return false;
                }

                m_byBlockIndex.emplace(blockIndex, keyImage);
                m_byKeyImage.emplace(keyImage, blockIndex);

                return true;
            }

            bool contains(const Crypto::KeyImage &keyImage) const
            {
                return m_byKeyImage.count(keyImage) > 0;
            }

            size_t size() const
            {
                return m_byKeyImage.size();
            }

            /* Moves every entry at or above splitBlockIndex into other, which
               is what splitSpentKeyImages does with lower_bound plus a range
               insert and a range erase. */
            void splitInto(ReplacementSpentKeyImages &other, uint32_t splitBlockIndex)
            {
                auto lowerBound = m_byBlockIndex.lower_bound(splitBlockIndex);

                for (auto it = lowerBound; it != m_byBlockIndex.end(); ++it)
                {
                    other.insert(it->first, it->second);
                    m_byKeyImage.erase(it->second);
                }

                m_byBlockIndex.erase(lowerBound, m_byBlockIndex.end());
            }

            std::vector<SpentKeyImageEntry> inBlockIndexOrder() const
            {
                std::vector<SpentKeyImageEntry> entries;
                entries.reserve(m_byBlockIndex.size());

                for (const auto &entry : m_byBlockIndex)
                {
                    entries.push_back(SpentKeyImageEntry {entry.first, entry.second});
                }

                return entries;
            }

          private:
            std::multimap<uint32_t, Crypto::KeyImage> m_byBlockIndex;

            std::unordered_map<Crypto::KeyImage, uint32_t> m_byKeyImage;
        };

        Crypto::KeyImage makeKeyImage(std::mt19937_64 &rng)
        {
            Crypto::KeyImage keyImage {};

            for (size_t i = 0; i < sizeof(keyImage.data); i++)
            {
                keyImage.data[i] = static_cast<uint8_t>(rng() & 0xff);
            }

            return keyImage;
        }

        void compareSpentKeyImages(
            const BoostSpentKeyImages &original,
            const ReplacementSpentKeyImages &replacement,
            const std::string &what,
            uint64_t seed,
            uint64_t step)
        {
            if (original.size() != replacement.size())
            {
                fail(what + ": size() diverged", seed, step);
            }

            std::vector<std::pair<uint32_t, Crypto::KeyImage>> originalOrder;

            for (const auto &entry : original.get<BlockIndexTag>())
            {
                originalOrder.emplace_back(entry.blockIndex, entry.keyImage);
            }

            std::vector<std::pair<uint32_t, Crypto::KeyImage>> replacementOrder;

            for (const auto &entry : replacement.inBlockIndexOrder())
            {
                replacementOrder.emplace_back(entry.blockIndex, entry.keyImage);
            }

            if (originalOrder.size() != replacementOrder.size())
            {
                fail(what + ": block index ordering length diverged", seed, step);
            }

            for (size_t i = 0; i < originalOrder.size(); i++)
            {
                if (originalOrder[i].first != replacementOrder[i].first
                    || !(originalOrder[i].second == replacementOrder[i].second))
                {
                    fail(what + ": block index ordering diverged", seed, step);
                }
            }
        }
    } // namespace

    void testSpentKeyImagesContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::spentKeyImages (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        BoostSpentKeyImages original;
        ReplacementSpentKeyImages replacement;

        std::vector<Crypto::KeyImage> live;

        for (uint64_t step = 0; step < iterations; step++)
        {
            const uint64_t roll = rng() % 100;

            if (roll < 80 || live.empty())
            {
                /* A small block index range so several key images share a
                   block, which is what the ordered index has to order. */
                const uint32_t blockIndex = static_cast<uint32_t>(rng() % 8);

                Crypto::KeyImage keyImage;

                /* Occasionally reuse a key image to exercise the unique index. */
                if (!live.empty() && (rng() % 10) == 0)
                {
                    keyImage = live[rng() % live.size()];
                }
                else
                {
                    keyImage = makeKeyImage(rng);
                }

                const bool insertedOriginal =
                    original.get<BlockIndexTag>().insert(SpentKeyImageEntry {blockIndex, keyImage}).second;
                const bool insertedReplacement = replacement.insert(blockIndex, keyImage);

                if (insertedOriginal != insertedReplacement)
                {
                    fail("insert() disagreed on whether the key image was new", seed, step);
                }

                if (insertedOriginal)
                {
                    live.push_back(keyImage);
                }
            }
            else
            {
                /* The segment split: everything at or above the pivot moves. */
                const uint32_t pivot = static_cast<uint32_t>(rng() % 8);

                BoostSpentKeyImages originalUpper;
                ReplacementSpentKeyImages replacementUpper;

                /* Seed the target with key images already live in the
                   source, so the split has to skip duplicates on the
                   receiving side rather than desync its two maps. */
                if (!live.empty())
                {
                    const size_t seedCount = 1 + (rng() % 2);

                    for (size_t i = 0; i < seedCount; i++)
                    {
                        const Crypto::KeyImage seedImage = live[rng() % live.size()];
                        const uint32_t seedBlock = static_cast<uint32_t>(rng() % 8);

                        originalUpper.get<BlockIndexTag>().insert(
                            SpentKeyImageEntry {seedBlock, seedImage});
                        replacementUpper.insert(seedBlock, seedImage);
                    }
                }

                auto &imagesIndex = original.get<BlockIndexTag>();
                auto lowerBound = imagesIndex.lower_bound(pivot);

                originalUpper.get<BlockIndexTag>().insert(lowerBound, imagesIndex.end());
                imagesIndex.erase(lowerBound, imagesIndex.end());

                replacement.splitInto(replacementUpper, pivot);

                compareSpentKeyImages(originalUpper, replacementUpper, "upper segment", seed, step);

                /* live no longer tracks what moved out; rebuild it from the
                   lower segment so later inserts stay meaningful. */
                live.clear();

                for (const auto &entry : original.get<BlockIndexTag>())
                {
                    live.push_back(entry.keyImage);
                }
            }

            compareSpentKeyImages(original, replacement, "lower segment", seed, step);

            for (const Crypto::KeyImage &keyImage : live)
            {
                const bool inOriginal = original.get<KeyImageTag>().count(keyImage) != 0;
                const bool inReplacement = replacement.contains(keyImage);

                if (inOriginal != inReplacement)
                {
                    fail("key image lookup diverged", seed, step);
                }
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::paymentIds
       ---------------------------------------------------------------------- */
    namespace
    {
        struct PaymentIdEntry
        {
            Crypto::Hash paymentId;

            Crypto::Hash transactionHash;
        };

        struct PaymentIdOnlyTag
        {
        };
        struct TransactionHashTag
        {
        };

        typedef boost::multi_index_container<
            PaymentIdEntry,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<PaymentIdOnlyTag>,
                    BOOST_MULTI_INDEX_MEMBER(PaymentIdEntry, Crypto::Hash, paymentId)>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<TransactionHashTag>,
                    BOOST_MULTI_INDEX_MEMBER(PaymentIdEntry, Crypto::Hash, transactionHash)>>>
            BoostPaymentIds;

        /* Both indices are hash indices, so this is two maps and no ordering
           to preserve. */
        class ReplacementPaymentIds
        {
          public:
            bool insert(const Crypto::Hash &paymentId, const Crypto::Hash &transactionHash)
            {
                if (m_byTransactionHash.count(transactionHash) > 0)
                {
                    return false;
                }

                m_byTransactionHash.emplace(transactionHash, paymentId);
                m_byPaymentId.emplace(paymentId, transactionHash);

                return true;
            }

            bool erase(const Crypto::Hash &transactionHash)
            {
                const auto it = m_byTransactionHash.find(transactionHash);

                if (it == m_byTransactionHash.end())
                {
                    return false;
                }

                auto range = m_byPaymentId.equal_range(it->second);

                for (auto pit = range.first; pit != range.second; ++pit)
                {
                    if (pit->second == transactionHash)
                    {
                        m_byPaymentId.erase(pit);
                        break;
                    }
                }

                m_byTransactionHash.erase(it);

                return true;
            }

            size_t size() const
            {
                return m_byTransactionHash.size();
            }

            std::vector<Crypto::Hash> transactionHashesFor(const Crypto::Hash &paymentId) const
            {
                std::vector<Crypto::Hash> hashes;

                auto range = m_byPaymentId.equal_range(paymentId);

                for (auto it = range.first; it != range.second; ++it)
                {
                    hashes.push_back(it->second);
                }

                return hashes;
            }

          private:
            std::unordered_multimap<Crypto::Hash, Crypto::Hash> m_byPaymentId;

            std::unordered_map<Crypto::Hash, Crypto::Hash> m_byTransactionHash;
        };
    } // namespace

    void testPaymentIdContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::paymentIds (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        BoostPaymentIds original;
        ReplacementPaymentIds replacement;

        std::vector<Crypto::Hash> paymentIds;
        for (int i = 0; i < 5; i++)
        {
            paymentIds.push_back(makeHash(rng));
        }

        std::vector<Crypto::Hash> live;

        const auto byBytes = [](const Crypto::Hash &a, const Crypto::Hash &b) {
            return std::memcmp(a.data, b.data, sizeof(a.data)) < 0;
        };

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 70 || live.empty())
            {
                const Crypto::Hash paymentId = paymentIds[rng() % paymentIds.size()];

                Crypto::Hash transactionHash;

                if (!live.empty() && (rng() % 10) == 0)
                {
                    transactionHash = live[rng() % live.size()];
                }
                else
                {
                    transactionHash = makeHash(rng);
                }

                const bool insertedOriginal =
                    original.insert(PaymentIdEntry {paymentId, transactionHash}).second;
                const bool insertedReplacement = replacement.insert(paymentId, transactionHash);

                if (insertedOriginal != insertedReplacement)
                {
                    fail("insert() disagreed on whether the transaction was new", seed, step);
                }

                if (insertedOriginal)
                {
                    live.push_back(transactionHash);
                }
            }
            else
            {
                const size_t victim = rng() % live.size();
                const Crypto::Hash transactionHash = live[victim];

                const bool erasedOriginal = original.get<TransactionHashTag>().erase(transactionHash) > 0;
                const bool erasedReplacement = replacement.erase(transactionHash);

                if (erasedOriginal != erasedReplacement)
                {
                    fail("erase() disagreed on whether the transaction was present", seed, step);
                }

                live.erase(live.begin() + victim);
            }

            if (original.size() != replacement.size())
            {
                fail("size() diverged", seed, step);
            }

            /* Both indices are hash indices, so compare contents as sets. */
            for (const Crypto::Hash &paymentId : paymentIds)
            {
                std::vector<Crypto::Hash> originalHashes;

                auto range = original.get<PaymentIdOnlyTag>().equal_range(paymentId);

                for (auto it = range.first; it != range.second; ++it)
                {
                    originalHashes.push_back(it->transactionHash);
                }

                std::vector<Crypto::Hash> replacementHashes = replacement.transactionHashesFor(paymentId);

                std::sort(originalHashes.begin(), originalHashes.end(), byBytes);
                std::sort(replacementHashes.begin(), replacementHashes.end(), byBytes);

                if (originalHashes != replacementHashes)
                {
                    fail("payment id index diverged", seed, step);
                }
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    void runAll()
    {
        std::cout << std::endl << "Test Container Replacements" << std::endl << std::endl;

        /* A handful of fixed seeds rather than one, so the suite explores more
           interleavings while staying reproducible. */
        for (uint64_t seed : {1u, 7u, 1337u, 90210u})
        {
            testTransactionPoolContainer(seed, 400);
        }

        for (uint64_t seed : {1u, 7u, 1337u, 90210u})
        {
            testSpentKeyImagesContainer(seed, 400);
        }

        for (uint64_t seed : {1u, 7u, 1337u, 90210u})
        {
            testPaymentIdContainer(seed, 400);
        }
    }
} // namespace ContainerTests
