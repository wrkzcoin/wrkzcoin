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

#include <functional>
#include <list>
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

        /* Replaces a three-index boost::multi_index container.

           A std::map keyed on (blockIndex, transactionIndex) serves both the
           composite hashed_unique index and the ordered_non_unique index on
           block index: lexicographic ordering on the pair subsumes ordering on
           its first element, so lower_bound({blockIndex, 0}) is the same
           boundary the block index index gave.

           Elements live in a list so that CachedTransactionInfo - which owns
           two vectors - is not copied when indices are rebuilt.

           Covered by ContainerTests::testTransactionsCacheContainer. */
        class TransactionsCacheContainer
        {
          public:
            typedef std::list<CachedTransactionInfo> Transactions;

            typedef std::pair<uint32_t, uint32_t> BlockAndIndex;

            bool insert(CachedTransactionInfo info)
            {
                const BlockAndIndex position {info.blockIndex, info.transactionIndex};
                const Crypto::Hash hash = info.transactionHash;

                /* Both the composite index and the hash index are unique. */
                if (m_byPosition.count(position) > 0 || m_byHash.count(hash) > 0)
                {
                    return false;
                }

                const auto it = m_transactions.insert(m_transactions.end(), std::move(info));

                m_byPosition.emplace(position, it);
                m_byHash.emplace(hash, it);

                return true;
            }

            const CachedTransactionInfo *findByHash(const Crypto::Hash &transactionHash) const
            {
                const auto it = m_byHash.find(transactionHash);

                return it == m_byHash.end() ? nullptr : &*it->second;
            }

            const CachedTransactionInfo *findInBlock(uint32_t blockIndex, uint32_t transactionIndex) const
            {
                const auto it = m_byPosition.find(BlockAndIndex {blockIndex, transactionIndex});

                return it == m_byPosition.end() ? nullptr : &*it->second;
            }

            bool containsHash(const Crypto::Hash &transactionHash) const
            {
                return m_byHash.count(transactionHash) > 0;
            }

            size_t size() const
            {
                return m_byHash.size();
            }

            /* The transaction hashes at or above a block index, in block order.
               splitTransactions needs these before the entries move. */
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

            /* Moves every entry at or above splitBlockIndex into other. */
            void splitInto(TransactionsCacheContainer &other, uint32_t splitBlockIndex)
            {
                auto lowerBound = m_byPosition.lower_bound(BlockAndIndex {splitBlockIndex, 0});

                for (auto it = lowerBound; it != m_byPosition.end(); ++it)
                {
                    /* insert() rather than a raw emplace so a duplicate in the
                       target is skipped instead of desyncing the indices, which
                       is what a range insert on the old container did. */
                    other.insert(*it->second);

                    m_byHash.erase(it->second->transactionHash);
                    m_transactions.erase(it->second);
                }

                m_byPosition.erase(lowerBound, m_byPosition.end());
            }

            /* Iteration for callers that want every transaction. The old
               container's first index was a hash index, so no order was
               specified then either. */
            const Transactions &all() const
            {
                return m_transactions;
            }

            std::vector<CachedTransactionInfo> entries() const
            {
                return std::vector<CachedTransactionInfo>(m_transactions.begin(), m_transactions.end());
            }

          private:
            Transactions m_transactions;

            /* Composite index and block ordering in one. */
            std::map<BlockAndIndex, Transactions::iterator> m_byPosition;

            std::unordered_map<Crypto::Hash, Transactions::iterator> m_byHash;
        };

        /* Replaces a three-index boost::multi_index container: random_access
           beside hashed_unique on block hash and ordered_non_unique on
           timestamp.

           The random_access index is a plain vector. That is safe because the
           only erase is the tail truncation in splitBlocks, so a block's
           position never shifts while it is in the container and a
           hash -> position map stays valid. It also removes the need for
           project<BlockIndexTag>(), which existed only to turn a hash lookup
           back into an ordinal position - positionOf() does that directly.

           The vector-like surface is deliberate: callers index it, take
           front()/back(), and run std::lower_bound over it.

           Covered by ContainerTests::testBlockInfoContainer. */
        class BlockInfoContainer
        {
          public:
            typedef std::vector<CachedBlockInfo>::const_iterator const_iterator;

            void push_back(CachedBlockInfo blockInfo)
            {
                const Crypto::Hash hash = blockInfo.blockHash;
                const uint64_t timestamp = blockInfo.timestamp;
                const size_t position = m_blocks.size();

                m_blocks.push_back(std::move(blockInfo));
                m_positionByHash.emplace(hash, position);
                m_positionByTimestamp.emplace(timestamp, position);
            }

            const CachedBlockInfo &operator[](size_t position) const
            {
                return m_blocks[position];
            }

            const CachedBlockInfo &at(size_t position) const
            {
                return m_blocks.at(position);
            }

            const CachedBlockInfo &front() const
            {
                return m_blocks.front();
            }

            const CachedBlockInfo &back() const
            {
                return m_blocks.back();
            }

            const_iterator begin() const
            {
                return m_blocks.begin();
            }

            const_iterator end() const
            {
                return m_blocks.end();
            }

            size_t size() const
            {
                return m_blocks.size();
            }

            bool empty() const
            {
                return m_blocks.empty();
            }

            bool containsHash(const Crypto::Hash &blockHash) const
            {
                return m_positionByHash.count(blockHash) > 0;
            }

            /* The block's offset within this segment, replacing a lookup in the
               hash index projected onto the random access index. */
            std::optional<size_t> positionOf(const Crypto::Hash &blockHash) const
            {
                const auto it = m_positionByHash.find(blockHash);

                return it == m_positionByHash.end() ? std::nullopt : std::optional<size_t>(it->second);
            }

            /* Block hashes whose timestamp falls in [begin, end], in timestamp
               order - the ordered_non_unique index's lower_bound/upper_bound
               pair. */
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

            /* Moves everything from position onwards into other. */
            void splitInto(BlockInfoContainer &other, size_t position)
            {
                for (size_t i = position; i < m_blocks.size(); i++)
                {
                    other.push_back(std::move(m_blocks[i]));
                }

                m_blocks.erase(m_blocks.begin() + static_cast<std::ptrdiff_t>(position), m_blocks.end());

                rebuildIndices();
            }

            const std::vector<CachedBlockInfo> &entries() const
            {
                return m_blocks;
            }

            void assign(std::vector<CachedBlockInfo> blocks)
            {
                m_blocks = std::move(blocks);
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

            std::vector<CachedBlockInfo> m_blocks;

            std::unordered_map<Crypto::Hash, size_t> m_positionByHash;

            std::multimap<uint64_t, size_t> m_positionByTimestamp;
        };

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
