// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "daemon/LiteSnapshotImporter.h"

#include "daemon/LiteSnapshot.h"

#include <chrono>
#include <common/StringTools.h>
#include <config/CryptoNoteCheckpoints.h>
#include <config/CryptoNoteSnapshots.h>
#include <cryptonotecore/BlockchainReadBatch.h>
#include <cryptonotecore/BlockchainWriteBatch.h>
#include <cryptonotecore/DBUtils.h>
#include <serialization/KVBinaryCommon.h>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>

using namespace Logging;

namespace CryptoNote
{
    namespace LiteSnapshotImport
    {
        namespace
        {
            /* Records are batched into the write path this many at a time. Only
               the small derived tables come through here - the two big ones are
               ingested as table files - so this is about bounding memory, not
               about throughput. */
            constexpr size_t BLOCKS_PER_BATCH = 20000;

            /* Keys are not stored as raw strings: DB::serializeKey wraps the
               (prefix, key) pair, so a stored key begins with the serialized
               preamble rather than the prefix letter. What every key of one
               table shares is that preamble, which is what this returns.

               Derived from the serializer rather than hardcoded. Getting it
               wrong would classify every record as belonging to no table, which
               reads exactly like an empty snapshot. */
            std::string tableKeyPrefix(const std::string &tablePrefix)
            {
                const std::string probe = DB::serializeKey(tablePrefix, uint32_t {0});

                const size_t nameOffset = sizeof(KVBinaryStorageBlockHeader) + 1 /* field count */ + 1 /* name len */;
                const size_t end = nameOffset + tablePrefix.size();

                if (probe.size() < end || probe.compare(nameOffset, tablePrefix.size(), tablePrefix) != 0)
                {
                    throw std::runtime_error(
                        "Database key layout is not what the snapshot importer expects; it would file every record "
                        "under no table at all");
                }

                return probe.substr(0, end);
            }

            bool hasPrefix(const std::string &key, const std::string &prefix)
            {
                return key.size() >= prefix.size() && key.compare(0, prefix.size(), prefix) == 0;
            }

            /* The digest is the whole security of an imported snapshot, so an
               unrecognised one is refused rather than warned about. */
            bool digestIsBlessed(const uint32_t liteHeight, const Crypto::Hash &digest)
            {
                const std::string hex = Common::podToHex(digest);

                for (const auto &entry : LITE_SNAPSHOT_DIGESTS)
                {
                    if (entry.liteHeight == liteHeight && hex == entry.payloadDigest)
                    {
                        return true;
                    }
                }

                return false;
            }

            /* Reads the last block index straight out of the database, without a
               cache in the way: the importer runs before anything is allowed to
               believe it knows the chain's shape. */
            uint32_t readTopBlockIndex(IDataBase &database)
            {
                auto batch = BlockchainReadBatch().requestLastBlockIndex();

                const auto error = database.read(batch);

                if (error)
                {
                    throw std::runtime_error("Could not read the database: " + error.message());
                }

                const auto result = batch.extractResult().getLastBlockIndex();

                return result.second ? result.first : 0;
            }

            /* Everything a verifying pass learns, and that the writing pass or
               the caller then needs. */
            struct Audit
            {
                uint64_t blockInfoRecords = 0;

                uint64_t keyImageRecords = 0;

                uint64_t keyOutputRecords = 0;

                uint64_t transactionsCount = 0;
            };

            /* Checks the block info table can stand on its own, and agrees with
               what this build already knows.

               None of this touches the key outputs or the key images, which are
               98% of the payload and cannot be checked at all without the block
               bodies. The digest is what covers those. What this catches is a
               file that is mangled, truncated, built for another chain, or
               carrying a fabricated emission or difficulty schedule - cheaply,
               and before 5 GB has been written. */
            class BlockInfoAudit
            {
              public:
                void observe(const uint32_t blockIndex, const CachedBlockInfo &info)
                {
                    if (blockIndex >= m_liteHeight)
                    {
                        throw std::runtime_error(
                            "The snapshot carries block " + std::to_string(blockIndex) + ", which is at or above the "
                            + "height it claims to stop below (" + std::to_string(m_liteHeight) + ")");
                    }

                    if (m_seen[blockIndex])
                    {
                        throw std::runtime_error("The snapshot carries block " + std::to_string(blockIndex) + " twice");
                    }

                    m_seen[blockIndex] = true;
                    m_seenCount++;

                    m_byIndex[blockIndex] = {info.cumulativeDifficulty, info.alreadyGeneratedCoins,
                                             info.alreadyGeneratedTransactions};

                    const auto checkpoint = m_checkpoints.find(blockIndex);

                    if (checkpoint != m_checkpoints.end() && Common::podToHex(info.blockHash) != checkpoint->second)
                    {
                        throw std::runtime_error(
                            "The snapshot's block " + std::to_string(blockIndex) + " is "
                            + Common::podToHex(info.blockHash) + ", but this build's checkpoints say it must be "
                            + checkpoint->second + ". This snapshot is not for this chain.");
                    }
                }

