// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainCache.h"
#include "CryptoNote.h"
#include "DatabaseCacheData.h"
#include "IWriteBatch.h"

namespace CryptoNote
{
    class BlockchainWriteBatch : public IWriteBatch
    {
      public:
        BlockchainWriteBatch();

        ~BlockchainWriteBatch();

        /* Writes the key image -> block index entries that double spend checks
           read. The block index -> key images list that used to go beside them is
           no longer written: a rewind reads the same key images out of the raw
           block it is undoing. */
        BlockchainWriteBatch &
            insertSpentKeyImages(uint32_t blockIndex, const std::unordered_set<Crypto::KeyImage> &spentKeyImages);

        BlockchainWriteBatch &
            insertCachedTransaction(const ExtendedTransactionInfo &transaction, uint64_t totalTxsCount);

        /* Bumps the running transaction counter without storing the transaction
           record itself. A lite node drops the records below its lite height but
           must still count them, or getBlockchainTransactionCount and the tx_count
           in /info would report only the transactions since that height. */
        BlockchainWriteBatch &insertTransactionCount(uint64_t totalTxsCount);

        BlockchainWriteBatch &insertPaymentId(
            const Crypto::Hash &transactionHash,
            const Crypto::Hash paymentId,
            uint32_t totalTxsCountForPaymentId);

        BlockchainWriteBatch &insertCachedBlock(
            const CachedBlockInfo &block,
            uint32_t blockIndex,
            const std::vector<Crypto::Hash> &blockTxs);

        /* The running total of key outputs seen for this amount, which is what
           assigns global indexes. The per-output records that used to sit beside
           it under the same prefix are gone - what they held now lives in
           KeyOutputInfo. */
        BlockchainWriteBatch &
            insertKeyOutputCountForAmount(IBlockchainCache::Amount amount, uint32_t totalOutputsCountForAmount);

        /* Sets the chain's top block index on its own.

           insertCachedBlock writes this as a side effect of every block it
           stores, which is right when blocks are pushed in order and wrong for
           anything that writes them in another one. A snapshot import writes
           them in the order the stored keys sort, which is not the order the
           blocks were mined, so it has to state the top explicitly afterwards. */
        BlockchainWriteBatch &insertLastBlockIndex(uint32_t blockIndex);

        BlockchainWriteBatch &insertRawBlock(uint32_t blockIndex, const RawBlock &block);

        BlockchainWriteBatch &insertClosestTimestampBlockIndex(uint64_t timestamp, uint32_t blockIndex);

        BlockchainWriteBatch &insertKeyOutputAmounts(
            const std::set<IBlockchainCache::Amount> &amounts,
            uint32_t totalKeyOutputAmountsCount);

        BlockchainWriteBatch &insertTimestamp(uint64_t timestamp, const std::vector<Crypto::Hash> &blockHashes);

        BlockchainWriteBatch &insertKeyOutputInfo(
            IBlockchainCache::Amount amount,
            IBlockchainCache::GlobalOutputIndex globalIndex,
            const KeyOutputInfo &outputInfo);

        BlockchainWriteBatch &
            removeSpentKeyImages(uint32_t blockIndex, const std::vector<Crypto::KeyImage> &spentKeyImages);

        BlockchainWriteBatch &removeCachedTransaction(const Crypto::Hash &transactionHash, uint64_t totalTxsCount);

        BlockchainWriteBatch &removePaymentId(const Crypto::Hash paymentId, uint32_t totalTxsCountForPaytmentId);

        BlockchainWriteBatch &removeCachedBlock(const Crypto::Hash &blockHash, uint32_t blockIndex);

        BlockchainWriteBatch &removeRawBlock(uint32_t blockIndex);

        BlockchainWriteBatch &removeClosestTimestampBlockIndex(uint64_t timestamp);

        BlockchainWriteBatch &removeTimestamp(uint64_t timestamp);

        BlockchainWriteBatch &
            removeKeyOutputInfo(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex globalIndex);

        /* Moves another batch's writes onto the end of this one and leaves it
           empty, so several blocks can share a single database write. Order is
           preserved, which is what makes a later block's value for a key - a
           running count, the chain's top index - win over an earlier one's.

           Puts and deletes are applied as two groups rather than interleaved,
           so merging batches where a later one removes what an earlier one
           wrote would come out wrong. The one caller merges blocks pushed
           during an import, which only ever insert. */
        void append(BlockchainWriteBatch &&other);

        std::vector<std::pair<std::string, std::string>> extractRawDataToInsert() override;

        std::vector<std::string> extractRawKeysToRemove() override;

      private:
        std::vector<std::pair<std::string, std::string>> rawDataToInsert;

        std::vector<std::string> rawKeysToRemove;
    };

} // namespace CryptoNote
