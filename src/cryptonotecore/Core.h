// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainCache.h"
#include "BlockchainMessages.h"
#include "CachedBlock.h"
#include "CachedTransaction.h"
#include "Checkpoints.h"
#include "Currency.h"
#include "IBlockchainCache.h"
#include "IBlockchainCacheFactory.h"
#include "ICore.h"
#include "ICoreInformation.h"
#include "ITransactionPool.h"
#include "ITransactionPoolCleaner.h"
#include "IUpgradeManager.h"
#include "ChainLockManager.h"
#include "InstantSendManager.h"
#include "MasternodeQuorum.h"
#include "MasternodeReward.h"
#include "MasternodeStateTracker.h"
#include "MasternodeTx.h"
#include "MessageQueue.h"
#include "TransactionValidatiorState.h"

#include <WalletTypes.h>
#include <common/FileSystemShim.h>
#include <ctime>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <logging/LoggerMessage.h>
#include <system/ContextGroup.h>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <utilities/ThreadPool.h>
#include <utilities/ThreadSafeQueue.h>
#include <vector>

namespace CryptoNote
{
    class Core : public ICore, public ICoreInformation
    {
      public:
        Core(
            const Currency &currency,
            std::shared_ptr<Logging::ILogger> logger,
            Checkpoints &&checkpoints,
            System::Dispatcher &dispatcher,
            std::unique_ptr<IBlockchainCacheFactory> &&blockchainCacheFactory,
            uint32_t transactionValidationThreads,
            std::string dataDirectory);

        virtual ~Core();

        virtual bool addMessageQueue(MessageQueue<BlockchainMessage> &messageQueue) override;

        virtual bool removeMessageQueue(MessageQueue<BlockchainMessage> &messageQueue) override;

        virtual uint32_t getTopBlockIndex() const override;

        virtual Crypto::Hash getTopBlockHash() const override;

        virtual Crypto::Hash getBlockHashByIndex(uint32_t blockIndex) const override;

        virtual uint64_t getBlockTimestampByIndex(uint32_t blockIndex) const override;

        virtual bool hasBlock(const Crypto::Hash &blockHash) const override;

        /* hasBlock without taking m_chainMutex, for callers that already hold
           it. A shared mutex is not recursive, so addBlock must use this. */
        bool hasBlockUnsafe(const Crypto::Hash &blockHash) const;

        virtual BlockTemplate getBlockByIndex(uint32_t index) const override;

        virtual BlockTemplate getBlockByHash(const Crypto::Hash &blockHash) const override;

        virtual std::vector<Crypto::Hash> buildSparseChain() const override;

        virtual std::vector<Crypto::Hash> findBlockchainSupplement(
            const std::vector<Crypto::Hash> &remoteBlockIds,
            size_t maxCount,
            uint32_t &totalBlockCount,
            uint32_t &startBlockIndex) const override;

        virtual std::vector<RawBlock> getBlocks(uint32_t minIndex, uint32_t count) const override;

        virtual void getBlocks(
            const std::vector<Crypto::Hash> &blockHashes,
            std::vector<RawBlock> &blocks,
            std::vector<Crypto::Hash> &missedHashes) const override;

        virtual bool queryBlocks(
            const std::vector<Crypto::Hash> &blockHashes,
            uint64_t timestamp,
            uint32_t &startIndex,
            uint32_t &currentIndex,
            uint32_t &fullOffset,
            std::vector<BlockFullInfo> &entries) const override;

        virtual bool queryBlocksLite(
            const std::vector<Crypto::Hash> &knownBlockHashes,
            uint64_t timestamp,
            uint32_t &startIndex,
            uint32_t &currentIndex,
            uint32_t &fullOffset,
            std::vector<BlockShortInfo> &entries) const override;

        virtual bool queryBlocksDetailed(
            const std::vector<Crypto::Hash> &knownBlockHashes,
            uint64_t timestamp,
            uint64_t &startIndex,
            uint64_t &currentIndex,
            uint64_t &fullOffset,
            std::vector<BlockDetails> &entries,
            uint32_t blockCount) const override;

        virtual bool getWalletSyncData(
            const std::vector<Crypto::Hash> &knownBlockHashes,
            const uint64_t startHeight,
            const uint64_t startTimestamp,
            const uint64_t blockCount,
            const uint64_t endHeight,
            const bool skipCoinbaseTransactions,
            const bool skipEmptyBlocks,
            std::vector<WalletTypes::WalletBlockInfo> &walletBlocks,
            std::optional<WalletTypes::TopBlock> &topBlockInfo,
            uint64_t &scannedToHeight) const override;