                explicit BlockInfoAudit(const uint32_t liteHeight):
                    m_liteHeight(liteHeight),
                    m_seen(liteHeight, false),
                    m_byIndex(liteHeight)
                {
                    for (const auto &checkpoint : CHECKPOINTS)
                    {
                        if (checkpoint.index < liteHeight)
                        {
                            m_checkpoints.emplace(checkpoint.index, checkpoint.blockId);
                        }
                    }
                }

                /* Run once the whole table has been seen: the records arrive in
                   the key's byte order, which is not block order, so the
                   monotonic checks cannot be made as they go. */
                uint64_t finish()
                {
                    if (m_seenCount != m_liteHeight)
                    {
                        throw std::runtime_error(
                            "The snapshot carries " + std::to_string(m_seenCount) + " blocks and needs all "
                            + std::to_string(m_liteHeight) + " below its height. It is incomplete.");
                    }

                    uint64_t previousDifficulty = 0;
                    uint64_t previousCoins = 0;
                    uint64_t previousTransactions = 0;

                    for (uint32_t index = 0; index < m_liteHeight; index++)
                    {
                        const auto &entry = m_byIndex[index];

                        /* All three are running totals over the chain, so none of
                           them may ever go backwards. A snapshot that mints coins
                           or rewrites the difficulty schedule fails here. */
                        if (entry.cumulativeDifficulty < previousDifficulty)
                        {
                            throw std::runtime_error(
                                "The snapshot's cumulative difficulty falls at block " + std::to_string(index));
                        }

                        if (entry.alreadyGeneratedCoins < previousCoins)
                        {
                            throw std::runtime_error(
                                "The snapshot's generated coins fall at block " + std::to_string(index));
                        }

                        if (entry.alreadyGeneratedTransactions < previousTransactions)
                        {
                            throw std::runtime_error(
                                "The snapshot's transaction count falls at block " + std::to_string(index));
                        }

                        previousDifficulty = entry.cumulativeDifficulty;
                        previousCoins = entry.alreadyGeneratedCoins;
                        previousTransactions = entry.alreadyGeneratedTransactions;
                    }

                    return previousTransactions;
                }

              private:
                struct Totals
                {
                    uint64_t cumulativeDifficulty = 0;
                    uint64_t alreadyGeneratedCoins = 0;
                    uint64_t alreadyGeneratedTransactions = 0;
                };

                const uint32_t m_liteHeight;

                std::map<uint32_t, std::string> m_checkpoints;

                /* Flat, and sized up front. A set and a map keyed by block index
                   would hold the same 4.2M entries in about five times the
                   memory, on the laptop this whole feature exists to spare. */
                std::vector<bool> m_seen;

                uint64_t m_seenCount = 0;

                std::vector<Totals> m_byIndex;
            };
        } // namespace

