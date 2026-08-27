// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainStorage.h"
#include "Currency.h"
#include "IBlockchainCache.h"
#include "common/StringView.h"
#include "cryptonotecore/UpgradeManager.h"

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <functional>
#include <map>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CryptoNote
{
    class ISerializer;

    struct SpentKeyImage
    {
        uint32_t blockIndex;

        Crypto::KeyImage keyImage;

        void serialize(ISerializer &s);
    };

    struct CachedTransactionInfo
    {
        uint32_t blockIndex;

        uint32_t transactionIndex;

        Crypto::Hash transactionHash;

        uint64_t unlockTime;

        std::vector<TransactionOutputTarget> outputs;

        std::vector<uint64_t> outputAmounts;

        // needed for getTransactionGlobalIndexes query
        std::vector<uint32_t> globalIndexes;

        std::vector<KeyInput> keyInputs;

        Crypto::PublicKey transactionPublicKey;

        std::string paymentId;

        void serialize(ISerializer &s);
    };


    struct OutputGlobalIndexesForAmount
    {
        uint32_t startIndex = 0;

        // 1. This container must be sorted by PackedOutIndex::blockIndex and PackedOutIndex::transactionIndex
        // 2. GlobalOutputIndex for particular output is calculated as following: startIndex + index in vector
        std::vector<PackedOutIndex> outputs;

        void serialize(ISerializer &s);
    };

    struct PaymentIdTransactionHashPair
    {
        Crypto::Hash paymentId;

        Crypto::Hash transactionHash;

        void serialize(ISerializer &s);
    };

    bool serialize(PackedOutIndex &value, Common::StringView name, CryptoNote::ISerializer &serializer);

    class DatabaseBlockchainCache;

    class BlockchainCache : public IBlockchainCache
    {
      public:
        BlockchainCache(
            const std::string &filename,
            const Currency &currency,
            std::shared_ptr<Logging::ILogger> logger,
            IBlockchainCache *parent,
            uint32_t startIndex = 0);

        // Returns upper part of segment. [this] remains lower part.
        // All of indexes on blockIndex == splitBlockIndex belong to upper part
        std::unique_ptr<IBlockchainCache> split(uint32_t splitBlockIndex) override;

        void rewind(const uint64_t height) override;

        virtual void pushBlock(
            const CachedBlock &cachedBlock,
            const std::vector<CachedTransaction> &cachedTransactions,
            const TransactionValidatorState &validatorState,
            size_t blockSize,
            uint64_t generatedCoins,
            uint64_t blockDifficulty,
            RawBlock &&rawBlock) override;

        virtual PushedBlockInfo getPushedBlockInfo(uint32_t index) const override;

        bool checkIfSpent(const Crypto::KeyImage &keyImage, uint32_t blockIndex) const override;

        bool checkIfSpent(const Crypto::KeyImage &keyImage) const override;

        bool isTransactionSpendTimeUnlocked(uint64_t unlockTime) const override;

        bool isTransactionSpendTimeUnlocked(uint64_t unlockTime, uint32_t blockIndex) const override;

        ExtractOutputKeysResult extractKeyOutputKeys(
            uint64_t amount,
            Common::ArrayView<uint32_t> globalIndexes,
            std::vector<Crypto::PublicKey> &publicKeys) const override;

        ExtractOutputKeysResult extractKeyOutputKeys(
            uint64_t amount,
            uint32_t blockIndex,
            Common::ArrayView<uint32_t> globalIndexes,
            std::vector<Crypto::PublicKey> &publicKeys) const override;

        ExtractOutputKeysResult extractKeyOtputIndexes(
            uint64_t amount,
            Common::ArrayView<uint32_t> globalIndexes,
            std::vector<PackedOutIndex> &outIndexes) const override;

        ExtractOutputKeysResult extractKeyOtputReferences(
            uint64_t amount,
            Common::ArrayView<uint32_t> globalIndexes,
            std::vector<std::pair<Crypto::Hash, size_t>> &outputReferences) const override;

        uint32_t getTopBlockIndex() const override;

        const Crypto::Hash &getTopBlockHash() const override;

        uint32_t getBlockCount() const override;

        bool hasBlock(const Crypto::Hash &blockHash) const override;

        uint32_t getBlockIndex(const Crypto::Hash &blockHash) const override;

        bool hasTransaction(const Crypto::Hash &transactionHash) const override;

        std::vector<uint64_t> getLastTimestamps(size_t count) const override;

        std::vector<uint64_t> getLastTimestamps(size_t count, uint32_t blockIndex, UseGenesis) const override;

        std::vector<uint64_t> getLastBlocksSizes(size_t count) const override;

        std::vector<uint64_t> getLastBlocksSizes(size_t count, uint32_t blockIndex, UseGenesis) const override;

        std::vector<uint64_t>
            getLastCumulativeDifficulties(size_t count, uint32_t blockIndex, UseGenesis) const override;

        std::vector<uint64_t> getLastCumulativeDifficulties(size_t count) const override;

        uint64_t getDifficultyForNextBlock() const override;

        uint64_t getDifficultyForNextBlock(uint32_t blockIndex) const override;

        virtual uint64_t getCurrentCumulativeDifficulty() const override;

        virtual uint64_t getCurrentCumulativeDifficulty(uint32_t blockIndex) const override;

        uint64_t getAlreadyGeneratedCoins() const override;

        uint64_t getAlreadyGeneratedCoins(uint32_t blockIndex) const override;

        uint64_t getAlreadyGeneratedTransactions(uint32_t blockIndex) const override;

        std::vector<uint64_t> getLastUnits(
            size_t count,
            uint32_t blockIndex,
            UseGenesis use,
            std::function<uint64_t(const CachedBlockInfo &)> pred) const override;

        Crypto::Hash getBlockHash(uint32_t blockIndex) const override;

        virtual std::vector<Crypto::Hash> getBlockHashes(uint32_t startIndex, size_t maxCount) const override;

        virtual IBlockchainCache *getParent() const override;

        virtual void setParent(IBlockchainCache *p) override;

        virtual uint32_t getStartBlockIndex() const override;

        virtual size_t getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const override;

        std::tuple<bool, uint64_t> getBlockHeightForTimestamp(uint64_t timestamp) const override;

        virtual uint32_t getTimestampLowerBoundBlockIndex(uint64_t timestamp) const override;

        virtual bool getTransactionGlobalIndexes(
            const Crypto::Hash &transactionHash,
            std::vector<uint32_t> &globalIndexes) const override;

        virtual size_t getTransactionCount() const override;

        virtual uint32_t getBlockIndexContainingTx(const Crypto::Hash &transactionHash) const override;

        virtual size_t getChildCount() const override;

        virtual void addChild(IBlockchainCache *child) override;

        virtual bool deleteChild(IBlockchainCache *) override;

        virtual void save() override;

        virtual void load() override;

        virtual std::vector<BinaryArray> getRawTransactions(
            const std::vector<Crypto::Hash> &transactions,
            std::vector<Crypto::Hash> &missedTransactions) const override;

        virtual std::vector<BinaryArray>
            getRawTransactions(const std::vector<Crypto::Hash> &transactions) const override;

        void getRawTransactions(
            const std::vector<Crypto::Hash> &transactions,
            std::vector<BinaryArray> &foundTransactions,
            std::vector<Crypto::Hash> &missedTransactions) const override;

        virtual std::unordered_map<Crypto::Hash, std::vector<uint64_t>>
            getGlobalIndexes(const std::vector<Crypto::Hash> transactionHashes) const override;

        virtual RawBlock getBlockByIndex(uint32_t index) const override;

        virtual BinaryArray getRawTransaction(uint32_t blockIndex, uint32_t transactionIndex) const override;

        virtual std::vector<Crypto::Hash> getTransactionHashes() const override;

        virtual std::vector<uint32_t>
            getRandomOutsByAmount(uint64_t amount, size_t count, uint32_t blockIndex) const override;

        virtual ExtractOutputKeysResult extractKeyOutputs(
            uint64_t amount,
            uint32_t blockIndex,
            Common::ArrayView<uint32_t> globalIndexes,
            std::function<
                ExtractOutputKeysResult(const CachedTransactionInfo &info, PackedOutIndex index, uint32_t globalIndex)>
                pred) const override;

        virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash &paymentId) const override;

        virtual std::vector<Crypto::Hash>
            getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const override;

        virtual std::vector<RawBlock>
            getBlocksByHeight(const uint64_t startHeight, const uint64_t endHeight) const override;

        virtual std::vector<RawBlock>
            getNonEmptyBlocks(const uint64_t startHeight, const size_t blockCount) const override;

      private:
        struct BlockIndexTag
        {
        };
        struct BlockHashTag
        {
        };
        struct TransactionHashTag
        {
        };
        struct KeyImageTag
        {
        };
        struct TransactionInBlockTag
        {
        };
        struct PackedOutputTag
        {
        };
        struct TimestampTag
        {
        };
        struct PaymentIdTag
        {
        };

        /* Replaces a two-index boost::multi_index container: ordered_non_unique
           on block index beside hashed_unique on key image.

           The ordered index is what the segment split walks, so a std::multimap
           carries it. std::multimap and ordered_non_unique agree on where an
           equivalent key lands (both at the upper bound of the equal range), so
           the order of several key images spent in the same block is preserved.

           Covered by ContainerTests::testSpentKeyImagesContainer. */
        class SpentKeyImagesContainer
        {
          public:
            bool insert(uint32_t blockIndex, const Crypto::KeyImage &keyImage)
            {
                /* The key image index is unique. */
                if (m_byKeyImage.count(keyImage) > 0)
                {
                    return false;
                }

                m_byBlockIndex.emplace(blockIndex, keyImage);
                m_byKeyImage.emplace(keyImage, blockIndex);

                return true;
            }

            /* The block this key image was spent at, if this segment has it. */
            std::optional<uint32_t> spentAtBlock(const Crypto::KeyImage &keyImage) const
            {
                const auto it = m_byKeyImage.find(keyImage);

                return it == m_byKeyImage.end() ? std::nullopt : std::optional<uint32_t>(it->second);
            }

            bool contains(const Crypto::KeyImage &keyImage) const
            {
                return m_byKeyImage.count(keyImage) > 0;
            }

            std::vector<Crypto::KeyImage> keyImagesSpentAt(uint32_t blockIndex) const
            {
                std::vector<Crypto::KeyImage> keyImages;

                auto range = m_byBlockIndex.equal_range(blockIndex);

                for (auto it = range.first; it != range.second; ++it)
                {
                    keyImages.push_back(it->second);
                }

                return keyImages;
            }

            /* Moves every entry at or above splitBlockIndex into other. Key
               images spent at exactly splitBlockIndex move with the upper
               segment, which is what lower_bound gave before. */
            void splitInto(SpentKeyImagesContainer &other, uint32_t splitBlockIndex)
            {
                auto lowerBound = m_byBlockIndex.lower_bound(splitBlockIndex);

                for (auto it = lowerBound; it != m_byBlockIndex.end(); ++it)
                {
                    /* insert() rather than a raw emplace, so a key image
                       already present in the target is skipped rather than
                       desyncing the two maps. A range insert on the old
                       container behaved the same way. */
                    other.insert(it->first, it->second);
                    m_byKeyImage.erase(it->second);
                }

                m_byBlockIndex.erase(lowerBound, m_byBlockIndex.end());
            }

            /* Every entry in block index order, for serialization - the order
               the old container's first index produced. */
            std::vector<SpentKeyImage> entries() const
            {
                std::vector<SpentKeyImage> all;
                all.reserve(m_byBlockIndex.size());

                for (const auto &entry : m_byBlockIndex)
                {
                    all.push_back(SpentKeyImage {entry.first, entry.second});
                }

                return all;
            }

            size_t size() const
            {
                return m_byKeyImage.size();
            }

          private:
            std::multimap<uint32_t, Crypto::KeyImage> m_byBlockIndex;

            std::unordered_map<Crypto::KeyImage, uint32_t> m_byKeyImage;
        };

        typedef boost::multi_index_container<
            CachedTransactionInfo,
            boost::multi_index::indexed_by<
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<TransactionInBlockTag>,
                    boost::multi_index::composite_key<
                        CachedTransactionInfo,
                        BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, uint32_t, blockIndex),
                        BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, uint32_t, transactionIndex)>>,
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<BlockIndexTag>,
                    BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, uint32_t, blockIndex)>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<TransactionHashTag>,
                    BOOST_MULTI_INDEX_MEMBER(CachedTransactionInfo, Crypto::Hash, transactionHash)>>>
            TransactionsCacheContainer;

        typedef boost::multi_index_container<
            CachedBlockInfo,
            boost::multi_index::indexed_by<
                // The index here is blockIndex - startIndex
                boost::multi_index::random_access<boost::multi_index::tag<BlockIndexTag>>,
                boost::multi_index::hashed_unique<
                    boost::multi_index::tag<BlockHashTag>,
                    BOOST_MULTI_INDEX_MEMBER(CachedBlockInfo, Crypto::Hash, blockHash)>,
                boost::multi_index::ordered_non_unique<
                    boost::multi_index::tag<TimestampTag>,
                    BOOST_MULTI_INDEX_MEMBER(CachedBlockInfo, uint64_t, timestamp)>>>
            BlockInfoContainer;

        /* Replaces a two-index boost::multi_index container: hashed_non_unique
           on payment id beside hashed_unique on transaction hash. Both were
           hash indices, so there is no iteration order to preserve - the old
           container's first index was itself a hash index, which is why the
           serialised order was already unspecified.

           Covered by ContainerTests::testPaymentIdContainer. */
        class PaymentIdContainer
        {
          public:
            bool insert(const PaymentIdTransactionHashPair &entry)
            {
                /* The transaction hash index is unique. */
                if (m_byTransactionHash.count(entry.transactionHash) > 0)
                {
                    return false;
                }

                m_byTransactionHash.emplace(entry.transactionHash, entry.paymentId);
                m_byPaymentId.emplace(entry.paymentId, entry.transactionHash);

                return true;
            }

            /* Removes the entry for a transaction and hands it back, so a
               caller splitting the segment can move it elsewhere. */
            std::optional<PaymentIdTransactionHashPair> extract(const Crypto::Hash &transactionHash)
            {
                const auto it = m_byTransactionHash.find(transactionHash);

                if (it == m_byTransactionHash.end())
                {
                    return std::nullopt;
                }

                const PaymentIdTransactionHashPair entry {it->second, transactionHash};

                auto range = m_byPaymentId.equal_range(it->second);

                for (auto paymentIdIt = range.first; paymentIdIt != range.second; ++paymentIdIt)
                {
                    if (paymentIdIt->second == transactionHash)
                    {
                        m_byPaymentId.erase(paymentIdIt);
                        break;
                    }
                }

                m_byTransactionHash.erase(it);

                return entry;
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

            /* Every entry, for serialization. */
            std::vector<PaymentIdTransactionHashPair> entries() const
            {
                std::vector<PaymentIdTransactionHashPair> all;
                all.reserve(m_byTransactionHash.size());

                for (const auto &entry : m_byTransactionHash)
                {
                    all.push_back(PaymentIdTransactionHashPair {entry.second, entry.first});
                }

                return all;
            }

            size_t size() const
            {
                return m_byTransactionHash.size();
            }

          private:
            std::unordered_multimap<Crypto::Hash, Crypto::Hash> m_byPaymentId;

            std::unordered_map<Crypto::Hash, Crypto::Hash> m_byTransactionHash;
        };

        typedef std::map<uint64_t, OutputGlobalIndexesForAmount> OutputsGlobalIndexesContainer;

        typedef std::map<BlockIndex, std::vector<std::pair<Amount, GlobalOutputIndex>>> OutputSpentInBlock;

        typedef std::set<std::pair<Amount, GlobalOutputIndex>> SpentOutputsOnAmount;

        const uint32_t CURRENT_SERIALIZATION_VERSION = 1;

        std::string filename;

        const Currency &currency;

        Logging::LoggerRef logger;

        IBlockchainCache *parent;

        // index of first block stored in this cache
        uint32_t startIndex;

        TransactionsCacheContainer transactions;

        SpentKeyImagesContainer spentKeyImages;

        BlockInfoContainer blockInfos;

        OutputsGlobalIndexesContainer keyOutputsGlobalIndexes;

        PaymentIdContainer paymentIds;

        std::unique_ptr<BlockchainStorage> storage;

        std::vector<IBlockchainCache *> children;

        void serialize(ISerializer &s);

        void addSpentKeyImage(const Crypto::KeyImage &keyImage, uint32_t blockIndex);

        void pushTransaction(const CachedTransaction &tx, uint32_t blockIndex, uint16_t transactionBlockIndex);

        void splitSpentKeyImages(BlockchainCache &newCache, uint32_t splitBlockIndex);

        void splitTransactions(BlockchainCache &newCache, uint32_t splitBlockIndex);

        void splitBlocks(BlockchainCache &newCache, uint32_t splitBlockIndex);

        void splitKeyOutputsGlobalIndexes(BlockchainCache &newCache, uint32_t splitBlockIndex);

        void removePaymentId(const Crypto::Hash &transactionHash, BlockchainCache &newCache);

        uint32_t insertKeyOutputToGlobalIndex(uint64_t amount, PackedOutIndex output, uint32_t blockIndex);

        enum class OutputSearchResult : uint8_t
        {
            FOUND,
            NOT_FOUND,
            INVALID_ARGUMENT
        };

        TransactionValidatorState fillOutputsSpentByBlock(uint32_t blockIndex) const;

        uint8_t getBlockMajorVersionForHeight(uint32_t height) const;

        void fixChildrenParent(IBlockchainCache *p);

        void doPushBlock(
            const CachedBlock &cachedBlock,
            const std::vector<CachedTransaction> &cachedTransactions,
            const TransactionValidatorState &validatorState,
            size_t blockSize,
            uint64_t generatedCoins,
            uint64_t blockDifficulty,
            RawBlock &&rawBlock);
    };

} // namespace CryptoNote