        virtual bool getWalletSyncStartIndex(
            const std::vector<Crypto::Hash> &knownBlockHashes,
            const uint64_t startHeight,
            const uint64_t startTimestamp,
            uint64_t &startIndex) const override;

        virtual bool getRawBlocks(
            const std::vector<Crypto::Hash> &knownBlockHashes,
            const uint64_t startHeight,
            const uint64_t startTimestamp,
            const uint64_t blockCount,
            const bool skipCoinbaseTransactions,
            std::vector<RawBlock> &walletBlocks,
            std::optional<WalletTypes::TopBlock> &topBlockInfo) const override;

        virtual bool getTransactionsStatus(
            std::unordered_set<Crypto::Hash> transactionHashes,
            std::unordered_set<Crypto::Hash> &transactionsInPool,
            std::unordered_set<Crypto::Hash> &transactionsInBlock,
            std::unordered_set<Crypto::Hash> &transactionsUnknown) const override;

        virtual bool hasTransaction(const Crypto::Hash &transactionHash) const override;

        virtual std::optional<BinaryArray> getTransaction(const Crypto::Hash &transactionHash) const override;

        virtual void getTransactions(
            const std::vector<Crypto::Hash> &transactionHashes,
            std::vector<BinaryArray> &transactions,
            std::vector<Crypto::Hash> &missedHashes) const override;

        virtual uint64_t getBlockDifficulty(uint32_t blockIndex) const override;

        virtual uint64_t getDifficultyForNextBlock() const override;

        virtual std::error_code addBlock(const CachedBlock &cachedBlock, RawBlock &&rawBlock) override;

        virtual std::error_code addBlock(RawBlock &&rawBlock) override;

        virtual std::error_code submitBlock(const BinaryArray &rawBlockTemplate) override;

        virtual bool getTransactionGlobalIndexes(
            const Crypto::Hash &transactionHash,
            std::vector<uint32_t> &globalIndexes) const override;

        virtual std::tuple<bool, std::string> getRandomOutputs(
            uint64_t amount,
            uint16_t count,
            std::vector<uint32_t> &globalIndexes,
            std::vector<Crypto::PublicKey> &publicKeys) const override;

        virtual bool getGlobalIndexesForRange(
            const uint64_t startHeight,
            const uint64_t endHeight,
            std::unordered_map<Crypto::Hash, std::vector<uint64_t>> &indexes) const override;

        virtual std::tuple<bool, std::string> addTransactionToPool(const BinaryArray &transactionBinaryArray) override;

        virtual std::vector<Crypto::Hash> getPoolTransactionHashes() const override;

        virtual std::tuple<bool, BinaryArray> getPoolTransaction(const Crypto::Hash &transactionHash) const override;

        virtual bool getPoolChanges(
            const Crypto::Hash &lastBlockHash,
            const std::vector<Crypto::Hash> &knownHashes,
            std::vector<BinaryArray> &addedTransactions,
            std::vector<Crypto::Hash> &deletedTransactions) const override;

        virtual bool getPoolChangesLite(
            const Crypto::Hash &lastBlockHash,
            const std::vector<Crypto::Hash> &knownHashes,
            std::vector<TransactionPrefixInfo> &addedTransactions,
            std::vector<Crypto::Hash> &deletedTransactions) const override;

        virtual std::tuple<bool, std::string> getBlockTemplate(
            BlockTemplate &b,
            const Crypto::PublicKey &publicViewKey,
            const Crypto::PublicKey &publicSpendKey,
            const BinaryArray &extraNonce,
            uint64_t &difficulty,
            uint32_t &height) override;

        virtual CoreStatistics getCoreStatistics() const override;

        size_t getMasternodeCount() const;

        // Returns the IDs of all masternodes that are currently eligible for rewards / quorum participation.
        std::vector<Crypto::Hash> getActiveMasternodeSet(uint32_t height) const;

        std::vector<MasternodeStateTracker::Snapshot> getMasternodeSnapshots(size_t offset, size_t limit) const;

        std::optional<MasternodeStateTracker::Snapshot> getMasternodeSnapshot(const Crypto::Hash &masternodeId) const;

        size_t getMasternodeEligibleCount(uint32_t height) const;

        Crypto::Hash getMasternodeSetHash(uint32_t height) const;