        void importSnapshot(
            IDataBase &database,
            const std::string &path,
            const uint32_t liteHeight,
            const Crypto::Hash &genesisHash,
            const std::string &scratchDirectory,
            Logging::LoggerRef &logger)
        {
            const std::string blockInfoPrefix = tableKeyPrefix(DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX);
            const std::string keyImagePrefix = tableKeyPrefix(DB::KEY_IMAGE_TO_BLOCK_INDEX_PREFIX);
            const std::string keyOutputPrefix = tableKeyPrefix(DB::KEY_OUTPUT_KEY_PREFIX);

            /* ---------------------------------------------------------------
               Refusals that cost nothing, before the file is even opened twice.
               --------------------------------------------------------------- */
            const uint32_t topBlockIndex = readTopBlockIndex(database);

            if (topBlockIndex != 0)
            {
                throw std::runtime_error(
                    "This database already holds a chain up to block " + std::to_string(topBlockIndex)
                    + ". A snapshot can only be imported into an empty one - nothing has been touched. Use a new "
                      "data directory, or --resync to rebuild this one.");
            }

            LiteSnapshot::Header header;

            {
                LiteSnapshot::Reader probe(path);
                header = probe.open();
            }

            if (header.genesisHash != genesisHash)
            {
                throw std::runtime_error(
                    "That snapshot is for a chain whose genesis block is " + Common::podToHex(header.genesisHash)
                    + ", and this daemon's is " + Common::podToHex(genesisHash) + ".");
            }

            if (header.liteHeight != liteHeight)
            {
                throw std::runtime_error(
                    "That snapshot describes the chain below height " + std::to_string(header.liteHeight)
                    + ", and this daemon was started with --lite-height " + std::to_string(liteHeight)
                    + ". Restart with --lite-height " + std::to_string(header.liteHeight) + " to use it.");
            }

            if (!digestIsBlessed(header.liteHeight, header.payloadDigest))
            {
                throw std::runtime_error(
                    "This build does not recognise that snapshot. Its digest is "
                    + Common::podToHex(header.payloadDigest) + " at height " + std::to_string(header.liteHeight)
                    + ", and no digest for that height is compiled into this daemon.\n"
                      "Everything a snapshot carries below its height is taken on trust - it cannot be checked "
                      "without the block bodies you do not have - so an unrecognised one is refused. There is no "
                      "flag to override this. Use a published snapshot, or sync the chain normally.");
            }

            logger(INFO) << "Importing a lite node snapshot at height " << header.liteHeight << " from " << path;
            logger(INFO) << "Records: " << header.totalRecords() << ", digest "
                         << Common::podToHex(header.payloadDigest);

            /* ---------------------------------------------------------------
               Pass one: verify. Nothing is written.
               --------------------------------------------------------------- */
            logger(INFO) << "Checking the snapshot before writing any of it...";

            const auto verifyStarted = std::chrono::steady_clock::now();

            Audit audit;

            {
                LiteSnapshot::Reader reader(path);
                reader.open();

                BlockInfoAudit blockAudit(header.liteHeight);

                std::string key;
                std::string value;

                while (reader.next(key, value))
                {
                    if (hasPrefix(key, blockInfoPrefix))
                    {
                        std::pair<std::string, uint32_t> decodedKey;
                        DB::deserialize(key, decodedKey, DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX);

                        CachedBlockInfo info;
                        DB::deserialize(value, info, DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX);

                        blockAudit.observe(decodedKey.second, info);

                        audit.blockInfoRecords++;
                    }
                    else if (hasPrefix(key, keyImagePrefix))
                    {
                        audit.keyImageRecords++;
                    }
                    else if (hasPrefix(key, keyOutputPrefix))
                    {
                        audit.keyOutputRecords++;
                    }
                    else
                    {
                        throw std::runtime_error(
                            "The snapshot holds a record belonging to no table a snapshot may carry. Refusing it.");
                    }
                }

                if (reader.computedDigest() != header.payloadDigest)
                {
                    throw std::runtime_error(
                        "The snapshot's contents hash to " + Common::podToHex(reader.computedDigest())
                        + " but its header claims " + Common::podToHex(header.payloadDigest)
                        + ". The file is damaged or has been tampered with. Nothing has been written.");
                }

                audit.transactionsCount = blockAudit.finish();
            }

            if (audit.blockInfoRecords != header.blockInfoRecords || audit.keyImageRecords != header.keyImageRecords
                || audit.keyOutputRecords != header.keyOutputRecords)
            {
                throw std::runtime_error("The snapshot holds different record counts than its header claims.");
            }

            if (audit.transactionsCount != header.transactionsCount)
            {
                throw std::runtime_error(
                    "The snapshot's block info ends at " + std::to_string(audit.transactionsCount)
                    + " transactions and its header claims " + std::to_string(header.transactionsCount) + ".");
            }

            logger(INFO) << "Snapshot verified in "
                         << std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - verifyStarted)
                                .count()
                         << "s. Writing it now.";

            /* ---------------------------------------------------------------
               Pass two: write.

               The two big tables are handed to the storage engine as sorted
               table files, which is what makes this minutes rather than hours.
               Block info is small enough to go through the ordinary write path,
               where insertCachedBlock also writes the block hash index and the
               empty transaction hash list that a syncing lite node writes for
               every block below its height - so the result matches a natively
               synced database rather than merely working.
               --------------------------------------------------------------- */
            const auto writeStarted = std::chrono::steady_clock::now();

            LiteSnapshot::Reader reader(path);
            reader.open();

            BlockchainWriteBatch blockBatch;

            size_t blocksInBatch = 0;

            uint64_t blocksWritten = 0;

            /* Per amount output counts, rebuilt rather than carried: the
               exporting node's own counts are as of its tip, and importing those
               would corrupt the global index of every output written afterwards. */
            std::map<IBlockchainCache::Amount, uint32_t> amountCounts;

            uint64_t ingested = 0;

            const auto flushBlocks = [&]() {
                if (blocksInBatch == 0)
                {
                    return;
                }

                const auto error = database.write(blockBatch, false);

                if (error)
                {
                    throw std::runtime_error("Failed writing block info: " + error.message());
                }

                blockBatch = BlockchainWriteBatch();
                blocksInBatch = 0;
            };

