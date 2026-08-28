// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ITransfersContainer.h"
#include "WalletGreenTypes.h"
#include "common/FileMappedVector.h"
#include "crypto/chacha8.h"

#include <cassert>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CryptoNote
{
    const uint64_t ACCOUNT_CREATE_TIME_ACCURACY = 60 * 60 * 24;

    struct WalletRecord
    {
        Crypto::PublicKey spendPublicKey;
        Crypto::SecretKey spendSecretKey;
        CryptoNote::ITransfersContainer *container = nullptr;
        uint64_t pendingBalance = 0;
        uint64_t actualBalance = 0;
        time_t creationTimestamp;
    };

#pragma pack(push, 1)
    struct EncryptedWalletRecord
    {
        Crypto::chacha8_iv iv;
        // Secret key, public key and creation timestamp
        uint8_t data[sizeof(Crypto::PublicKey) + sizeof(Crypto::SecretKey) + sizeof(uint64_t)];
    };
#pragma pack(pop)

    struct RandomAccessIndex
    {
    };
    struct KeysIndex
    {
    };
    struct TransfersContainerIndex
    {
    };

    struct WalletIndex
    {
    };
    struct TransactionOutputIndex
    {
    };
    struct BlockHeightIndex
    {
    };

    struct TransactionHashIndex
    {
    };
    struct TransactionIndex
    {
    };
    struct BlockHashIndex
    {
    };

    /* ----------------------------------------------------------------------
       Replacements for four boost::multi_index containers.

       Unlike everywhere else in this codebase, modify() here genuinely
       re-indexes: six of the nine call sites write blockHeight or container,
       which are index keys. Each replacement therefore exposes an explicit
       update that fixes the affected index rather than relying on the key
       being derived on read.

       project<RandomAccessIndex>() followed by std::distance appeared four
       times, always to recover an element's ordinal position from a lookup by
       some other key. positionOf() does that directly.
       ---------------------------------------------------------------------- */

    /* random_access + hashed_unique on spend public key + hashed_unique on the
       transfers container pointer.

       A vector carries the random access index. Wallets are erased from the
       middle (deleteAddress), which shifts positions, so the two position maps
       are rebuilt on erase - there are at most a few thousand wallets and
       deletion is rare. */
    class WalletsContainer
    {
      public:
        typedef std::vector<WalletRecord>::const_iterator const_iterator;

        bool push_back(WalletRecord record)
        {
            if (m_positionBySpendKey.count(record.spendPublicKey) > 0)
            {
                return false;
            }

            const Crypto::PublicKey spendPublicKey = record.spendPublicKey;
            const CryptoNote::ITransfersContainer *container = record.container;
            const size_t position = m_records.size();

            m_records.push_back(std::move(record));
            m_positionBySpendKey.emplace(spendPublicKey, position);
            m_positionByContainer.emplace(container, position);

            return true;
        }

        const WalletRecord &operator[](size_t position) const
        {
            return m_records[position];
        }

        const WalletRecord &at(size_t position) const
        {
            return m_records.at(position);
        }

        const_iterator begin() const
        {
            return m_records.begin();
        }

        const_iterator end() const
        {
            return m_records.end();
        }

        size_t size() const
        {
            return m_records.size();
        }

        bool empty() const
        {
            return m_records.empty();
        }

        void clear()
        {
            m_records.clear();
            m_positionBySpendKey.clear();
            m_positionByContainer.clear();
        }

        size_t countBySpendKey(const Crypto::PublicKey &spendPublicKey) const
        {
            return m_positionBySpendKey.count(spendPublicKey);
        }

        const WalletRecord *findBySpendKey(const Crypto::PublicKey &spendPublicKey) const
        {
            const auto it = m_positionBySpendKey.find(spendPublicKey);

            return it == m_positionBySpendKey.end() ? nullptr : &m_records[it->second];
        }

        const WalletRecord *findByContainer(const CryptoNote::ITransfersContainer *container) const
        {
            const auto it = m_positionByContainer.find(container);

            return it == m_positionByContainer.end() ? nullptr : &m_records[it->second];
        }

        /* Replaces project<RandomAccessIndex>() + std::distance. */
        std::optional<size_t> positionOfSpendKey(const Crypto::PublicKey &spendPublicKey) const
        {
            const auto it = m_positionBySpendKey.find(spendPublicKey);

            return it == m_positionBySpendKey.end() ? std::nullopt : std::optional<size_t>(it->second);
        }

        std::optional<size_t> positionOfContainer(const CryptoNote::ITransfersContainer *container) const
        {
            const auto it = m_positionByContainer.find(container);

            return it == m_positionByContainer.end() ? std::nullopt : std::optional<size_t>(it->second);
        }

        /* Mutates a record in place and repairs the container index if that
           key changed. The spend public key is the primary key and may not
           change. */
        template<typename F> bool updateAt(size_t position, F modifier)
        {
            if (position >= m_records.size())
            {
                return false;
            }

            WalletRecord &record = m_records[position];

            const Crypto::PublicKey spendPublicKey = record.spendPublicKey;
            const CryptoNote::ITransfersContainer *oldContainer = record.container;

            modifier(record);

            assert(record.spendPublicKey == spendPublicKey);

            if (record.container != oldContainer)
            {
                m_positionByContainer.erase(oldContainer);
                m_positionByContainer.emplace(record.container, position);
            }

            return true;
        }

        bool eraseBySpendKey(const Crypto::PublicKey &spendPublicKey)
        {
            const auto it = m_positionBySpendKey.find(spendPublicKey);

            if (it == m_positionBySpendKey.end())
            {
                return false;
            }

            m_records.erase(m_records.begin() + static_cast<std::ptrdiff_t>(it->second));

            rebuildIndices();

            return true;
        }

        const std::vector<WalletRecord> &entries() const
        {
            return m_records;
        }

      private:
        void rebuildIndices()
        {
            m_positionBySpendKey.clear();
            m_positionByContainer.clear();

            for (size_t i = 0; i < m_records.size(); i++)
            {
                m_positionBySpendKey.emplace(m_records[i].spendPublicKey, i);
                m_positionByContainer.emplace(m_records[i].container, i);
            }
        }

        std::vector<WalletRecord> m_records;

        std::unordered_map<Crypto::PublicKey, size_t> m_positionBySpendKey;

        std::unordered_map<const CryptoNote::ITransfersContainer *, size_t> m_positionByContainer;
    };

    struct UnlockTransactionJob
    {
        uint32_t blockHeight;
        CryptoNote::ITransfersContainer *container;
        Crypto::Hash transactionHash;
    };

    /* ordered_non_unique on block height + hashed_non_unique on transaction
       hash. Both are range queries; nothing indexes uniquely. */
    class UnlockTransactionJobs
    {
      public:
        typedef std::list<UnlockTransactionJob> Jobs;

        typedef std::multimap<uint32_t, Jobs::iterator> ByBlockHeight;

        void insert(UnlockTransactionJob job)
        {
            const uint32_t blockHeight = job.blockHeight;
            const Crypto::Hash transactionHash = job.transactionHash;

            const auto it = m_jobs.insert(m_jobs.end(), std::move(job));

            m_byBlockHeight.emplace(blockHeight, it);
            m_byTransactionHash.emplace(transactionHash, it);
        }

        /* Every job at or below blockHeight, in block height order. */
        std::vector<UnlockTransactionJob> jobsUpTo(uint32_t blockHeight) const
        {
            std::vector<UnlockTransactionJob> jobs;

            for (auto it = m_byBlockHeight.begin();
                 it != m_byBlockHeight.end() && it->first <= blockHeight;
                 ++it)
            {
                jobs.push_back(*it->second);
            }

            return jobs;
        }

        /* Drops every job at or below blockHeight. */
        void eraseUpTo(uint32_t blockHeight)
        {
            auto it = m_byBlockHeight.begin();

            while (it != m_byBlockHeight.end() && it->first <= blockHeight)
            {
                eraseFromTransactionHash(it->second);
                m_jobs.erase(it->second);
                it = m_byBlockHeight.erase(it);
            }
        }

        void eraseByTransactionHash(const Crypto::Hash &transactionHash)
        {
            auto range = m_byTransactionHash.equal_range(transactionHash);

            for (auto it = range.first; it != range.second;)
            {
                const auto job = it->second;

                eraseFromBlockHeight(job);
                m_jobs.erase(job);

                it = m_byTransactionHash.erase(it);
            }
        }

        void eraseByContainer(const CryptoNote::ITransfersContainer *container)
        {
            for (auto it = m_byBlockHeight.begin(); it != m_byBlockHeight.end();)
            {
                if (it->second->container == container)
                {
                    eraseFromTransactionHash(it->second);
                    m_jobs.erase(it->second);
                    it = m_byBlockHeight.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        size_t size() const
        {
            return m_jobs.size();
        }

        Jobs::const_iterator begin() const
        {
            return m_jobs.begin();
        }

        Jobs::const_iterator end() const
        {
            return m_jobs.end();
        }

        void clear()
        {
            m_jobs.clear();
            m_byBlockHeight.clear();
            m_byTransactionHash.clear();
        }

      private:
        void eraseFromBlockHeight(Jobs::iterator job)
        {
            auto range = m_byBlockHeight.equal_range(job->blockHeight);

            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second == job)
                {
                    m_byBlockHeight.erase(it);
                    return;
                }
            }
        }

        void eraseFromTransactionHash(Jobs::iterator job)
        {
            auto range = m_byTransactionHash.equal_range(job->transactionHash);

            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second == job)
                {
                    m_byTransactionHash.erase(it);
                    return;
                }
            }
        }

        Jobs m_jobs;

        ByBlockHeight m_byBlockHeight;

        std::unordered_multimap<Crypto::Hash, Jobs::iterator> m_byTransactionHash;
    };

    /* random_access + hashed_unique on transaction hash + ordered_non_unique
       on block height.

       Transactions are never erased - transactionId is the position in this
       vector and stays valid for the life of the wallet - so the position maps
       never need rebuilding. blockHeight is an index key and modify() changes
       it, so updateAt() repairs that index. */
    class WalletTransactions
    {
      public:
        typedef std::vector<CryptoNote::WalletTransaction>::const_iterator const_iterator;

        bool push_back(CryptoNote::WalletTransaction transaction)
        {
            if (m_positionByHash.count(transaction.hash) > 0)
            {
                return false;
            }

            const Crypto::Hash hash = transaction.hash;
            const uint32_t blockHeight = transaction.blockHeight;
            const size_t position = m_transactions.size();

            m_transactions.push_back(std::move(transaction));
            m_positionByHash.emplace(hash, position);
            m_positionByBlockHeight.emplace(blockHeight, position);

            return true;
        }

        const CryptoNote::WalletTransaction &operator[](size_t position) const
        {
            return m_transactions[position];
        }

        const CryptoNote::WalletTransaction &at(size_t position) const
        {
            return m_transactions.at(position);
        }

        const_iterator begin() const
        {
            return m_transactions.begin();
        }

        const_iterator end() const
        {
            return m_transactions.end();
        }

        size_t size() const
        {
            return m_transactions.size();
        }

        bool empty() const
        {
            return m_transactions.empty();
        }

        void clear()
        {
            m_transactions.clear();
            m_positionByHash.clear();
            m_positionByBlockHeight.clear();
        }

        void reserve(size_t count)
        {
            m_transactions.reserve(count);
        }

        /* Positions of every transaction at exactly blockHeight, in the
           order the ordered index produced them. */
        std::vector<size_t> positionsAtBlockHeight(uint32_t blockHeight) const
        {
            std::vector<size_t> positions;

            auto range = m_positionByBlockHeight.equal_range(blockHeight);

            for (auto it = range.first; it != range.second; ++it)
            {
                positions.push_back(it->second);
            }

            return positions;
        }

        /* Positions of every transaction at or above blockHeight, in block
           height order - the tail of the ordered index from lower_bound. */
        std::vector<size_t> positionsFromBlockHeight(uint32_t blockHeight) const
        {
            std::vector<size_t> positions;

            for (auto it = m_positionByBlockHeight.lower_bound(blockHeight);
                 it != m_positionByBlockHeight.end();
                 ++it)
            {
                positions.push_back(it->second);
            }

            return positions;
        }

        const CryptoNote::WalletTransaction *findByHash(const Crypto::Hash &hash) const
        {
            const auto it = m_positionByHash.find(hash);

            return it == m_positionByHash.end() ? nullptr : &m_transactions[it->second];
        }

        /* Replaces project<RandomAccessIndex>() + std::distance. */
        std::optional<size_t> positionOf(const Crypto::Hash &hash) const
        {
            const auto it = m_positionByHash.find(hash);

            return it == m_positionByHash.end() ? std::nullopt : std::optional<size_t>(it->second);
        }

        /* Mutates in place and repairs the block height index if that key
           changed. The transaction hash is the primary key and may not
           change. */
        template<typename F> bool updateAt(size_t position, F modifier)
        {
            if (position >= m_transactions.size())
            {
                return false;
            }

            CryptoNote::WalletTransaction &transaction = m_transactions[position];

            const Crypto::Hash hash = transaction.hash;
            const uint32_t oldBlockHeight = transaction.blockHeight;

            modifier(transaction);

            assert(transaction.hash == hash);

            if (transaction.blockHeight != oldBlockHeight)
            {
                eraseFromBlockHeight(oldBlockHeight, position);
                m_positionByBlockHeight.emplace(transaction.blockHeight, position);
            }

            return true;
        }

        const std::vector<CryptoNote::WalletTransaction> &entries() const
        {
            return m_transactions;
        }

      private:
        void eraseFromBlockHeight(uint32_t blockHeight, size_t position)
        {
            auto range = m_positionByBlockHeight.equal_range(blockHeight);

            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second == position)
                {
                    m_positionByBlockHeight.erase(it);
                    return;
                }
            }
        }

        std::vector<CryptoNote::WalletTransaction> m_transactions;

        std::unordered_map<Crypto::Hash, size_t> m_positionByHash;

        std::multimap<uint32_t, size_t> m_positionByBlockHeight;
    };

    typedef Common::FileMappedVector<EncryptedWalletRecord> ContainerStorage;

    typedef std::pair<uint64_t, CryptoNote::WalletTransfer> TransactionTransferPair;

    typedef std::vector<TransactionTransferPair> WalletTransfers;

    typedef std::map<uint64_t, CryptoNote::Transaction> UncommitedTransactions;

    /* random_access + hashed_unique on the hash itself. Only ever truncated at
       the tail (blocksRollback), so positions are stable. */
    class BlockHashesContainer
    {
      public:
        typedef std::vector<Crypto::Hash>::const_iterator const_iterator;

        bool push_back(const Crypto::Hash &blockHash)
        {
            if (m_positionByHash.count(blockHash) > 0)
            {
                return false;
            }

            m_positionByHash.emplace(blockHash, m_hashes.size());
            m_hashes.push_back(blockHash);

            return true;
        }

        const Crypto::Hash &operator[](size_t position) const
        {
            return m_hashes[position];
        }

        const Crypto::Hash &at(size_t position) const
        {
            return m_hashes.at(position);
        }

        const Crypto::Hash &back() const
        {
            return m_hashes.back();
        }

        const_iterator begin() const
        {
            return m_hashes.begin();
        }

        const_iterator end() const
        {
            return m_hashes.end();
        }

        size_t size() const
        {
            return m_hashes.size();
        }

        bool empty() const
        {
            return m_hashes.empty();
        }

        void clear()
        {
            m_hashes.clear();
            m_positionByHash.clear();
        }

        bool contains(const Crypto::Hash &blockHash) const
        {
            return m_positionByHash.count(blockHash) > 0;
        }

        /* Replaces project<BlockHeightIndex>() + std::distance. */
        std::optional<size_t> positionOf(const Crypto::Hash &blockHash) const
        {
            const auto it = m_positionByHash.find(blockHash);

            return it == m_positionByHash.end() ? std::nullopt : std::optional<size_t>(it->second);
        }

        /* Drops everything from position onwards. */
        void truncate(size_t position)
        {
            for (size_t i = position; i < m_hashes.size(); i++)
            {
                m_positionByHash.erase(m_hashes[i]);
            }

            m_hashes.erase(m_hashes.begin() + static_cast<std::ptrdiff_t>(position), m_hashes.end());
        }

        const std::vector<Crypto::Hash> &entries() const
        {
            return m_hashes;
        }

      private:
        std::vector<Crypto::Hash> m_hashes;

        std::unordered_map<Crypto::Hash, size_t> m_positionByHash;
    };

} // namespace CryptoNote