        std::optional<Crypto::Hash> getMasternodeRewardWinner(uint32_t height) const;

        /* ChainLock quorum for `height`: the active set sorted by H(mnId || hash(block height-1)).
         * Empty optional if block height-1 is not known locally (height > top + 1). */
        std::optional<std::vector<Crypto::Hash>> getChainLockQuorum(uint32_t height) const;

        /* InstantSend quorum for the current cycle (seeded by the hash of the block at the start of
         * the INSTANTSEND_QUORUM_CYCLE_BLOCKS cycle containing the tip). */
        std::optional<std::vector<Crypto::Hash>> getInstantSendQuorum() const;

        static const char *masternodeStatusToString(MasternodeStateTracker::Status status);

        // Notification callbacks (called synchronously after block/tx commit; no coroutine context needed).
        void setBlockNotifyCallback(std::function<void(uint32_t, const Crypto::Hash &)> cb);
        void setTransactionNotifyCallback(std::function<void(const Crypto::Hash &)> cb);

        // ChainLock API (thread-safe: may be called from the P2P dispatcher, RPC and signer threads).
        // Votes/locks are validated against the on-chain masternode set (registered signing key,
        // quorum membership, distinct voters, height window) before they are stored. Only
        // Added/Assembled results should be relayed.
        MasternodeVoteResult addChainLockVote(const ChainLockVote &vote);
        bool addChainLock(const ChainLock &cl);
        bool hasChainLock(uint32_t height) const;
        std::optional<ChainLock> getChainLock(uint32_t height) const;
        bool isChainLockConflict(uint32_t height, const Crypto::Hash &blockHash) const;

        // InstantSend API (thread-safe, same validation policy as ChainLock).
        MasternodeVoteResult addInstantSendVote(const InstantSendVote &vote);
        bool addInstantSendLock(const InstantSendLock &lock);
        bool isInstantSendLocked(const Crypto::KeyImage &keyImage) const;
        bool isInstantSendConflict(const Crypto::KeyImage &keyImage, const Crypto::Hash &txHash) const;
        std::optional<InstantSendLock> getInstantSendLock(const Crypto::KeyImage &keyImage) const;
        std::optional<InstantSendLock> getInstantSendLockByTxHash(const Crypto::Hash &txHash) const;

        /* Deterministic coinbase transaction secret key used whenever a block pays a masternode
         * reward: r = H("MNCB1" || height_LE4 || previousBlockHash). Every node can recompute it,
         * which is what makes the winner's stealth output verifiable in consensus. */
        static Crypto::SecretKey deriveMasternodeCoinbaseTxSecretKey(uint32_t height, const Crypto::Hash &previousBlockHash);

        /* Standard CryptoNote one-time output key for the masternode payout address:
         * P = Hs(r * A || outputIndex) * G + B, with A = payout view key, B = payout spend key. */
        static bool deriveMasternodeRewardOutputKey(
            const Crypto::PublicKey &payoutSpendKey,
            const Crypto::PublicKey &payoutViewKey,
            const Crypto::SecretKey &txSecretKey,
            size_t outputIndex,
            Crypto::PublicKey &outputKey);

        virtual std::time_t getStartTime() const;

        // ICoreInformation
        virtual size_t getPoolTransactionCount() const override;

        virtual size_t getBlockchainTransactionCount() const override;

        virtual size_t getAlternativeBlockCount() const override;

        virtual std::vector<Transaction> getPoolTransactions() const override;

        const Currency &getCurrency() const;

        virtual void save() override;

        virtual void load() override;

        virtual BlockDetails getBlockDetails(const Crypto::Hash &blockHash) const override;

        BlockDetails getBlockDetails(const uint32_t blockHeight, const uint32_t attempt = 0) const;

        virtual TransactionDetails getTransactionDetails(const Crypto::Hash &transactionHash) const override;

        virtual std::vector<Crypto::Hash>
            getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const override;

        virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash &paymentId) const override;

        static WalletTypes::RawCoinbaseTransaction getRawCoinbaseTransaction(const CryptoNote::Transaction &t);

        static WalletTypes::RawTransaction getRawTransaction(const std::vector<uint8_t> &rawTX);

        virtual std::string exportBlockchain(
            const std::string filePath,
            const uint64_t numBlocks) override;

        virtual std::tuple<Crypto::Hash, std::string> importRawBlock(
            RawBlock &rawBlock,
            const Crypto::Hash previousBlockHash,
            const uint64_t height,
            const bool lastBlock) override;