            /* Pulls the next record the engine should bulk load, dealing with
               block info on the way past. Mixing the two is deliberate: the
               payload is one sorted stream and reading it twice more to separate
               the tables would cost another 5 GB of decompression for nothing. */
            const auto nextIngestRecord = [&](std::string &key, std::string &value) -> bool {
                while (reader.next(key, value))
                {
                    if (hasPrefix(key, blockInfoPrefix))
                    {
                        std::pair<std::string, uint32_t> decodedKey;
                        DB::deserialize(key, decodedKey, DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX);

                        CachedBlockInfo info;
                        DB::deserialize(value, info, DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX);

                        /* Genesis is already in the database, written in full by
                           the ordinary startup path - raw block, base
                           transaction and all - and a snapshot carries none of
                           that. Leave it alone. */
                        if (decodedKey.second != 0)
                        {
                            blockBatch.insertCachedBlock(info, decodedKey.second, {});
                            blocksInBatch++;
                            blocksWritten++;

                            if (blocksInBatch >= BLOCKS_PER_BATCH)
                            {
                                flushBlocks();

                                logger(INFO) << "  block info: " << blocksWritten << " / " << header.blockInfoRecords;
                            }
                        }

                        continue;
                    }

                    if (hasPrefix(key, keyOutputPrefix))
                    {
                        std::pair<std::string, std::pair<IBlockchainCache::Amount, IBlockchainCache::GlobalOutputIndex>>
                            decodedKey;
                        DB::deserialize(key, decodedKey, DB::KEY_OUTPUT_KEY_PREFIX);

                        amountCounts[decodedKey.second.first]++;
                    }

                    ingested++;

                    if ((ingested % 5000000) == 0)
                    {
                        logger(INFO) << "  bulk load: " << ingested << " records";
                    }

                    return true;
                }

                return false;
            };

            const auto ingestError = database.ingestSorted(scratchDirectory, nextIngestRecord);

            if (ingestError)
            {
                throw std::runtime_error(
                    "Failed bulk loading the snapshot: " + ingestError.message()
                    + ". The database is now part written and should be deleted.");
            }

            flushBlocks();

            if (reader.computedDigest() != header.payloadDigest)
            {
                throw std::runtime_error(
                    "The snapshot hashed differently on the second read than the first. The file changed underneath "
                    "the import, and the database should be deleted.");
            }

            /* ---------------------------------------------------------------
               The counters a resumed sync depends on. An off by one in the per
               amount counts silently corrupts the global index of every output
               this node ever writes, so they are cross checked against what the
               exporter believed before anything is stored.
               --------------------------------------------------------------- */
            if (amountCounts.size() != header.keyOutputAmountsCount)
            {
                throw std::runtime_error(
                    "The snapshot's key outputs cover " + std::to_string(amountCounts.size())
                    + " distinct amounts and its header claims " + std::to_string(header.keyOutputAmountsCount)
                    + ". The database is now part written and should be deleted.");
            }

            BlockchainWriteBatch counters;

            std::set<IBlockchainCache::Amount> amounts;

            for (const auto &[amount, count] : amountCounts)
            {
                counters.insertKeyOutputCountForAmount(amount, count);
                amounts.insert(amount);
            }

            /* Ids are assigned in ascending amount order here, where a syncing
               node assigns them in the order it first met each amount. Nothing
               reads them back - only the count beside them is ever read - so the
               difference is invisible, but it is the one place an imported
               database is not byte identical to a synced one. */
            counters.insertKeyOutputAmounts(amounts, static_cast<uint32_t>(amounts.size()));

            counters.insertTransactionCount(header.transactionsCount);

            /* The chain's top block, stated rather than left to whichever block
               was written last.

               insertCachedBlock sets this on every call, and the records above
               arrive in the order the stored keys sort - which is not the order
               the blocks were mined. Leaving it implicit put the top at an
               arbitrary block a couple of thousand short of the lite height, and
               the node then tried to sync forward from there over blocks it
               already held as index only records. */
            counters.insertLastBlockIndex(liteHeight - 1);

            const auto counterError = database.write(counters);

            if (counterError)
            {
                throw std::runtime_error(
                    "Failed writing the snapshot's counters: " + counterError.message()
                    + ". The database is now part written and should be deleted.");
            }

            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::steady_clock::now() - writeStarted)
                                     .count();

            logger(INFO, BRIGHT_GREEN) << "Imported " << header.totalRecords() << " records in " << seconds << "s.";
            logger(INFO) << "This node now holds the chain below height " << header.liteHeight
                         << " and will sync the rest from the network.";
        }
    } // namespace LiteSnapshotImport
} // namespace CryptoNote
