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

    void runAll()
    {
        std::cout << std::endl << "Test Container Replacements" << std::endl << std::endl;

        /* A handful of fixed seeds rather than one, so the suite explores more
           interleavings while staying reproducible. */
        for (uint64_t seed : {1u, 7u, 1337u, 90210u})
        {
            testTransactionPoolContainer(seed, 400);
        }
    }
} // namespace ContainerTests
