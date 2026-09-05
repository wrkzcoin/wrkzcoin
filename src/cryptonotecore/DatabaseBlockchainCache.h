// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "Currency.h"
#include "IBlockchainCache.h"
#include "common/StringView.h"
#include "cryptonotecore/UpgradeManager.h"

#include <IDataBase.h>
#include <WalletTypes.h>
#include <cryptonotecore/BlockchainReadBatch.h>
#include <cryptonotecore/BlockchainWriteBatch.h>
#include <cryptonotecore/DatabaseCacheData.h>
#include <cryptonotecore/IBlockchainCacheFactory.h>
#include <deque>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>

namespace CryptoNote
{
    /*
     * Implementation of IBlockchainCache that uses database to store internal indexes.
     * Current implementation is designed to always be the root of blockchain, ie
     * start index is always zero, parent is always nullptr, no methods
     * do recursive calls to parent.
     */
    class DatabaseBlockchainCache : public IBlockchainCache
    {
      public:
        using BlockIndex = uint32_t;
        using GlobalOutputIndex = uint32_t;
        using Amount = uint64_t;

        /*
         * Constructs new DatabaseBlockchainCache object. Currnetly, only factories that produce
         * BlockchainCache objects as children are supported.
         */
        /* liteHeight of 0 means full storage. Above zero, blocks below that height
           are stored as indexes only - key output info, key images, per amount
           output counts and block info - with the block body, transaction records,
           payment ids and timestamp indexes left out. See LITENODE.md. */
        DatabaseBlockchainCache(
            const Currency &currency,
            IDataBase &dataBase,
            IBlockchainCacheFactory &blockchainCacheFactory,
            std::shared_ptr<Logging::ILogger> logger,
            uint32_t liteHeight = 0);

        static bool checkDBSchemeVersion(IDataBase &dataBase, std::shared_ptr<Logging::ILogger> logger);

        /*
         * This methods splits cache, upper part (ie blocks with indexes larger than splitBlockIndex)
         * is copied to new BlockchainCache. Unfortunately, implementation requires return value to be of
         * BlockchainCache type.
         */
        std::unique_ptr<IBlockchainCache> split(uint32_t splitBlockIndex) override;

        void rewind(const uint64_t height) override;