        virtual std::string importBlockchain(
            const std::string filePath,
            const bool performExpensiveValidation) override;

        virtual void rewind(const uint64_t blockIndex) override;

        virtual void addDynamicCheckpoint(uint32_t height, const Crypto::Hash &hash) override;

        size_t pruneRawBlocks(uint32_t pruneDepth);

        std::error_code compactDatabase();

        std::pair<std::error_code, std::string> compactDatabaseDetailed();

      private:
        const Currency &currency;

        System::Dispatcher &dispatcher;

        System::ContextGroup contextGroup;

        Logging::LoggerRef logger;

        Checkpoints checkpoints;

        std::unique_ptr<IUpgradeManager> upgradeManager;

        std::vector<std::unique_ptr<IBlockchainCache>> chainsStorage;

        std::vector<IBlockchainCache *> chainsLeaves;

        std::unique_ptr<ITransactionPoolCleanWrapper> transactionPool;

        std::unordered_set<IBlockchainCache *> mainChainSet;

        std::string dataFolder;

        IntrusiveLinkedList<MessageQueue<BlockchainMessage>> queueList;

        std::unique_ptr<IBlockchainCacheFactory> blockchainCacheFactory;

        Utilities::ThreadPool<bool> m_transactionValidationThreadPool;

        bool initialized;

        time_t start_time;

        size_t blockMedianSize;

        /* Tracks masternode health, fairness accounting, and spend-lock states. */
        MasternodeStateTracker masternodeStateTracker;

        /* ChainLock vote collection and finalized lock storage. */
        ChainLockManager m_chainLockManager;

        /* InstantSend lock collection and key-image lock storage. */
        InstantSendManager m_instantSendManager;

        /* Guards m_chainLockManager and m_instantSendManager. They are touched from the P2P
         * dispatcher (addBlock / handlers), the RPC threads and the MasternodeSigner threads.
         * Lock order: m_chainMutex (if needed) BEFORE m_masternodeMutex. */
        mutable std::mutex m_masternodeMutex;

        /* Reads the tracker without taking m_chainMutex — caller must hold it (shared or unique). */
        std::vector<Crypto::Hash> getActiveMasternodeSetUnlocked(uint32_t height) const;

        std::optional<Crypto::Hash> getMasternodeRewardWinnerUnlocked(uint32_t height) const;

        std::optional<std::vector<Crypto::Hash>> getChainLockQuorumUnlocked(uint32_t height) const;

        std::optional<std::vector<Crypto::Hash>> getInstantSendQuorumUnlocked() const;

        /* True if any transaction currently in the pool (other than `excludeTransactionHash`)
         * spends `keyImage`. Used to keep a Register whose collateral is being spent out of the pool. */
        bool hasKeyImageSpentInPool(const Crypto::KeyImage &keyImage, const Crypto::Hash &excludeTransactionHash) const;

        /* Membership validation shared by vote and assembled-lock ingestion. Caller holds m_chainMutex. */
        bool validateChainLockVoteMembership(
            const ChainLockVote &vote,
            const std::vector<Crypto::Hash> &quorum) const;

        bool validateInstantSendVoteMembership(
            const InstantSendVote &vote,
            const std::vector<Crypto::Hash> &quorum) const;

        /* Optional callbacks set by the daemon to forward events to MasternodeSigner. */
        std::function<void(uint32_t, const Crypto::Hash &)> m_blockNotifyCallback;
        std::function<void(const Crypto::Hash &)> m_txNotifyCallback;

        bool isMasternodeFeatureForkActive(uint32_t height) const;

        bool isMasternodeRewardForkActive(uint32_t height) const;

        std::vector<Crypto::Hash> getMasternodeRewardCandidates(uint32_t height) const;

        std::vector<Crypto::Hash>
            getMasternodeRewardCandidatesForTracker(uint32_t height, const MasternodeStateTracker &tracker) const;

        MasternodeStateTracker::RewardDistribution getMasternodeRewardDistribution(
            uint64_t totalReward,
            uint64_t totalFee,
            uint32_t height) const;

        MasternodeStateTracker::RewardDistribution getMasternodeRewardDistributionForTracker(
            uint64_t totalReward,
            uint64_t totalFee,
            uint32_t height,
            const MasternodeStateTracker &tracker) const;

        void applyMasternodeEventFromTransaction(const Transaction &transaction, uint64_t txFee, uint32_t height);

