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

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
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

        typedef boost::multi_index_container<
            TransactionOutputInformationEx,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<SpentOutputDescriptorIndex>,
                    boost::multi_index::const_mem_fun<
                        TransactionOutputInformationEx,
                        SpentOutputDescriptor,
                        &TransactionOutputInformationEx::getSpentOutputDescriptor>,
                    SpentOutputDescriptorHasher>,
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<ContainingTransactionIndex>,
                    boost::multi_index::const_mem_fun<
                        TransactionOutputInformationEx,
                        const Crypto::Hash &,
                        &TransactionOutputInformationEx::getTransactionHash>>>>
            UnconfirmedTransfersMultiIndex;

        typedef boost::multi_index_container<
            TransactionOutputInformationEx,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<SpentOutputDescriptorIndex>,
                    boost::multi_index::const_mem_fun<
                        TransactionOutputInformationEx,
                        SpentOutputDescriptor,
                        &TransactionOutputInformationEx::getSpentOutputDescriptor>,
                    SpentOutputDescriptorHasher>,
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<ContainingTransactionIndex>,
                    boost::multi_index::const_mem_fun<
                        TransactionOutputInformationEx,
                        const Crypto::Hash &,
                        &TransactionOutputInformationEx::getTransactionHash>>>>
            AvailableTransfersMultiIndex;

        typedef boost::multi_index_container<
            SpentTransactionOutput,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<SpentOutputDescriptorIndex>,
                    boost::multi_index::const_mem_fun<
                        TransactionOutputInformationEx,
                        SpentOutputDescriptor,
                        &TransactionOutputInformationEx::getSpentOutputDescriptor>,
                    SpentOutputDescriptorHasher>,
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<ContainingTransactionIndex>,
                    boost::multi_index::const_mem_fun<
                        TransactionOutputInformationEx,
                        const Crypto::Hash &,
                        &SpentTransactionOutput::getTransactionHash>>,
                boost::multi_index::hashed_non_unique<
                    boost::multi_index::tag<SpendingTransactionIndex>,
                    boost::multi_index::const_mem_fun<
                        SpentTransactionOutput,
                        const Crypto::Hash &,
                        &SpentTransactionOutput::getSpendingTransactionHash>>>>
            SpentTransfersMultiIndex;

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

        UnconfirmedTransfersMultiIndex m_unconfirmedTransfers;

        AvailableTransfersMultiIndex m_availableTransfers;

        SpentTransfersMultiIndex m_spentTransfers;

        uint32_t m_currentHeight; // current height is needed to check if a transfer is unlocked
        size_t m_transactionSpendableAge;

        const CryptoNote::Currency &m_currency;

        mutable std::mutex m_mutex;

        Logging::LoggerRef m_logger;
    };

} // namespace CryptoNote
