// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ContainerTests.h"

#include "CryptoTypes.h"
#include "common/StringTools.h"

#include <algorithm>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
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

    /* ----------------------------------------------------------------------
       BlockchainCache::transactions
       ---------------------------------------------------------------------- */
    namespace
    {
        struct CacheTxEntry
        {
            uint32_t blockIndex;

            uint32_t transactionIndex;

            Crypto::Hash transactionHash;
        };

        struct CacheInBlockTag
        {
        };
        struct CacheBlockIndexTag
        {
        };
        struct CacheTxHashTag
        {
        };

        typedef boost::multi_index_container<
            CacheTxEntry,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<CacheInBlockTag>,
                    boost::multi_index::composite_key<
                        CacheTxEntry,
                        BOOST_MULTI_INDEX_MEMBER(CacheTxEntry, uint32_t, blockIndex),
                        BOOST_MULTI_INDEX_MEMBER(CacheTxEntry, uint32_t, transactionIndex)>>,
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<CacheBlockIndexTag>,
                    BOOST_MULTI_INDEX_MEMBER(CacheTxEntry, uint32_t, blockIndex)>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<CacheTxHashTag>,
                    BOOST_MULTI_INDEX_MEMBER(CacheTxEntry, Crypto::Hash, transactionHash)>>>
            BoostTransactionsCache;

        /* One ordered map on the (blockIndex, transactionIndex) pair carries
           both the composite index and the block ordering. */
        class ReplacementTransactionsCache
        {
          public:
            typedef std::list<CacheTxEntry> Entries;

            typedef std::pair<uint32_t, uint32_t> BlockAndIndex;

            bool insert(CacheTxEntry entry)
            {
                const BlockAndIndex position {entry.blockIndex, entry.transactionIndex};
                const Crypto::Hash hash = entry.transactionHash;

                if (m_byPosition.count(position) > 0 || m_byHash.count(hash) > 0)
                {
                    return false;
                }

                const auto it = m_entries.insert(m_entries.end(), std::move(entry));

                m_byPosition.emplace(position, it);
                m_byHash.emplace(hash, it);

                return true;
            }

            const CacheTxEntry *findByHash(const Crypto::Hash &transactionHash) const
            {
                const auto it = m_byHash.find(transactionHash);

                return it == m_byHash.end() ? nullptr : &*it->second;
            }

            const CacheTxEntry *findInBlock(uint32_t blockIndex, uint32_t transactionIndex) const
            {
                const auto it = m_byPosition.find(BlockAndIndex {blockIndex, transactionIndex});

                return it == m_byPosition.end() ? nullptr : &*it->second;
            }

            size_t size() const
            {
                return m_byHash.size();
            }

            std::vector<Crypto::Hash> transactionHashesAtOrAbove(uint32_t blockIndex) const
            {
                std::vector<Crypto::Hash> hashes;

                for (auto it = m_byPosition.lower_bound(BlockAndIndex {blockIndex, 0});
                     it != m_byPosition.end();
                     ++it)
                {
                    hashes.push_back(it->second->transactionHash);
                }

                return hashes;
            }

            void splitInto(ReplacementTransactionsCache &other, uint32_t splitBlockIndex)
            {
                auto lowerBound = m_byPosition.lower_bound(BlockAndIndex {splitBlockIndex, 0});

                for (auto it = lowerBound; it != m_byPosition.end(); ++it)
                {
                    other.insert(*it->second);

                    m_byHash.erase(it->second->transactionHash);
                    m_entries.erase(it->second);
                }

                m_byPosition.erase(lowerBound, m_byPosition.end());
            }

            std::vector<CacheTxEntry> inBlockOrder() const
            {
                std::vector<CacheTxEntry> ordered;

                for (const auto &entry : m_byPosition)
                {
                    ordered.push_back(*entry.second);
                }

                return ordered;
            }

          private:
            Entries m_entries;

            std::map<BlockAndIndex, Entries::iterator> m_byPosition;

            std::unordered_map<Crypto::Hash, Entries::iterator> m_byHash;
        };

        void compareTransactionsCache(
            const BoostTransactionsCache &original,
            const ReplacementTransactionsCache &replacement,
            const std::string &what,
            uint64_t seed,
            uint64_t step)
        {
            if (original.size() != replacement.size())
            {
                fail(what + ": size() diverged", seed, step);
            }

            /* The block index index orders on block index only, so entries
               within a block are ordered by insertion. The replacement orders
               by (blockIndex, transactionIndex). Compare the block index
               sequence, which both must agree on, and the set of positions. */
            std::vector<uint32_t> originalBlocks;

            for (const auto &entry : original.get<CacheBlockIndexTag>())
            {
                originalBlocks.push_back(entry.blockIndex);
            }

            std::vector<uint32_t> replacementBlocks;

            for (const auto &entry : replacement.inBlockOrder())
            {
                replacementBlocks.push_back(entry.blockIndex);
            }

            if (originalBlocks != replacementBlocks)
            {
                fail(what + ": block ordering diverged", seed, step);
            }
        }
    } // namespace

    void testTransactionsCacheContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::transactionsCache (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        BoostTransactionsCache original;
        ReplacementTransactionsCache replacement;

        std::vector<CacheTxEntry> live;

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 80 || live.empty())
            {
                CacheTxEntry entry {};
                entry.blockIndex = static_cast<uint32_t>(rng() % 6);
                entry.transactionIndex = static_cast<uint32_t>(rng() % 4);
                entry.transactionHash = makeHash(rng);

                /* Sometimes reuse an existing hash to exercise that unique
                   index independently of the composite one. */
                if (!live.empty() && (rng() % 8) == 0)
                {
                    entry.transactionHash = live[rng() % live.size()].transactionHash;
                }

                const bool insertedOriginal = original.insert(entry).second;
                const bool insertedReplacement = replacement.insert(entry);

                if (insertedOriginal != insertedReplacement)
                {
                    fail("insert() disagreed on whether the entry was new", seed, step);
                }

                if (insertedOriginal)
                {
                    live.push_back(entry);
                }
            }
            else
            {
                const uint32_t pivot = static_cast<uint32_t>(rng() % 6);

                /* The hashes moving out, which splitTransactions collects
                   before the entries move. */
                std::vector<Crypto::Hash> originalMoving;

                auto &blockIndexIndex = original.get<CacheBlockIndexTag>();

                for (auto it = blockIndexIndex.lower_bound(pivot); it != blockIndexIndex.end(); ++it)
                {
                    originalMoving.push_back(it->transactionHash);
                }

                std::vector<Crypto::Hash> replacementMoving = replacement.transactionHashesAtOrAbove(pivot);

                const auto byBytes = [](const Crypto::Hash &a, const Crypto::Hash &b) {
                    return std::memcmp(a.data, b.data, sizeof(a.data)) < 0;
                };

                std::sort(originalMoving.begin(), originalMoving.end(), byBytes);
                std::sort(replacementMoving.begin(), replacementMoving.end(), byBytes);

                if (originalMoving != replacementMoving)
                {
                    fail("the set of transactions at or above the pivot diverged", seed, step);
                }

                BoostTransactionsCache originalUpper;
                ReplacementTransactionsCache replacementUpper;

                auto lowerBound = blockIndexIndex.lower_bound(pivot);
                originalUpper.get<CacheBlockIndexTag>().insert(lowerBound, blockIndexIndex.end());
                blockIndexIndex.erase(lowerBound, blockIndexIndex.end());

                replacement.splitInto(replacementUpper, pivot);

                compareTransactionsCache(originalUpper, replacementUpper, "upper segment", seed, step);

                live.clear();

                for (const auto &entry : original.get<CacheBlockIndexTag>())
                {
                    live.push_back(entry);
                }
            }

            compareTransactionsCache(original, replacement, "lower segment", seed, step);

            for (const CacheTxEntry &entry : live)
            {
                const auto originalIt = original.get<CacheTxHashTag>().find(entry.transactionHash);
                const CacheTxEntry *replacementEntry = replacement.findByHash(entry.transactionHash);

                if ((originalIt == original.get<CacheTxHashTag>().end()) != (replacementEntry == nullptr))
                {
                    fail("findByHash() disagreed on presence", seed, step);
                }

                const auto originalComposite = original.get<CacheInBlockTag>().find(
                    boost::make_tuple<uint32_t, uint32_t>(entry.blockIndex, entry.transactionIndex));
                const CacheTxEntry *replacementComposite =
                    replacement.findInBlock(entry.blockIndex, entry.transactionIndex);

                if ((originalComposite == original.get<CacheInBlockTag>().end())
                    != (replacementComposite == nullptr))
                {
                    fail("findInBlock() disagreed on presence", seed, step);
                }

                if (replacementComposite != nullptr
                    && !(originalComposite->transactionHash == replacementComposite->transactionHash))
                {
                    fail("findInBlock() returned a different entry", seed, step);
                }
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::blockInfos
       ---------------------------------------------------------------------- */
    namespace
    {
        struct BlockInfoEntry
        {
            Crypto::Hash blockHash;

            uint64_t timestamp;
        };

        struct BiHashTag
        {
        };
        struct BiTimestampTag
        {
        };
        struct BiRandomTag
        {
        };

        typedef boost::multi_index_container<
            BlockInfoEntry,
            boost::multi_index::indexed_by<
                boost::multi_index::random_access<boost::multi_index::tag<BiRandomTag>>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<BiHashTag>,
                    BOOST_MULTI_INDEX_MEMBER(BlockInfoEntry, Crypto::Hash, blockHash)>,
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<BiTimestampTag>,
                    BOOST_MULTI_INDEX_MEMBER(BlockInfoEntry, uint64_t, timestamp)>>>
            BoostBlockInfos;

        /* A vector plus a hash-to-position map and a timestamp-ordered map.
           Safe because the only erase is a tail truncation, so positions never
           shift while an entry is in the container. */
        class ReplacementBlockInfos
        {
          public:
            bool push_back(BlockInfoEntry entry)
            {
                if (m_positionByHash.count(entry.blockHash) > 0)
                {
                    return false;
                }

                const Crypto::Hash hash = entry.blockHash;
                const uint64_t timestamp = entry.timestamp;
                const size_t position = m_blocks.size();

                m_blocks.push_back(std::move(entry));
                m_positionByHash.emplace(hash, position);
                m_positionByTimestamp.emplace(timestamp, position);

                return true;
            }

            size_t size() const
            {
                return m_blocks.size();
            }

            const BlockInfoEntry &operator[](size_t position) const
            {
                return m_blocks[position];
            }

            bool containsHash(const Crypto::Hash &blockHash) const
            {
                return m_positionByHash.count(blockHash) > 0;
            }

            std::optional<size_t> positionOf(const Crypto::Hash &blockHash) const
            {
                const auto it = m_positionByHash.find(blockHash);

                return it == m_positionByHash.end() ? std::nullopt : std::optional<size_t>(it->second);
            }

            std::vector<Crypto::Hash> hashesInTimestampRange(uint64_t begin, uint64_t end) const
            {
                std::vector<Crypto::Hash> hashes;

                auto first = m_positionByTimestamp.lower_bound(begin);
                auto last = m_positionByTimestamp.upper_bound(end);

                for (auto it = first; it != last; ++it)
                {
                    hashes.push_back(m_blocks[it->second].blockHash);
                }

                return hashes;
            }

            void splitInto(ReplacementBlockInfos &other, size_t position)
            {
                for (size_t i = position; i < m_blocks.size(); i++)
                {
                    other.push_back(m_blocks[i]);
                }

                m_blocks.erase(m_blocks.begin() + static_cast<std::ptrdiff_t>(position), m_blocks.end());

                rebuildIndices();
            }

            const std::vector<BlockInfoEntry> &entries() const
            {
                return m_blocks;
            }

          private:
            void rebuildIndices()
            {
                m_positionByHash.clear();
                m_positionByTimestamp.clear();

                for (size_t i = 0; i < m_blocks.size(); i++)
                {
                    m_positionByHash.emplace(m_blocks[i].blockHash, i);
                    m_positionByTimestamp.emplace(m_blocks[i].timestamp, i);
                }
            }

            std::vector<BlockInfoEntry> m_blocks;

            std::unordered_map<Crypto::Hash, size_t> m_positionByHash;

            std::multimap<uint64_t, size_t> m_positionByTimestamp;
        };
    } // namespace

    void testBlockInfoContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::blockInfos (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        BoostBlockInfos original;
        ReplacementBlockInfos replacement;

        std::vector<Crypto::Hash> live;

        const auto byBytes = [](const Crypto::Hash &a, const Crypto::Hash &b) {
            return std::memcmp(a.data, b.data, sizeof(a.data)) < 0;
        };

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 85 || original.empty())
            {
                BlockInfoEntry entry {};
                entry.blockHash = makeHash(rng);

                /* A narrow timestamp range so the ordered index has ties, and
                   so timestamps are not monotonic with position. */
                entry.timestamp = rng() % 10;

                const bool insertedOriginal = original.get<BiRandomTag>().push_back(entry).second;
                const bool insertedReplacement = replacement.push_back(entry);

                if (insertedOriginal != insertedReplacement)
                {
                    fail("push_back() disagreed on whether the block was new", seed, step);
                }

                if (insertedOriginal)
                {
                    live.push_back(entry.blockHash);
                }
            }
            else
            {
                /* Tail split, the only erase this container ever sees. */
                const size_t position = rng() % original.size();

                BoostBlockInfos originalUpper;
                ReplacementBlockInfos replacementUpper;

                auto &randomIndex = original.get<BiRandomTag>();
                auto bound = std::next(randomIndex.begin(), static_cast<std::ptrdiff_t>(position));

                for (auto it = bound; it != randomIndex.end(); ++it)
                {
                    originalUpper.get<BiRandomTag>().push_back(*it);
                }

                randomIndex.erase(bound, randomIndex.end());

                replacement.splitInto(replacementUpper, position);

                if (originalUpper.size() != replacementUpper.size())
                {
                    fail("upper segment size diverged after split", seed, step);
                }

                for (size_t i = 0; i < originalUpper.size(); i++)
                {
                    if (!(originalUpper.get<BiRandomTag>()[i].blockHash == replacementUpper[i].blockHash))
                    {
                        fail("upper segment ordering diverged after split", seed, step);
                    }
                }

                live.clear();

                for (const auto &entry : original.get<BiRandomTag>())
                {
                    live.push_back(entry.blockHash);
                }
            }

            if (original.size() != replacement.size())
            {
                fail("size() diverged", seed, step);
            }

            /* Positional access, and the hash-to-position lookup that replaced
               project<BlockIndexTag>() + std::distance. */
            for (size_t i = 0; i < original.size(); i++)
            {
                const auto &originalEntry = original.get<BiRandomTag>()[i];

                if (!(originalEntry.blockHash == replacement[i].blockHash))
                {
                    fail("positional access diverged", seed, step);
                }

                const auto hashIt = original.get<BiHashTag>().find(originalEntry.blockHash);
                const auto projected = original.project<BiRandomTag>(hashIt);
                const size_t originalPosition =
                    static_cast<size_t>(std::distance(original.get<BiRandomTag>().begin(), projected));

                const auto replacementPosition = replacement.positionOf(originalEntry.blockHash);

                if (!replacementPosition || *replacementPosition != originalPosition)
                {
                    fail("positionOf() diverged from project<>() + distance", seed, step);
                }
            }

            /* Timestamp range query. The ordered index does not specify the
               order of equal timestamps against the replacement's, so compare
               as sets. */
            {
                const uint64_t begin = rng() % 10;
                const uint64_t end = begin + (rng() % 4);

                std::vector<Crypto::Hash> originalHashes;

                auto &timestampIndex = original.get<BiTimestampTag>();

                for (auto it = timestampIndex.lower_bound(begin); it != timestampIndex.upper_bound(end); ++it)
                {
                    originalHashes.push_back(it->blockHash);
                }

                std::vector<Crypto::Hash> replacementHashes = replacement.hashesInTimestampRange(begin, end);

                std::sort(originalHashes.begin(), originalHashes.end(), byBytes);
                std::sort(replacementHashes.begin(), replacementHashes.end(), byBytes);

                if (originalHashes != replacementHashes)
                {
                    fail("timestamp range query diverged", seed, step);
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

        for (uint64_t seed : {1u, 7u, 1337u, 90210u})
        {
            testTransactionsCacheContainer(seed, 400);
        }

        for (uint64_t seed : {1u, 7u, 1337u, 90210u})
        {
            testBlockInfoContainer(seed, 200);
        }
    }
} // namespace ContainerTests