        void applyMasternodeEventFromTransactionToTracker(
            const Transaction &transaction,
            uint64_t txFee,
            uint32_t height,
            MasternodeStateTracker &tracker) const;

        void applyMasternodeEventsFromBlock(
            const CachedBlock &cachedBlock,
            const std::vector<CachedTransaction> &transactions,
            uint32_t height);

        bool buildMasternodeStateForChain(
            IBlockchainCache *cache,
            uint32_t topHeight,
            MasternodeStateTracker &tracker) const;

        void rebuildMasternodeStateFromMainChain();

        /* `blockValidatorState` (optional): key images already spent by earlier transactions of the
         * block being validated — a Register whose collateral is spent in the same block is invalid. */
        std::error_code validateMasternodeTransactionEvent(
            const CachedTransaction &cachedTransaction,
            uint64_t transactionFee,
            uint32_t nextBlockHeight,
            IBlockchainCache *validationCache,
            bool checkPoolTokenReplay,
            const TransactionValidatorState *blockValidatorState = nullptr) const;

        std::error_code validateMasternodeTransactionEventWithTracker(
            const CachedTransaction &cachedTransaction,
            uint64_t transactionFee,
            uint32_t nextBlockHeight,
            IBlockchainCache *validationCache,
            const MasternodeStateTracker &tracker,
            bool checkPoolTokenReplay,
            const TransactionValidatorState *blockValidatorState = nullptr) const;

        bool hasMasternodePayload(const Transaction &transaction) const;

        bool hasMasternodeRegistrationTokenInPool(
            const Crypto::Hash &tokenId,
            const Crypto::Hash &excludeTransactionHash) const;

        bool hasMasternodeCollateralInPool(
            uint64_t collateralAmount,
            uint32_t collateralGlobalOutputIndex,
            const Crypto::KeyImage &collateralKeyImage,
            const Crypto::Hash &excludeTransactionHash) const;

        bool hasMasternodeEndpointCommitmentInPool(
            const Crypto::Hash &endpointCommitment,
            const Crypto::Hash &excludeTransactionHash) const;

        bool hasMasternodeHeartbeatInPool(
            const Crypto::Hash &masternodeId,
            const Crypto::Hash &excludeTransactionHash) const;

        fs::path getMasternodeStateSnapshotPath() const;

        bool saveMasternodeStateSnapshot() const;

        bool loadMasternodeStateSnapshot();

        void throwIfNotInitialized() const;

        bool extractTransactions(
            const std::vector<BinaryArray> &rawTransactions,
            std::vector<CachedTransaction> &transactions,
            uint64_t &cumulativeSize);

        std::error_code validateTransaction(
            const CachedTransaction &transaction,
            TransactionValidatorState &state,
            IBlockchainCache *cache,
            Utilities::ThreadPool<bool> &threadPool,
            uint64_t &fee,
            uint32_t blockIndex,
            uint64_t blockTimestamp,
            const bool isPoolTransaction,
            const MasternodeStateTracker *masternodeTrackerOverride = nullptr);

        uint32_t findBlockchainSupplement(const std::vector<Crypto::Hash> &remoteBlockIds) const;

        /* Assumes the chain lock is held and the caller has already converted
           any start timestamp to a height. */
        uint64_t resolveWalletSyncStartIndex(
            IBlockchainCache *mainChain,
            const std::vector<Crypto::Hash> &knownBlockHashes,
            const uint64_t startHeight,
            const uint64_t timestampBlockHeight) const;

        std::vector<Crypto::Hash> getBlockHashes(uint32_t startBlockIndex, uint32_t maxCount) const;

        std::error_code validateBlock(const CachedBlock &block, IBlockchainCache *cache, uint64_t &minerReward);

        uint64_t getAdjustedTime() const;

        void updateMainChainSet();

        IBlockchainCache *findSegmentContainingBlock(const Crypto::Hash &blockHash) const;

        IBlockchainCache *findSegmentContainingBlock(uint32_t blockHeight) const;

        IBlockchainCache *findMainChainSegmentContainingBlock(const Crypto::Hash &blockHash) const;

        IBlockchainCache *findAlternativeSegmentContainingBlock(const Crypto::Hash &blockHash) const;

        IBlockchainCache *findMainChainSegmentContainingBlock(uint32_t blockIndex) const;

        IBlockchainCache *findAlternativeSegmentContainingBlock(uint32_t blockIndex) const;

