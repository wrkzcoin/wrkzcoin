// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "ContainerTests.h"

#include "CryptoTypes.h"
#include "common/StringTools.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * These began as differential tests against the boost::multi_index containers
 * they replaced. Boost is gone, so the reference side is now a naive model:
 * everything in one vector, lookups by linear scan, ordering by an explicit
 * stable selection sort.
 *
 * That keeps the tests meaningful rather than tautological. The models are
 * correct by inspection and share no machinery with the containers under test,
 * which use hash maps, ordered maps and std::stable_sort. A model that merely
 * repeated the implementation would prove nothing.
 *
 * Ordering rule the models encode: among elements that compare equivalent,
 * insertion order wins. That is what ordered_non_unique did - its insert
 * placed an element at the upper bound of its equal range - and it is the
 * property the replacements have to reproduce.
 */

namespace ContainerTests
{
    namespace
    {
        void fail(const std::string &what, uint64_t seed, uint64_t step)
        {
            std::cout << std::endl
                      << "Container test FAILED at step " << step << " (seed " << seed << ")" << std::endl
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

        Crypto::KeyImage makeKeyImage(std::mt19937_64 &rng)
        {
            Crypto::KeyImage keyImage {};

            for (size_t i = 0; i < sizeof(keyImage.data); i++)
            {
                keyImage.data[i] = static_cast<uint8_t>(rng() & 0xff);
            }

            return keyImage;
        }

        bool hashLess(const Crypto::Hash &a, const Crypto::Hash &b)
        {
            return std::memcmp(a.data, b.data, sizeof(a.data)) < 0;
        }

        void requireSameHashSet(
            std::vector<Crypto::Hash> expected,
            std::vector<Crypto::Hash> actual,
            const std::string &what,
            uint64_t seed,
            uint64_t step)
        {
            std::sort(expected.begin(), expected.end(), hashLess);
            std::sort(actual.begin(), actual.end(), hashLess);

            if (expected != actual)
            {
                fail(what, seed, step);
            }
        }

        /* Stable selection sort: repeatedly take the first element that no
           remaining element compares less than. Deliberately not
           std::stable_sort, so the containers' use of it is checked rather
           than restated. */
        template<typename T, typename Less> std::vector<T> stableOrder(const std::vector<T> &items, Less less)
        {
            std::vector<T> remaining = items;
            std::vector<T> ordered;
            ordered.reserve(remaining.size());

            while (!remaining.empty())
            {
                size_t best = 0;

                for (size_t i = 1; i < remaining.size(); i++)
                {
                    if (less(remaining[i], remaining[best]))
                    {
                        best = i;
                    }
                }

                ordered.push_back(remaining[best]);
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(best));
            }

            return ordered;
        }
    } // namespace

    /* ----------------------------------------------------------------------
       TransactionPool: unique hash index, priority ordering, non-unique
       payment id index.
       ---------------------------------------------------------------------- */
    namespace
    {
        struct TestTx
        {
            uint64_t receiveTime;

            Crypto::Hash txHash;

            std::optional<Crypto::Hash> paymentId;

            uint64_t fee;

            uint64_t size;
        };

        /* Mirrors TransactionPriorityComparator: fee per byte descending, then
           size ascending, then receive time ascending. A strict weak ordering -
           equivalent elements return false, the property the real comparator
           was missing before this work. */
        struct TestPriorityComparator
        {
            bool operator()(const TestTx &lhs, const TestTx &rhs) const
            {
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

        class ModelPool
        {
          public:
            bool insert(const TestTx &tx)
            {
                if (find(tx.txHash) != nullptr)
                {
                    return false;
                }

                m_items.push_back(tx);

                return true;
            }

            const TestTx *find(const Crypto::Hash &hash) const
            {
                for (const auto &item : m_items)
                {
                    if (item.txHash == hash)
                    {
                        return &item;
                    }
                }

                return nullptr;
            }

            bool erase(const Crypto::Hash &hash)
            {
                for (size_t i = 0; i < m_items.size(); i++)
                {
                    if (m_items[i].txHash == hash)
                    {
                        m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(i));
                        return true;
                    }
                }

                return false;
            }

            size_t size() const
            {
                return m_items.size();
            }

            std::vector<Crypto::Hash> byPriority() const
            {
                const auto ordered = stableOrder(
                    m_items, [](const TestTx &a, const TestTx &b) { return TestPriorityComparator {}(a, b); });

                std::vector<Crypto::Hash> hashes;

                for (const auto &tx : ordered)
                {
                    hashes.push_back(tx.txHash);
                }

                return hashes;
            }

            std::vector<Crypto::Hash> hashesByPaymentId(const Crypto::Hash &paymentId) const
            {
                std::vector<Crypto::Hash> hashes;

                for (const auto &item : m_items)
                {
                    if (item.paymentId && *item.paymentId == paymentId)
                    {
                        hashes.push_back(item.txHash);
                    }
                }

                return hashes;
            }

          private:
            std::vector<TestTx> m_items;
        };