        void pushBlock(
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

        /*
         * This method always returns zero
         */
        virtual uint32_t getStartBlockIndex() const override;

        virtual size_t getKeyOutputsCountForAmount(uint64_t amount, uint32_t blockIndex) const override;

        std::tuple<bool, uint64_t> getBlockHeightForTimestamp(uint64_t timestamp) const override;

        virtual uint32_t getTimestampLowerBoundBlockIndex(uint64_t timestamp) const override;

        virtual std::unordered_map<Crypto::Hash, std::vector<uint64_t>>
            getGlobalIndexes(const std::vector<Crypto::Hash> transactionHashes) const override;

        virtual bool getTransactionGlobalIndexes(
            const Crypto::Hash &transactionHash,
            std::vector<uint32_t> &globalIndexes) const override;

        virtual size_t getTransactionCount() const override;

        virtual uint32_t getBlockIndexContainingTx(const Crypto::Hash &transactionHash) const override;

        virtual size_t getChildCount() const override;

        /*
         * This method always returns nullptr
         */
        virtual IBlockchainCache *getParent() const override;

        /*
         * This method does nothing, is here only to support full interface
         */
        virtual void setParent(IBlockchainCache *ptr) override;

        virtual void addChild(IBlockchainCache *ptr) override;

        virtual bool deleteChild(IBlockchainCache *ptr) override;

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

        virtual RawBlock getBlockByIndex(uint32_t index) const override;

        virtual bool tryGetBlockByIndex(uint32_t index, RawBlock &block) const override;

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

        bool getWalletSyncBlock(
            uint32_t blockIndex,
            bool skipCoinbaseTransactions,
            WalletTypes::WalletBlockInfo &walletBlock) const;

        /* Resolves as many of the given block hashes to raw blocks as this cache
         * holds, in two database round trips for the whole set rather than
         * three point lookups per hash. Hashes this cache does not hold are
         * simply absent from the result. */
        std::unordered_map<Crypto::Hash, RawBlock>
            getRawBlocksByHashes(const std::vector<Crypto::Hash> &blockHashes) const;

        /* Batched equivalent of getWalletSyncBlock. Costs a handful of database
         * round trips for a whole range instead of two per block. Blocks that
         * are missing from the database are skipped, as they are by the single
         * block reader.
         *
         * Returns at most (endIndex - startIndex) blocks, looking as far as
         * scanEndIndex to find them. The two differ only when skipEmptyBlocks
         * is set: blocks holding nothing but a coinbase carry nothing a wallet
         * that has opted out of coinbase scanning could own, so they are left
         * out and the scan keeps going, letting one response carry the wallet
         * across far more heights than its block count.
         *
         * The last block actually looked at is always included, empty or not.
         * It is what tells the caller how far the scan reached - without it a
         * window of nothing but empty blocks would come back as an empty
         * response, which a wallet reads as "fully synced".
         *
         * The range is additionally cut short once maxResponseBytes worth of
         * block data has been assembled, so a large block count stays safe
         * during a transaction flood. At least one block is always returned. */
        std::vector<WalletTypes::WalletBlockInfo> getWalletSyncBlocks(
            uint32_t startIndex,
            uint32_t endIndex,
            uint32_t scanEndIndex,
            bool skipCoinbaseTransactions,
            bool skipEmptyBlocks,
            uint64_t maxResponseBytes,
            uint32_t &scannedToIndex) const;

        std::vector<Crypto::Hash> getTransactionHashesByBlockRange(uint64_t startHeight, uint64_t endHeight) const;

        size_t pruneStoredRawBlocks(uint32_t pruneDepth);

        /* Per table record counts and on disk byte totals, measured by walking the
           database. Used to size a lite node snapshot before committing to a
           format: the block info section is the one component whose size is known
           up front, and whether it needs shrinking depends on how the other two
           compare to it. Walking the whole database takes minutes on a synced
           chain. See LITENODE.md. */
        std::map<std::string, StorageStats> measureStorage() const;

        /* Hands every record of the index only region [0, snapshotHeight) to
           sink, in ascending key order, so a caller can write it somewhere -
           a lite node snapshot file, in the only caller there is. See
           LITESNAPSHOT.md.

           The file format deliberately lives in the daemon rather than here.
           It needs a compressor, and this library is linked into every wallet
           binary, none of which has any use for a snapshot.

           The exporting node is at some tip well above snapshotHeight and its
           tables describe that tip, so every table is filtered back to the
           height the snapshot claims to describe. Works on a full node as well
           as a lite one: key output records are normalised to the form a lite
           node would have written, which is what lets the two produce the same
           digest.

           progress is called periodically with the table being walked and how
           much of it has been seen; returning false cancels the export and
           removes the partial file. */
        SnapshotWalkStats walkSnapshotRecords(
            uint32_t snapshotHeight,
            const std::function<void(const std::string &key, const std::string &value)> &sink,
            const std::function<bool(const std::string &table, uint64_t scanned, uint64_t kept)> &progress) const;

        std::error_code compactDatabase();

        std::pair<std::error_code, std::string> compactDatabaseDetailed(bool rewriteBottommost = false);

        bool writeMasternodeStateBlob(const std::string &blob);

        bool readMasternodeStateBlob(std::string &blob) const;

      private:
        const Currency &currency;

        IDataBase &database;

        IBlockchainCacheFactory &blockchainCacheFactory;

        mutable std::optional<uint32_t> topBlockIndex;

        mutable std::optional<Crypto::Hash> topBlockHash;

        mutable std::optional<uint64_t> transactionsCount;

        mutable std::optional<uint32_t> keyOutputAmountsCount;

        mutable std::unordered_map<Amount, int32_t> keyOutputCountsForAmounts;

        std::vector<IBlockchainCache *> children;

        Logging::LoggerRef logger;

        std::deque<CachedBlockInfo> unitsCache;

        const size_t unitsCacheSize = 1000;

        /* The midnight we last confirmed has a closest-timestamp-block-index
           entry in the database. One entry is kept per day, so consecutive
           blocks during a sync all ask about the same midnight; remembering
           the answer turns a read per block into a read per day. Cleared by
           deleteClosestTimestampBlockIndex(), which is what removes them. */
        std::optional<uint64_t> knownClosestTimestampMidnight;

        /* Height at and above which full block data is stored. Zero for a normal
           node, which stores everything. */
        uint32_t liteHeight = 0;

        /* Whether the block about to be written falls in the index only region.

           Genesis is never index only. addGenesisBlock writes its raw block and
           transaction hash list directly rather than through pushBlock, so
           dropping just its transaction record would leave it half stored, and
           anything reading block zero - the sparse chain the P2P layer builds,
           getBlockHash(0), the UseGenesis paths - would find an inconsistent
           block. It is one block; keeping it whole costs nothing. */
        bool isLiteIndexOnlyHeight(uint32_t blockIndex) const
        {
            return liteHeight != 0 && blockIndex != 0 && blockIndex < liteHeight;
        }

        /* The top block's cached info, served from unitsCache when it can be
           shown to be current. */
        CachedBlockInfo getTopBlockInfo() const;

        struct ExtendedPushedBlockInfo;

        ExtendedPushedBlockInfo getExtendedPushedBlockInfo(uint32_t blockIndex) const;

        void deleteClosestTimestampBlockIndex(BlockchainWriteBatch &writeBatch, uint32_t splitBlockIndex);

        CachedBlockInfo getCachedBlockInfo(uint32_t index) const;

        BlockchainReadResult readDatabase(BlockchainReadBatch &batch) const;

        void addSpentKeyImage(const Crypto::KeyImage &keyImage, uint32_t blockIndex);

        void pushTransaction(
            const CachedTransaction &cachedTransaction,
            uint32_t blockIndex,
            uint16_t transactionBlockIndex,
            BlockchainWriteBatch &batch);

        uint32_t insertKeyOutputToGlobalIndex(
            uint64_t amount,
            PackedOutIndex output); // TODO not implemented. Should it be removed?
        uint32_t updateKeyOutputCount(Amount amount, int32_t diff) const;

        void insertPaymentId(
            BlockchainWriteBatch &batch,
            const Crypto::Hash &transactionHash,
            const Crypto::Hash &paymentId);

        void insertBlockTimestamp(BlockchainWriteBatch &batch, uint64_t timestamp, const Crypto::Hash &blockHash);

        void addGenesisBlock(CachedBlock &&genesisBlock);

        enum class OutputSearchResult : uint8_t
        {
            FOUND,
            NOT_FOUND,
            INVALID_ARGUMENT
        };

        TransactionValidatorState fillOutputsSpentByBlock(uint32_t blockIndex) const;

        Crypto::Hash pushBlockToAnotherCache(IBlockchainCache &segment, PushedBlockInfo &&pushedBlockInfo);

        void requestDeleteSpentOutputs(
            BlockchainWriteBatch &writeBatch,
            uint32_t splitBlockIndex,
            const TransactionValidatorState &spentOutputs);

        std::vector<Crypto::Hash> requestTransactionHashesFromBlockIndex(uint32_t splitBlockIndex);

        void requestDeleteTransactions(
            BlockchainWriteBatch &writeBatch,
            const std::vector<Crypto::Hash> &transactionHashes);

        void requestDeletePaymentIds(
            BlockchainWriteBatch &writeBatch,
            const std::vector<Crypto::Hash> &transactionHashes);

        void requestDeletePaymentId(BlockchainWriteBatch &writeBatch, const Crypto::Hash &paymentId, size_t toDelete);

        void requestDeleteKeyOutputs(
            BlockchainWriteBatch &writeBatch,
            const std::map<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex> &boundaries);

        void requestDeleteKeyOutputsAmount(
            BlockchainWriteBatch &writeBatch,
            IBlockchainCache::Amount amount,
            IBlockchainCache::GlobalOutputIndex boundary,
            uint32_t outputsCount);

        void requestRemoveTimestamp(BlockchainWriteBatch &batch, uint64_t timestamp, const Crypto::Hash &blockHash);

        uint8_t getBlockMajorVersionForHeight(uint32_t height) const;

        uint64_t getCachedTransactionsCount() const;

        std::vector<CachedBlockInfo> getLastCachedUnits(uint32_t blockIndex, size_t count, UseGenesis useGenesis) const;

        std::vector<CachedBlockInfo> getLastDbUnits(uint32_t blockIndex, size_t count, UseGenesis useGenesis) const;
    };
} // namespace CryptoNote