        IBlockchainCache *findSegmentContainingTransaction(const Crypto::Hash &transactionHash) const;

        BlockTemplate restoreBlockTemplate(IBlockchainCache *blockchainCache, uint32_t blockIndex) const;

        std::vector<Crypto::Hash> doBuildSparseChain(const Crypto::Hash &blockHash) const;

        RawBlock getRawBlock(IBlockchainCache *segment, uint32_t blockIndex) const;

        size_t pushBlockHashes(
            uint32_t startIndex,
            uint32_t fullOffset,
            size_t maxItemsCount,
            std::vector<BlockShortInfo> &entries) const;

        size_t pushBlockHashes(
            uint32_t startIndex,
            uint32_t fullOffset,
            size_t maxItemsCount,
            std::vector<BlockFullInfo> &entries) const;

        size_t pushBlockHashes(
            uint32_t startIndex,
            uint32_t fullOffset,
            size_t maxItemsCount,
            std::vector<BlockDetails> &entries) const;

        bool notifyObservers(BlockchainMessage &&msg);

        void fillQueryBlockFullInfo(
            uint32_t fullOffset,
            uint32_t currentIndex,
            size_t maxItemsCount,
            std::vector<BlockFullInfo> &entries) const;

        void fillQueryBlockShortInfo(
            uint32_t fullOffset,
            uint32_t currentIndex,
            size_t maxItemsCount,
            std::vector<BlockShortInfo> &entries) const;

        void fillQueryBlockDetails(
            uint32_t fullOffset,
            uint32_t currentIndex,
            size_t maxItemsCount,
            std::vector<BlockDetails> &entries) const;

        void getTransactionPoolDifference(
            const std::vector<Crypto::Hash> &knownHashes,
            std::vector<Crypto::Hash> &newTransactions,
            std::vector<Crypto::Hash> &deletedTransactions) const;

        uint8_t getBlockMajorVersionForHeight(uint32_t height) const;

        size_t calculateCumulativeBlocksizeLimit(uint32_t height) const;

        /* `templateTracker` (optional): masternode state as it will be *after* the transactions
         * already placed in the template, so stateful MN rules are checked in template order. */
        bool validateBlockTemplateTransaction(
            const CachedTransaction &cachedTransaction,
            const uint64_t blockHeight,
            const MasternodeStateTracker *templateTracker);

        void fillBlockTemplate(
            BlockTemplate &block,
            const size_t medianSize,
            const size_t maxCumulativeSize,
            const uint64_t height,
            size_t &transactionsSize,
            uint64_t &fee);

        void deleteAlternativeChains();

        void pruneStaleAlternativeChains(IBlockchainCache *exclude = nullptr);

        void deleteLeaf(size_t leafIndex);

        void mergeMainChainSegments();

        void mergeSegments(IBlockchainCache *acceptingSegment, IBlockchainCache *segment);

        TransactionDetails getTransactionDetails(
            const Crypto::Hash &transactionHash,
            IBlockchainCache *segment,
            bool foundInPool) const;

        void notifyOnSuccess(
            error::AddBlockErrorCode opResult,
            uint32_t previousBlockIndex,
            const CachedBlock &cachedBlock,
            const IBlockchainCache &cache);

        void copyTransactionsToPool(IBlockchainCache *alt);

        void checkAndRemoveInvalidPoolTransactions(
            const TransactionValidatorState &blockTransactionsState);

        bool isTransactionInChain(const Crypto::Hash &txnHash);

        void transactionPoolCleaningProcedure();

        void updateBlockMedianSize();

        std::tuple<bool, std::string> addTransactionToPool(CachedTransaction &&cachedTransaction);

        std::tuple<bool, std::string> isTransactionValidForPool(
            const CachedTransaction &cachedTransaction,
            TransactionValidatorState &validatorState);

        void initRootSegment();

        void cutSegment(IBlockchainCache &segment, uint32_t startIndex);
        
        std::mutex m_submitBlockMutex;

        /* Read/write lock protecting chainsLeaves, chainsStorage, and mainChainSet.
           Writers (addBlock) take unique_lock; readers take shared_lock.
           Mutable because const reader methods need to acquire it. */
        mutable std::shared_mutex m_chainMutex;

        /* Internal implementation of getBlockDetails(hash) without locking,
           called by both public getBlockDetails overloads which hold the lock. */
        BlockDetails getBlockDetailsInternal(const Crypto::Hash &blockHash) const;
    };

} // namespace CryptoNote