        /* The shape TransactionPool uses: a list for stable addresses, hash
           maps for the indices, priority materialised on demand. */
        class PoolUnderTest
        {
          public:
            bool insert(TestTx tx)
            {
                if (m_byHash.count(tx.txHash) > 0)
                {
                    return false;
                }

                const Crypto::Hash hash = tx.txHash;
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

            std::vector<Crypto::Hash> byPriority() const
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

                std::vector<Crypto::Hash> hashes;

                for (const TestTx *tx : ordered)
                {
                    hashes.push_back(tx->txHash);
                }

                return hashes;
            }

            std::vector<Crypto::Hash> hashesByPaymentId(const Crypto::Hash &paymentId) const
            {
                std::vector<Crypto::Hash> hashes;

                auto range = m_byPaymentId.equal_range(paymentId);

                for (auto it = range.first; it != range.second; ++it)
                {
                    hashes.push_back(it->second->txHash);
                }

                return hashes;
            }

          private:
            typedef std::list<TestTx> Items;

            Items m_items;

            std::unordered_map<Crypto::Hash, Items::iterator> m_byHash;

            std::unordered_multimap<Crypto::Hash, Items::iterator> m_byPaymentId;
        };
    } // namespace

    void testTransactionPoolContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::transactionPool (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        ModelPool model;
        PoolUnderTest subject;

        std::vector<Crypto::Hash> paymentIds;
        for (int i = 0; i < 4; i++)
        {
            paymentIds.push_back(makeHash(rng));
        }

        std::vector<Crypto::Hash> live;

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 65 || live.empty())
            {
                TestTx tx {};

                /* Tiny ranges so equivalent elements are common - tie ordering
                   is the property most likely to diverge. */
                tx.receiveTime = rng() % 3;
                tx.fee = 1 + rng() % 3;
                tx.size = 1 + rng() % 3;

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

                const bool insertedModel = model.insert(tx);

                if (insertedModel != subject.insert(tx))
                {
                    fail("insert() disagreed on whether the transaction was new", seed, step);
                }

                if (insertedModel)
                {
                    live.push_back(tx.txHash);
                }
            }
            else
            {
                const size_t victim = rng() % live.size();
                const Crypto::Hash hash = live[victim];

                if (model.erase(hash) != subject.erase(hash))
                {
                    fail("erase() disagreed on whether the transaction was present", seed, step);
                }

                live.erase(live.begin() + static_cast<std::ptrdiff_t>(victim));
            }

            if (model.size() != subject.size())
            {
                fail("size() diverged", seed, step);
            }

            /* Priority ordering is fully specified, ties included. */
            if (model.byPriority() != subject.byPriority())
            {
                fail("priority ordering diverged", seed, step);
            }

            for (const Crypto::Hash &hash : live)
            {
                const TestTx *fromModel = model.find(hash);
                const TestTx *fromSubject = subject.find(hash);

                if ((fromModel == nullptr) != (fromSubject == nullptr))
                {
                    fail("find() disagreed on presence", seed, step);
                }

                if (fromModel != nullptr && fromModel->receiveTime != fromSubject->receiveTime)
                {
                    fail("find() returned a different transaction", seed, step);
                }
            }

