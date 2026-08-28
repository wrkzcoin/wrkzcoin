// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ITransaction.h"
#include "ITransfersContainer.h"
#include "crypto/crypto.h"
#include "cryptonotecore/CryptoNoteBasic.h"
#include "cryptonotecore/Currency.h"
#include "logging/LoggerRef.h"
#include "serialization/CryptoNoteSerialization.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"

#include <cassert>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace CryptoNote
{
    struct TransactionOutputInformationIn;

    class SpentOutputDescriptor
    {
      public:
        SpentOutputDescriptor();

        SpentOutputDescriptor(const TransactionOutputInformationIn &transactionInfo);

        SpentOutputDescriptor(const Crypto::KeyImage *keyImage);

        void assign(const Crypto::KeyImage *keyImage);

        bool operator==(const SpentOutputDescriptor &other) const;

        size_t hash() const;

      private:
        TransactionTypes::OutputType m_type;

        union {
            const Crypto::KeyImage *m_keyImage;
            struct
            {
                uint64_t m_amount;
                uint32_t m_globalOutputIndex;
            };
        };
    };

    struct SpentOutputDescriptorHasher
    {
        size_t operator()(const SpentOutputDescriptor &descriptor) const
        {
            return descriptor.hash();
        }
    };

    struct TransactionOutputInformationIn : public TransactionOutputInformation
    {
        Crypto::KeyImage keyImage; //!< \attention Used only for TransactionTypes::OutputType::Key
    };

    struct TransactionOutputInformationEx : public TransactionOutputInformationIn
    {
        uint64_t unlockTime;

        uint32_t blockHeight;

        uint32_t transactionIndex;

        bool visible;

        SpentOutputDescriptor getSpentOutputDescriptor() const
        {
            return SpentOutputDescriptor(*this);
        }

        const Crypto::Hash &getTransactionHash() const
        {
            return transactionHash;
        }

        void serialize(CryptoNote::ISerializer &s)
        {
            s(reinterpret_cast<uint8_t &>(type), "type");
            s(amount, "");
            serializeGlobalOutputIndex(s, globalOutputIndex, "");
            s(outputInTransaction, "");
            s(transactionPublicKey, "");
            s(keyImage, "");
            s(unlockTime, "");
            serializeBlockHeight(s, blockHeight, "");
            s(transactionIndex, "");
            s(transactionHash, "");
            s(visible, "");

            if (type == TransactionTypes::OutputType::Key)
            {
                s(outputKey, "");
            }
        }
    };

    struct TransactionBlockInfo
    {
        uint32_t height;

        uint64_t timestamp;

        uint32_t transactionIndex;

        void serialize(ISerializer &s)
        {
            serializeBlockHeight(s, height, "height");
            s(timestamp, "timestamp");
            s(transactionIndex, "transactionIndex");
        }
    };

    struct SpentTransactionOutput : TransactionOutputInformationEx
    {
        TransactionBlockInfo spendingBlock;

        Crypto::Hash spendingTransactionHash;

        uint32_t inputInTransaction;

        const Crypto::Hash &getSpendingTransactionHash() const
        {
            return spendingTransactionHash;
        }

        void serialize(ISerializer &s)
        {
            TransactionOutputInformationEx::serialize(s);
            s(spendingBlock, "spendingBlock");
            s(spendingTransactionHash, "spendingTransactionHash");
            s(inputInTransaction, "inputInTransaction");
        }
    };

    enum class KeyImageState
    {
        Unconfirmed,
        Confirmed,
        Spent
    };

    struct KeyOutputInfo
    {
        KeyImageState state;
        size_t count;
    };

    class TransfersContainer : public ITransfersContainer
    {
      public:
        TransfersContainer(
            const CryptoNote::Currency &currency,
            std::shared_ptr<Logging::ILogger> logger,
            size_t transactionSpendableAge);

        bool addTransaction(
            const TransactionBlockInfo &block,
            const ITransactionReader &tx,
            const std::vector<TransactionOutputInformationIn> &transfers);

        bool deleteUnconfirmedTransaction(const Crypto::Hash &transactionHash);

        bool markTransactionConfirmed(
            const TransactionBlockInfo &block,
            const Crypto::Hash &transactionHash,
            const std::vector<uint32_t> &globalIndices);

        std::vector<Crypto::Hash> detach(uint32_t height);

        bool advanceHeight(uint32_t height);

        // ITransfersContainer
        virtual size_t transactionsCount() const override;

        virtual uint64_t balance(uint32_t flags) const override;

        virtual void getOutputs(std::vector<TransactionOutputInformation> &transfers, uint32_t flags) const override;

        virtual bool getTransactionInformation(
            const Crypto::Hash &transactionHash,
            TransactionInformation &info,
            uint64_t *amountIn = nullptr,
            uint64_t *amountOut = nullptr) const override;

        virtual std::vector<TransactionOutputInformation>
            getTransactionOutputs(const Crypto::Hash &transactionHash, uint32_t flags) const override;

        // only type flags are feasible for this function
        virtual std::vector<TransactionOutputInformation>
            getTransactionInputs(const Crypto::Hash &transactionHash, uint32_t flags) const override;

        virtual void getUnconfirmedTransactions(std::vector<Crypto::Hash> &transactions) const override;

        virtual std::vector<SpentTransactionOutput> getUnspentInputs() const override;

        virtual std::vector<SpentTransactionOutput> getSpentInputs() const override;

        // IStreamSerializable
        virtual void save(std::ostream &os) override;

        virtual void load(std::istream &in) override;

      private:
        struct ContainingTransactionIndex
        {
        };
        struct SpendingTransactionIndex
        {
        };
        struct SpentOutputDescriptorIndex
        {
        };

        /* Replaces a two-index boost::multi_index container: hashed_unique on
           transaction hash beside ordered_non_unique on block height.

           Entries live in a list because SpentOutputDescriptor stores a
           pointer into the element it describes, so element addresses have to
           stay stable for as long as the entry exists.

           Covered by ContainerTests::testTransactionMultiIndex. */
        class TransactionMultiIndex
        {
          public:
            typedef std::list<TransactionInformation> Items;

            typedef std::multimap<uint32_t, Items::iterator> ByBlockHeight;

            bool insert(TransactionInformation info)
            {
                const Crypto::Hash hash = info.transactionHash;

                if (m_byHash.count(hash) > 0)
                {
                    return false;
                }

                const uint32_t blockHeight = info.blockHeight;

                const auto it = m_items.insert(m_items.end(), std::move(info));

                m_byHash.emplace(hash, it);
                m_byBlockHeight.emplace(blockHeight, it);

                return true;
            }

            const TransactionInformation *find(const Crypto::Hash &transactionHash) const
            {
                const auto it = m_byHash.find(transactionHash);

                return it == m_byHash.end() ? nullptr : &*it->second;
            }

            size_t count(const Crypto::Hash &transactionHash) const
            {
                return m_byHash.count(transactionHash);
            }

            size_t size() const
            {
                return m_byHash.size();
            }

            bool erase(const Crypto::Hash &transactionHash)
            {
                const auto it = m_byHash.find(transactionHash);

                if (it == m_byHash.end())
                {
                    return false;
                }

                eraseFromBlockHeight(it->second);

                m_items.erase(it->second);
                m_byHash.erase(it);

                return true;
            }

            /* Replaces an entry, moving it in the block height index if that
               key changed. The transaction hash is the unique key and cannot
               change, which callers rely on. */
            bool update(const Crypto::Hash &transactionHash, const TransactionInformation &updated)
            {
                const auto it = m_byHash.find(transactionHash);

                if (it == m_byHash.end())
                {
                    return false;
                }

                assert(updated.transactionHash == transactionHash);

                const auto item = it->second;

                if (item->blockHeight != updated.blockHeight)
                {
                    eraseFromBlockHeight(item);
                    m_byBlockHeight.emplace(updated.blockHeight, item);
                }

                *item = updated;

                return true;
            }

            ByBlockHeight::iterator blockHeightBegin()
            {
                return m_byBlockHeight.begin();
            }

            ByBlockHeight::iterator blockHeightEnd()
            {
                return m_byBlockHeight.end();
            }

            /* Removes the entry a block height iterator refers to from every
               index and returns the following block height iterator. */
            ByBlockHeight::iterator eraseAt(ByBlockHeight::iterator it)
            {
                const auto item = it->second;

                m_byHash.erase(item->transactionHash);
                m_items.erase(item);

                return m_byBlockHeight.erase(it);
            }

            Items::const_iterator begin() const
            {
                return m_items.begin();
            }

            Items::const_iterator end() const
            {
                return m_items.end();
            }

            std::vector<TransactionInformation> entries() const
            {
                return std::vector<TransactionInformation>(m_items.begin(), m_items.end());
            }

            void assign(std::vector<TransactionInformation> infos)
            {
                m_items.clear();
                m_byHash.clear();
                m_byBlockHeight.clear();

                for (auto &info : infos)
                {
                    insert(std::move(info));
                }
            }

          private:
            void eraseFromBlockHeight(Items::iterator item)
            {
                auto range = m_byBlockHeight.equal_range(item->blockHeight);

                for (auto it = range.first; it != range.second; ++it)
                {
                    if (it->second == item)
                    {
                        m_byBlockHeight.erase(it);
                        return;
                    }
                }
            }

            Items m_items;

            std::unordered_map<Crypto::Hash, Items::iterator> m_byHash;

            ByBlockHeight m_byBlockHeight;
        };

        /* An iterator over one hash index that dereferences straight to the
           element rather than to the map's value_type, so call sites keep
           writing transferIt->amount and *transferIt unchanged. */
        template<typename MapIterator, typename Element> class TransferIndexIterator
        {
          public:
            typedef std::forward_iterator_tag iterator_category;

            typedef Element value_type;

            typedef std::ptrdiff_t difference_type;

            typedef Element *pointer;

            typedef Element &reference;

            TransferIndexIterator() {}

            explicit TransferIndexIterator(MapIterator it): m_it(it) {}

            Element &operator*() const
            {
                return *m_it->second;
            }

            Element *operator->() const
            {
                return &*m_it->second;
            }

            TransferIndexIterator &operator++()
            {
                ++m_it;
                return *this;
            }

            TransferIndexIterator operator++(int)
            {
                TransferIndexIterator copy = *this;
                ++m_it;
                return copy;
            }

            bool operator==(const TransferIndexIterator &other) const
            {
                return m_it == other.m_it;
            }

            bool operator!=(const TransferIndexIterator &other) const
            {
                return m_it != other.m_it;
            }

            MapIterator base() const
            {
                return m_it;
            }

          private:
            MapIterator m_it;
        };

        /* Replaces the two identically shaped boost::multi_index containers
           holding unconfirmed and available transfers: hashed_non_unique on
           the spent output descriptor beside hashed_non_unique on the
           containing transaction hash. Both are hash indices, so there is no
           iteration order to preserve.

           Entries live in a list because SpentOutputDescriptor holds a pointer
           into the element it describes - the descriptors used as keys here
           point at the key image of the entry they are filed under, so the
           element has to outlive the index entry and must not move.

           Covered by ContainerTests::testTransferMultiIndex. */
        class TransferMultiIndex
        {
          public:
            typedef std::list<TransactionOutputInformationEx> Items;

            typedef std::unordered_multimap<SpentOutputDescriptor, Items::iterator, SpentOutputDescriptorHasher>
                ByDescriptor;

            typedef std::unordered_multimap<Crypto::Hash, Items::iterator> ByTransaction;

            typedef TransferIndexIterator<ByDescriptor::iterator, TransactionOutputInformationEx> DescriptorIterator;

            typedef TransferIndexIterator<ByTransaction::iterator, TransactionOutputInformationEx> TransactionIterator;

            /* const views, for the read-only query methods. */
            typedef TransferIndexIterator<ByDescriptor::const_iterator, const TransactionOutputInformationEx>
                ConstDescriptorIterator;

            typedef TransferIndexIterator<ByTransaction::const_iterator, const TransactionOutputInformationEx>
                ConstTransactionIterator;

            /* Neither index is unique, so this never fails. The bool is kept
               because callers assert on it. */
            std::pair<TransactionIterator, bool> insert(TransactionOutputInformationEx info)
            {
                const Crypto::Hash transactionHash = info.transactionHash;

                const auto item = m_items.insert(m_items.end(), std::move(info));

                m_byDescriptor.emplace(item->getSpentOutputDescriptor(), item);

                const auto inserted = m_byTransaction.emplace(transactionHash, item);

                return {TransactionIterator(inserted), true};
            }

            std::pair<DescriptorIterator, DescriptorIterator> rangeByDescriptor(const SpentOutputDescriptor &descriptor)
            {
                auto range = m_byDescriptor.equal_range(descriptor);

                return {DescriptorIterator(range.first), DescriptorIterator(range.second)};
            }

            std::pair<TransactionIterator, TransactionIterator> rangeByTransaction(const Crypto::Hash &transactionHash)
            {
                auto range = m_byTransaction.equal_range(transactionHash);

                return {TransactionIterator(range.first), TransactionIterator(range.second)};
            }

            std::pair<ConstDescriptorIterator, ConstDescriptorIterator>
                rangeByDescriptor(const SpentOutputDescriptor &descriptor) const
            {
                auto range = m_byDescriptor.equal_range(descriptor);

                return {ConstDescriptorIterator(range.first), ConstDescriptorIterator(range.second)};
            }

            std::pair<ConstTransactionIterator, ConstTransactionIterator>
                rangeByTransaction(const Crypto::Hash &transactionHash) const
            {
                auto range = m_byTransaction.equal_range(transactionHash);

                return {ConstTransactionIterator(range.first), ConstTransactionIterator(range.second)};
            }

            /* Removes the entry from every index and returns the following
               iterator in the descriptor index. */
            DescriptorIterator eraseByDescriptor(DescriptorIterator it)
            {
                const auto item = it.base()->second;

                eraseFromTransaction(item);

                const auto next = m_byDescriptor.erase(it.base());

                m_items.erase(item);

                return DescriptorIterator(next);
            }

            /* Removes the entry from every index and returns the following
               iterator in the transaction index. */
            TransactionIterator eraseByTransaction(TransactionIterator it)
            {
                const auto item = it.base()->second;

                eraseFromDescriptor(item);

                const auto next = m_byTransaction.erase(it.base());

                m_items.erase(item);

                return TransactionIterator(next);
            }

            /* Replaces an entry in place. Only the visible flag is ever
               changed here, and it is not part of either index key, so the
               indices do not move - which is also why the iterators the
               caller is holding stay valid, as they did with multi_index. */
            void replace(DescriptorIterator it, const TransactionOutputInformationEx &updated)
            {
                assert(updated.transactionHash == it->transactionHash);

                *it.base()->second = updated;
            }

            size_t size() const
            {
                return m_byTransaction.size();
            }

            Items::const_iterator begin() const
            {
                return m_items.begin();
            }

            Items::const_iterator end() const
            {
                return m_items.end();
            }

            std::vector<TransactionOutputInformationEx> entries() const
            {
                return std::vector<TransactionOutputInformationEx>(m_items.begin(), m_items.end());
            }

            void assign(std::vector<TransactionOutputInformationEx> infos)
            {
                m_items.clear();
                m_byDescriptor.clear();
                m_byTransaction.clear();

                for (auto &info : infos)
                {
                    insert(std::move(info));
                }
            }

          private:
            void eraseFromTransaction(Items::iterator item)
            {
                auto range = m_byTransaction.equal_range(item->transactionHash);

                for (auto it = range.first; it != range.second; ++it)
                {
                    if (it->second == item)
                    {
                        m_byTransaction.erase(it);
                        return;
                    }
                }
            }

            void eraseFromDescriptor(Items::iterator item)
            {
                auto range = m_byDescriptor.equal_range(item->getSpentOutputDescriptor());

                for (auto it = range.first; it != range.second; ++it)
                {
                    if (it->second == item)
                    {
                        m_byDescriptor.erase(it);
                        return;
                    }
                }
            }

            Items m_items;

            ByDescriptor m_byDescriptor;

            ByTransaction m_byTransaction;
        };

        /* Replaces the spent transfers container: hashed_unique on the spent
           output descriptor, plus hashed_non_unique on the containing
           transaction hash and on the spending transaction hash.

           Covered by ContainerTests::testSpentTransferMultiIndex. */
        class SpentTransferMultiIndex
        {
          public:
            typedef std::list<SpentTransactionOutput> Items;

            typedef std::unordered_map<SpentOutputDescriptor, Items::iterator, SpentOutputDescriptorHasher>
                ByDescriptor;

            typedef std::unordered_multimap<Crypto::Hash, Items::iterator> ByTransaction;

            typedef TransferIndexIterator<ByDescriptor::iterator, SpentTransactionOutput> DescriptorIterator;

            typedef TransferIndexIterator<ByTransaction::iterator, SpentTransactionOutput> TransactionIterator;

            typedef TransferIndexIterator<ByDescriptor::const_iterator, const SpentTransactionOutput>
                ConstDescriptorIterator;

            typedef TransferIndexIterator<ByTransaction::const_iterator, const SpentTransactionOutput>
                ConstTransactionIterator;

            /* The descriptor index is unique, so this can fail. */
            std::pair<TransactionIterator, bool> insert(SpentTransactionOutput output)
            {
                if (m_byDescriptor.count(output.getSpentOutputDescriptor()) > 0)
                {
                    return {TransactionIterator(m_bySpendingTransaction.end()), false};
                }

                const Crypto::Hash transactionHash = output.transactionHash;
                const Crypto::Hash spendingTransactionHash = output.spendingTransactionHash;

                const auto item = m_items.insert(m_items.end(), std::move(output));

                m_byDescriptor.emplace(item->getSpentOutputDescriptor(), item);
                m_byTransaction.emplace(transactionHash, item);

                const auto inserted = m_bySpendingTransaction.emplace(spendingTransactionHash, item);

                return {TransactionIterator(inserted), true};
            }

            std::pair<DescriptorIterator, DescriptorIterator> rangeByDescriptor(const SpentOutputDescriptor &descriptor)
            {
                auto range = m_byDescriptor.equal_range(descriptor);

                return {DescriptorIterator(range.first), DescriptorIterator(range.second)};
            }

            std::pair<TransactionIterator, TransactionIterator> rangeByTransaction(const Crypto::Hash &transactionHash)
            {
                auto range = m_byTransaction.equal_range(transactionHash);

                return {TransactionIterator(range.first), TransactionIterator(range.second)};
            }

            std::pair<TransactionIterator, TransactionIterator>
                rangeBySpendingTransaction(const Crypto::Hash &spendingTransactionHash)
            {
                auto range = m_bySpendingTransaction.equal_range(spendingTransactionHash);

                return {TransactionIterator(range.first), TransactionIterator(range.second)};
            }

            std::pair<ConstDescriptorIterator, ConstDescriptorIterator>
                rangeByDescriptor(const SpentOutputDescriptor &descriptor) const
            {
                auto range = m_byDescriptor.equal_range(descriptor);

                return {ConstDescriptorIterator(range.first), ConstDescriptorIterator(range.second)};
            }

            std::pair<ConstTransactionIterator, ConstTransactionIterator>
                rangeByTransaction(const Crypto::Hash &transactionHash) const
            {
                auto range = m_byTransaction.equal_range(transactionHash);

                return {ConstTransactionIterator(range.first), ConstTransactionIterator(range.second)};
            }

            std::pair<ConstTransactionIterator, ConstTransactionIterator>
                rangeBySpendingTransaction(const Crypto::Hash &spendingTransactionHash) const
            {
                auto range = m_bySpendingTransaction.equal_range(spendingTransactionHash);

                return {ConstTransactionIterator(range.first), ConstTransactionIterator(range.second)};
            }

            /* Removes the entry from every index and returns the following
               iterator in the spending transaction index. */
            TransactionIterator eraseBySpendingTransaction(TransactionIterator it)
            {
                const auto item = it.base()->second;

                m_byDescriptor.erase(item->getSpentOutputDescriptor());
                eraseFromTransaction(item);

                const auto next = m_bySpendingTransaction.erase(it.base());

                m_items.erase(item);

                return TransactionIterator(next);
            }

            void replace(DescriptorIterator it, const SpentTransactionOutput &updated)
            {
                *it.base()->second = updated;
            }

            void replaceBySpendingTransaction(TransactionIterator it, const SpentTransactionOutput &updated)
            {
                *it.base()->second = updated;
            }

            size_t size() const
            {
                return m_byDescriptor.size();
            }

            Items::const_iterator begin() const
            {
                return m_items.begin();
            }

            Items::const_iterator end() const
            {
                return m_items.end();
            }

            std::vector<SpentTransactionOutput> entries() const
            {
                return std::vector<SpentTransactionOutput>(m_items.begin(), m_items.end());
            }

            void assign(std::vector<SpentTransactionOutput> outputs)
            {
                m_items.clear();
                m_byDescriptor.clear();
                m_byTransaction.clear();
                m_bySpendingTransaction.clear();

                for (auto &output : outputs)
                {
                    insert(std::move(output));
                }
            }

          private:
            void eraseFromTransaction(Items::iterator item)
            {
                auto range = m_byTransaction.equal_range(item->transactionHash);

                for (auto it = range.first; it != range.second; ++it)
                {
                    if (it->second == item)
                    {
                        m_byTransaction.erase(it);
                        return;
                    }
                }
            }

            Items m_items;

            ByDescriptor m_byDescriptor;

            ByTransaction m_byTransaction;

            ByTransaction m_bySpendingTransaction;
        };

      private:
        void addTransaction(const TransactionBlockInfo &block, const ITransactionReader &tx);

        bool addTransactionOutputs(
            const TransactionBlockInfo &block,
            const ITransactionReader &tx,
            const std::vector<TransactionOutputInformationIn> &transfers);

        bool addTransactionInputs(const TransactionBlockInfo &block, const ITransactionReader &tx);

        void deleteTransactionTransfers(const Crypto::Hash &transactionHash);

        bool isSpendTimeUnlocked(uint64_t unlockTime) const;

        bool isIncluded(const TransactionOutputInformationEx &info, uint32_t flags) const;

        static bool isIncluded(TransactionTypes::OutputType type, uint32_t state, uint32_t flags);

        void updateTransfersVisibility(const Crypto::KeyImage &keyImage);

        void copyToSpent(
            const TransactionBlockInfo &block,
            const ITransactionReader &tx,
            size_t inputIndex,
            const TransactionOutputInformationEx &output);

      private:
        TransactionMultiIndex m_transactions;

        TransferMultiIndex m_unconfirmedTransfers;

        TransferMultiIndex m_availableTransfers;

        SpentTransferMultiIndex m_spentTransfers;

        uint32_t m_currentHeight; // current height is needed to check if a transfer is unlocked
        size_t m_transactionSpendableAge;

        const CryptoNote::Currency &m_currency;

        mutable std::mutex m_mutex;

        Logging::LoggerRef m_logger;
    };

} // namespace CryptoNote