            /* A hash index specifies no order, so compare as sets. */
            for (const Crypto::Hash &paymentId : paymentIds)
            {
                requireSameHashSet(
                    model.hashesByPaymentId(paymentId),
                    subject.hashesByPaymentId(paymentId),
                    "payment id index diverged",
                    seed,
                    step);
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::spentKeyImages: ordered on block index, unique on key
       image, split at a lower bound.
       ---------------------------------------------------------------------- */
    namespace
    {
        struct SpentKeyImageEntry
        {
            uint32_t blockIndex;

            Crypto::KeyImage keyImage;
        };

        class ModelSpentKeyImages
        {
          public:
            bool insert(uint32_t blockIndex, const Crypto::KeyImage &keyImage)
            {
                if (contains(keyImage))
                {
                    return false;
                }

                m_items.push_back(SpentKeyImageEntry {blockIndex, keyImage});

                return true;
            }

            bool contains(const Crypto::KeyImage &keyImage) const
            {
                for (const auto &item : m_items)
                {
                    if (item.keyImage == keyImage)
                    {
                        return true;
                    }
                }

                return false;
            }

            size_t size() const
            {
                return m_items.size();
            }

            std::vector<SpentKeyImageEntry> inBlockIndexOrder() const
            {
                return stableOrder(m_items, [](const SpentKeyImageEntry &a, const SpentKeyImageEntry &b) {
                    return a.blockIndex < b.blockIndex;
                });
            }

            void splitInto(ModelSpentKeyImages &other, uint32_t splitBlockIndex)
            {
                for (const auto &entry : inBlockIndexOrder())
                {
                    if (entry.blockIndex >= splitBlockIndex)
                    {
                        other.insert(entry.blockIndex, entry.keyImage);
                    }
                }

                std::vector<SpentKeyImageEntry> kept;

                for (const auto &entry : m_items)
                {
                    if (entry.blockIndex < splitBlockIndex)
                    {
                        kept.push_back(entry);
                    }
                }

                m_items = kept;
            }

          private:
            std::vector<SpentKeyImageEntry> m_items;
        };

        class SpentKeyImagesUnderTest
        {
          public:
            bool insert(uint32_t blockIndex, const Crypto::KeyImage &keyImage)
            {
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

            std::vector<SpentKeyImageEntry> inBlockIndexOrder() const
            {
                std::vector<SpentKeyImageEntry> entries;

                for (const auto &entry : m_byBlockIndex)
                {
                    entries.push_back(SpentKeyImageEntry {entry.first, entry.second});
                }

                return entries;
            }

            void splitInto(SpentKeyImagesUnderTest &other, uint32_t splitBlockIndex)
            {
                auto lowerBound = m_byBlockIndex.lower_bound(splitBlockIndex);

                for (auto it = lowerBound; it != m_byBlockIndex.end(); ++it)
                {
                    other.insert(it->first, it->second);
                    m_byKeyImage.erase(it->second);
                }

                m_byBlockIndex.erase(lowerBound, m_byBlockIndex.end());
            }

          private:
            std::multimap<uint32_t, Crypto::KeyImage> m_byBlockIndex;

            std::unordered_map<Crypto::KeyImage, uint32_t> m_byKeyImage;
        };

        void compareSpentKeyImages(
            const ModelSpentKeyImages &model,
            const SpentKeyImagesUnderTest &subject,
            const std::string &what,
            uint64_t seed,
            uint64_t step)
        {
            if (model.size() != subject.size())
            {
                fail(what + ": size() diverged", seed, step);
            }

            const auto expected = model.inBlockIndexOrder();
            const auto actual = subject.inBlockIndexOrder();

            if (expected.size() != actual.size())
            {
                fail(what + ": block index ordering length diverged", seed, step);
            }

            for (size_t i = 0; i < expected.size(); i++)
            {
                if (expected[i].blockIndex != actual[i].blockIndex || !(expected[i].keyImage == actual[i].keyImage))
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

        ModelSpentKeyImages model;
        SpentKeyImagesUnderTest subject;

        std::vector<Crypto::KeyImage> live;

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 80 || live.empty())
            {
                /* A narrow block range so several key images share a block. */
                const uint32_t blockIndex = static_cast<uint32_t>(rng() % 8);

                Crypto::KeyImage keyImage;

                if (!live.empty() && (rng() % 10) == 0)
                {
                    keyImage = live[rng() % live.size()];
                }
                else
                {
                    keyImage = makeKeyImage(rng);
                }

                if (model.insert(blockIndex, keyImage) != subject.insert(blockIndex, keyImage))
                {
                    fail("insert() disagreed on whether the key image was new", seed, step);
                }

                live.push_back(keyImage);
            }
            else
            {
                const uint32_t pivot = static_cast<uint32_t>(rng() % 8);

                ModelSpentKeyImages modelUpper;
                SpentKeyImagesUnderTest subjectUpper;

                /* Seed the target with key images still live in the source, so
                   the split has to skip duplicates on the receiving side rather
                   than desync its two indices. */
                if (!live.empty())
                {
                    const size_t seedCount = 1 + (rng() % 2);

                    for (size_t i = 0; i < seedCount; i++)
                    {
                        const Crypto::KeyImage seedImage = live[rng() % live.size()];
                        const uint32_t seedBlock = static_cast<uint32_t>(rng() % 8);

                        modelUpper.insert(seedBlock, seedImage);
                        subjectUpper.insert(seedBlock, seedImage);
                    }
                }

                model.splitInto(modelUpper, pivot);
                subject.splitInto(subjectUpper, pivot);

                compareSpentKeyImages(modelUpper, subjectUpper, "upper segment", seed, step);

                live.clear();

                for (const auto &entry : model.inBlockIndexOrder())
                {
                    live.push_back(entry.keyImage);
                }
            }

            compareSpentKeyImages(model, subject, "lower segment", seed, step);

            for (const Crypto::KeyImage &keyImage : live)
            {
                if (model.contains(keyImage) != subject.contains(keyImage))
                {
                    fail("key image lookup diverged", seed, step);
                }
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::paymentIds: two hash indices, one unique.
       ---------------------------------------------------------------------- */
    namespace
    {
        struct PaymentIdEntry
        {
            Crypto::Hash paymentId;

            Crypto::Hash transactionHash;
        };

        class ModelPaymentIds
        {
          public:
            bool insert(const Crypto::Hash &paymentId, const Crypto::Hash &transactionHash)
            {
                for (const auto &entry : m_items)
                {
                    if (entry.transactionHash == transactionHash)
                    {
                        return false;
                    }
                }

                m_items.push_back(PaymentIdEntry {paymentId, transactionHash});

                return true;
            }

            bool erase(const Crypto::Hash &transactionHash)
            {
                for (size_t i = 0; i < m_items.size(); i++)
                {
                    if (m_items[i].transactionHash == transactionHash)
                    {
                        m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(i));
                        return true;
                    }
                }

                return false;
            }

            size_t size() const
            {
                return m_items.size();
            }

            std::vector<Crypto::Hash> transactionHashesFor(const Crypto::Hash &paymentId) const
            {
                std::vector<Crypto::Hash> hashes;

                for (const auto &entry : m_items)
                {
                    if (entry.paymentId == paymentId)
                    {
                        hashes.push_back(entry.transactionHash);
                    }
                }

                return hashes;
            }

          private:
            std::vector<PaymentIdEntry> m_items;
        };

        class PaymentIdsUnderTest
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

        ModelPaymentIds model;
        PaymentIdsUnderTest subject;

        std::vector<Crypto::Hash> paymentIds;
        for (int i = 0; i < 5; i++)
        {
            paymentIds.push_back(makeHash(rng));
        }

        std::vector<Crypto::Hash> live;

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

                const bool insertedModel = model.insert(paymentId, transactionHash);

                if (insertedModel != subject.insert(paymentId, transactionHash))
                {
                    fail("insert() disagreed on whether the transaction was new", seed, step);
                }

                if (insertedModel)
                {
                    live.push_back(transactionHash);
                }
            }
            else
            {
                const size_t victim = rng() % live.size();
                const Crypto::Hash transactionHash = live[victim];

                if (model.erase(transactionHash) != subject.erase(transactionHash))
                {
                    fail("erase() disagreed on whether the transaction was present", seed, step);
                }

                live.erase(live.begin() + static_cast<std::ptrdiff_t>(victim));
            }

            if (model.size() != subject.size())
            {
                fail("size() diverged", seed, step);
            }

            for (const Crypto::Hash &paymentId : paymentIds)
            {
                requireSameHashSet(
                    model.transactionHashesFor(paymentId),
                    subject.transactionHashesFor(paymentId),
                    "payment id index diverged",
                    seed,
                    step);
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::transactions: one ordered map on the
       (blockIndex, transactionIndex) pair standing in for both a composite
       unique index and an ordered index on blockIndex.
       ---------------------------------------------------------------------- */
    namespace
    {
        struct CacheTxEntry
        {
            uint32_t blockIndex;

            uint32_t transactionIndex;

            Crypto::Hash transactionHash;
        };

        class ModelTransactionsCache
        {
          public:
            bool insert(const CacheTxEntry &entry)
            {
                for (const auto &item : m_items)
                {
                    if ((item.blockIndex == entry.blockIndex && item.transactionIndex == entry.transactionIndex)
                        || item.transactionHash == entry.transactionHash)
                    {
                        return false;
                    }
                }

                m_items.push_back(entry);

                return true;
            }

            const CacheTxEntry *findInBlock(uint32_t blockIndex, uint32_t transactionIndex) const
            {
                for (const auto &item : m_items)
                {
                    if (item.blockIndex == blockIndex && item.transactionIndex == transactionIndex)
                    {
                        return &item;
                    }
                }

                return nullptr;
            }

            const CacheTxEntry *findByHash(const Crypto::Hash &hash) const
            {
                for (const auto &item : m_items)
                {
                    if (item.transactionHash == hash)
                    {
                        return &item;
                    }
                }

                return nullptr;
            }

            size_t size() const
            {
                return m_items.size();
            }

            std::vector<Crypto::Hash> transactionHashesAtOrAbove(uint32_t blockIndex) const
            {
                std::vector<Crypto::Hash> hashes;

                for (const auto &item : m_items)
                {
                    if (item.blockIndex >= blockIndex)
                    {
                        hashes.push_back(item.transactionHash);
                    }
                }

                return hashes;
            }

            std::vector<uint32_t> blockOrder() const
            {
                const auto ordered = stableOrder(m_items, [](const CacheTxEntry &a, const CacheTxEntry &b) {
                    if (a.blockIndex != b.blockIndex)
                    {
                        return a.blockIndex < b.blockIndex;
                    }

                    return a.transactionIndex < b.transactionIndex;
                });

                std::vector<uint32_t> blocks;

                for (const auto &entry : ordered)
                {
                    blocks.push_back(entry.blockIndex);
                }

                return blocks;
            }

            void splitInto(ModelTransactionsCache &other, uint32_t splitBlockIndex)
            {
                std::vector<CacheTxEntry> kept;

                for (const auto &item : m_items)
                {
                    if (item.blockIndex >= splitBlockIndex)
                    {
                        other.insert(item);
                    }
                    else
                    {
                        kept.push_back(item);
                    }
                }

                m_items = kept;
            }

          private:
            std::vector<CacheTxEntry> m_items;
        };

        class TransactionsCacheUnderTest
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

            const CacheTxEntry *findInBlock(uint32_t blockIndex, uint32_t transactionIndex) const
            {
                const auto it = m_byPosition.find(BlockAndIndex {blockIndex, transactionIndex});

                return it == m_byPosition.end() ? nullptr : &*it->second;
            }

            const CacheTxEntry *findByHash(const Crypto::Hash &hash) const
            {
                const auto it = m_byHash.find(hash);

                return it == m_byHash.end() ? nullptr : &*it->second;
            }

            size_t size() const
            {
                return m_byHash.size();
            }

            std::vector<Crypto::Hash> transactionHashesAtOrAbove(uint32_t blockIndex) const
            {
                std::vector<Crypto::Hash> hashes;

                for (auto it = m_byPosition.lower_bound(BlockAndIndex {blockIndex, 0}); it != m_byPosition.end(); ++it)
                {
                    hashes.push_back(it->second->transactionHash);
                }

                return hashes;
            }

            std::vector<uint32_t> blockOrder() const
            {
                std::vector<uint32_t> blocks;

                for (const auto &entry : m_byPosition)
                {
                    blocks.push_back(entry.second->blockIndex);
                }

                return blocks;
            }

            void splitInto(TransactionsCacheUnderTest &other, uint32_t splitBlockIndex)
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

          private:
            Entries m_entries;

            std::map<BlockAndIndex, Entries::iterator> m_byPosition;

            std::unordered_map<Crypto::Hash, Entries::iterator> m_byHash;
        };
    } // namespace

    void testTransactionsCacheContainer(uint64_t seed, uint64_t iterations)
    {
        std::cout << "ContainerTests::transactionsCache (seed " << seed << ", " << iterations << " ops): ";

        std::mt19937_64 rng(seed);

        ModelTransactionsCache model;
        TransactionsCacheUnderTest subject;

        std::vector<CacheTxEntry> live;

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 80 || live.empty())
            {
                CacheTxEntry entry {};
                entry.blockIndex = static_cast<uint32_t>(rng() % 6);
                entry.transactionIndex = static_cast<uint32_t>(rng() % 4);
                entry.transactionHash = makeHash(rng);

                if (!live.empty() && (rng() % 8) == 0)
                {
                    entry.transactionHash = live[rng() % live.size()].transactionHash;
                }

                const bool insertedModel = model.insert(entry);

                if (insertedModel != subject.insert(entry))
                {
                    fail("insert() disagreed on whether the entry was new", seed, step);
                }

                if (insertedModel)
                {
                    live.push_back(entry);
                }
            }
            else
            {
                const uint32_t pivot = static_cast<uint32_t>(rng() % 6);

                requireSameHashSet(
                    model.transactionHashesAtOrAbove(pivot),
                    subject.transactionHashesAtOrAbove(pivot),
                    "the set of transactions at or above the pivot diverged",
                    seed,
                    step);

                ModelTransactionsCache modelUpper;
                TransactionsCacheUnderTest subjectUpper;

                model.splitInto(modelUpper, pivot);
                subject.splitInto(subjectUpper, pivot);

                if (modelUpper.size() != subjectUpper.size() || modelUpper.blockOrder() != subjectUpper.blockOrder())
                {
                    fail("upper segment diverged after split", seed, step);
                }

                live.clear();

                for (uint32_t block = 0; block < 6; block++)
                {
                    for (uint32_t index = 0; index < 4; index++)
                    {
                        const auto *entry = model.findInBlock(block, index);

                        if (entry != nullptr)
                        {
                            live.push_back(*entry);
                        }
                    }
                }
            }

            if (model.size() != subject.size() || model.blockOrder() != subject.blockOrder())
            {
                fail("lower segment diverged", seed, step);
            }

            for (const CacheTxEntry &entry : live)
            {
                if ((model.findByHash(entry.transactionHash) == nullptr)
                    != (subject.findByHash(entry.transactionHash) == nullptr))
                {
                    fail("findByHash() disagreed on presence", seed, step);
                }

                const auto *fromModel = model.findInBlock(entry.blockIndex, entry.transactionIndex);
                const auto *fromSubject = subject.findInBlock(entry.blockIndex, entry.transactionIndex);

                if ((fromModel == nullptr) != (fromSubject == nullptr))
                {
                    fail("findInBlock() disagreed on presence", seed, step);
                }

                if (fromModel != nullptr && !(fromModel->transactionHash == fromSubject->transactionHash))
                {
                    fail("findInBlock() returned a different entry", seed, step);
                }
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    /* ----------------------------------------------------------------------
       BlockchainCache::blockInfos: positional access, a hash to position
       lookup, a timestamp range query, and a tail split.
       ---------------------------------------------------------------------- */
    namespace
    {
        struct BlockInfoEntry
        {
            Crypto::Hash blockHash;

            uint64_t timestamp;
        };

        class ModelBlockInfos
        {
          public:
            bool push_back(const BlockInfoEntry &entry)
            {
                if (positionOf(entry.blockHash))
                {
                    return false;
                }

                m_blocks.push_back(entry);

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

            std::optional<size_t> positionOf(const Crypto::Hash &blockHash) const
            {
                for (size_t i = 0; i < m_blocks.size(); i++)
                {
                    if (m_blocks[i].blockHash == blockHash)
                    {
                        return i;
                    }
                }

                return std::nullopt;
            }

            std::vector<Crypto::Hash> hashesInTimestampRange(uint64_t begin, uint64_t end) const
            {
                std::vector<Crypto::Hash> hashes;

                for (const auto &block : m_blocks)
                {
                    if (block.timestamp >= begin && block.timestamp <= end)
                    {
                        hashes.push_back(block.blockHash);
                    }
                }

                return hashes;
            }

            void splitInto(ModelBlockInfos &other, size_t position)
            {
                for (size_t i = position; i < m_blocks.size(); i++)
                {
                    other.push_back(m_blocks[i]);
                }

                m_blocks.erase(m_blocks.begin() + static_cast<std::ptrdiff_t>(position), m_blocks.end());
            }

          private:
            std::vector<BlockInfoEntry> m_blocks;
        };

        class BlockInfosUnderTest
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

            void splitInto(BlockInfosUnderTest &other, size_t position)
            {
                for (size_t i = position; i < m_blocks.size(); i++)
                {
                    other.push_back(m_blocks[i]);
                }

                m_blocks.erase(m_blocks.begin() + static_cast<std::ptrdiff_t>(position), m_blocks.end());

                rebuildIndices();
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

        ModelBlockInfos model;
        BlockInfosUnderTest subject;

        for (uint64_t step = 0; step < iterations; step++)
        {
            if (rng() % 100 < 85 || model.size() == 0)
            {
                BlockInfoEntry entry {};
                entry.blockHash = makeHash(rng);

                /* A narrow timestamp range, so the ordered index has ties and
                   timestamps are not monotonic with position. */
                entry.timestamp = rng() % 10;

                if (model.push_back(entry) != subject.push_back(entry))
                {
                    fail("push_back() disagreed on whether the block was new", seed, step);
                }
            }
            else
            {
                /* The tail split, the only erase this container ever sees. */
                const size_t position = rng() % model.size();

                ModelBlockInfos modelUpper;
                BlockInfosUnderTest subjectUpper;

                model.splitInto(modelUpper, position);
                subject.splitInto(subjectUpper, position);

                if (modelUpper.size() != subjectUpper.size())
                {
                    fail("upper segment size diverged after split", seed, step);
                }

                for (size_t i = 0; i < modelUpper.size(); i++)
                {
                    if (!(modelUpper[i].blockHash == subjectUpper[i].blockHash))
                    {
                        fail("upper segment ordering diverged after split", seed, step);
                    }
                }
            }

            if (model.size() != subject.size())
            {
                fail("size() diverged", seed, step);
            }

            for (size_t i = 0; i < model.size(); i++)
            {
                if (!(model[i].blockHash == subject[i].blockHash))
                {
                    fail("positional access diverged", seed, step);
                }

                const auto expected = model.positionOf(model[i].blockHash);
                const auto actual = subject.positionOf(model[i].blockHash);

                if (!actual || *expected != *actual)
                {
                    fail("positionOf() diverged", seed, step);
                }
            }

            {
                const uint64_t begin = rng() % 10;
                const uint64_t end = begin + (rng() % 4);

                requireSameHashSet(
                    model.hashesInTimestampRange(begin, end),
                    subject.hashesInTimestampRange(begin, end),
                    "timestamp range query diverged",
                    seed,
                    step);
            }
        }

        std::cout << "PASSED" << std::endl;
    }

    void runAll()
    {
        std::cout << std::endl << "Test Container Replacements" << std::endl << std::endl;

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
