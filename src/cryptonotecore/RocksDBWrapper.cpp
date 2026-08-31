// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "RocksDBWrapper.h"

#include "DataBaseErrors.h"
#include "rocksdb/cache.h"
#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/table.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

using namespace CryptoNote;
using namespace Logging;

namespace
{
    const std::string DB_NAME = "DB";
}

RocksDBWrapper::RocksDBWrapper(
    std::shared_ptr<Logging::ILogger> logger,
    const DataBaseConfig &config):
    logger(logger, "RocksDBWrapper"),
    m_config(config),
    state(NOT_INITIALIZED)
{
}

RocksDBWrapper::~RocksDBWrapper() {}

void RocksDBWrapper::init()
{
    if (state.load() != NOT_INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::ALREADY_INITIALIZED));
    }

    std::string dataDir = getDataDir(m_config);

    logger(INFO) << "Opening DB in " << dataDir;

    rocksdb::DB *dbPtr;

    rocksdb::Options dbOptions = getDBOptions(m_config);
    rocksdb::Status status = rocksdb::DB::Open(dbOptions, dataDir, &dbPtr);
    if (status.ok())
    {
        logger(INFO) << "DB opened in " << dataDir;
    }
    else if (!status.ok() && status.IsInvalidArgument())
    {
        logger(INFO) << "DB not found in " << dataDir << ". Creating new DB...";
        dbOptions.create_if_missing = true;
        rocksdb::Status status = rocksdb::DB::Open(dbOptions, dataDir, &dbPtr);
        if (!status.ok())
        {
            logger(ERROR) << "DB Error. DB can't be created in " << dataDir << ". Error: " << status.ToString();
            throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR));
        }
    }
    else if (status.IsIOError())
    {
        logger(ERROR) << "DB Error. DB can't be opened in " << dataDir << ". Error: " << status.ToString();
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::IO_ERROR));
    }
    else
    {
        logger(ERROR) << "DB Error. DB can't be opened in " << dataDir << ". Error: " << status.ToString();
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR));
    }

    db.reset(dbPtr);
    state.store(INITIALIZED);
}

void RocksDBWrapper::shutdown()
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    logger(INFO) << "Closing DB.";
    rocksdb::FlushOptions flushOptions;
    flushOptions.wait = true;

    const rocksdb::Status flushStatus = db->Flush(flushOptions);
    if (!flushStatus.ok())
    {
        logger(ERROR) << "Failed to flush RocksDB on shutdown: " << flushStatus.ToString();
    }

    const rocksdb::Status walStatus = db->SyncWAL();
    if (!walStatus.ok())
    {
        logger(ERROR) << "Failed to sync RocksDB WAL on shutdown: " << walStatus.ToString();
    }

    db.reset();
    state.store(NOT_INITIALIZED);
}

void RocksDBWrapper::destroy()
{
    if (state.load() != NOT_INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::ALREADY_INITIALIZED));
    }

    std::string dataDir = getDataDir(m_config);

    logger(WARNING) << "Destroying DB in " << dataDir;

    rocksdb::Options dbOptions = getDBOptions(m_config);
    rocksdb::Status status = rocksdb::DestroyDB(dataDir, dbOptions);

    if (status.ok())
    {
        logger(WARNING) << "DB destroyed in " << dataDir;
    }
    else
    {
        logger(ERROR) << "DB Error. DB can't be destroyed in " << dataDir << ". Error: " << status.ToString();
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR));
    }
}

std::error_code RocksDBWrapper::write(IWriteBatch &batch)
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    return write(batch, true);
}

std::error_code RocksDBWrapper::write(IWriteBatch &batch, bool sync)
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    rocksdb::WriteOptions writeOptions;
    writeOptions.sync = sync;

    rocksdb::WriteBatch rocksdbBatch;
    std::vector<std::pair<std::string, std::string>> rawData(batch.extractRawDataToInsert());
    for (const std::pair<std::string, std::string> &kvPair : rawData)
    {
        rocksdbBatch.Put(rocksdb::Slice(kvPair.first), rocksdb::Slice(kvPair.second));
    }

    std::vector<std::string> rawKeys(batch.extractRawKeysToRemove());
    for (const std::string &key : rawKeys)
    {
        rocksdbBatch.Delete(rocksdb::Slice(key));
    }

    rocksdb::Status status = db->Write(writeOptions, &rocksdbBatch);

    if (!status.ok())
    {
        logger(ERROR) << "Can't write to DB. " << status.ToString();
        return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
    }
    else
    {
        return std::error_code();
    }
}

std::error_code RocksDBWrapper::read(IReadBatch &batch)
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    rocksdb::ReadOptions readOptions;

    std::vector<std::string> rawKeys(batch.getRawKeys());

    if (rawKeys.empty())
    {
        logger(ERROR) << "RocksDBWrapper::read: detected rawKeys.size() == 0!!!";
        return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
    }

    std::vector<rocksdb::Slice> keySlices;
    keySlices.reserve(rawKeys.size());
    for (const std::string &key : rawKeys)
    {
        keySlices.emplace_back(rocksdb::Slice(key));
    }

    /* The batched MultiGet, rather than the older vector overload that is a
     * loop over Get in all but name. This one sorts the keys internally,
     * visits each table file once for all the keys that land in it, and can
     * issue the reads for a level in parallel - which is what the wallet sync
     * path wants, since it asks for whole ranges of blocks at a time. */
    std::vector<rocksdb::PinnableSlice> pinnedValues(rawKeys.size());
    std::vector<rocksdb::Status> statuses(rawKeys.size());

    db->MultiGet(
        readOptions,
        db->DefaultColumnFamily(),
        keySlices.size(),
        keySlices.data(),
        pinnedValues.data(),
        statuses.data());

    std::vector<std::string> values(rawKeys.size());
    std::vector<bool> resultStates;
    resultStates.reserve(rawKeys.size());

    for (size_t i = 0; i < statuses.size(); ++i)
    {
        if (!statuses[i].ok() && !statuses[i].IsNotFound())
        {
            return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
        }

        if (statuses[i].ok())
        {
            values[i].assign(pinnedValues[i].data(), pinnedValues[i].size());
        }

        resultStates.push_back(statuses[i].ok());
    }

    batch.submitRawResult(values, resultStates);

    return std::error_code();
}

std::error_code RocksDBWrapper::readThreadSafe(IReadBatch &batch)
{
    if (state.load() != INITIALIZED)
    {
        throw std::runtime_error("Not initialized.");
    }

    rocksdb::ReadOptions readOptions;

    std::vector<std::string> rawKeys(batch.getRawKeys());

    std::vector<std::string> values(rawKeys.size());

    std::vector<bool> resultStates;

    int i = 0;

    for (const std::string &key : rawKeys)
    {
        const rocksdb::Status status = db->Get(readOptions, rocksdb::Slice(key), &values[i]);

        if (status.ok())
        {
            resultStates.push_back(true);
        }
        else
        {
            if (!status.IsNotFound())
            {
                return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
            }

            resultStates.push_back(false);
        }

        i++;
    }

    batch.submitRawResult(values, resultStates);
    return std::error_code();
}

std::error_code RocksDBWrapper::compact()
{
    return compactDetailed(false).first;
}

std::pair<std::error_code, std::string> RocksDBWrapper::compactDetailed(bool rewriteBottommost)
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    logger(INFO) << "Starting RocksDB full compaction"
                 << (rewriteBottommost ? " (rewriting the bottommost level)..." : "...");

    rocksdb::CompactRangeOptions options;
    options.change_level = true;
    options.target_level = -1;

    /* CompactRange leaves the bottommost level alone unless there is a compaction
     * filter, and there is none here. After an earlier full compaction that level
     * holds essentially the whole database, so an ordinary compaction rewrites
     * almost nothing - which is what you want for reclaiming space after deletes,
     * and useless for applying changed compression settings to data already
     * written. kForce rather than kForceOptimized because the latter skips files
     * a previous manual compaction produced, and those are exactly the ones still
     * carrying the old settings. */
    if (rewriteBottommost)
    {
        options.bottommost_level_compaction = rocksdb::BottommostLevelCompaction::kForce;
    }

    const rocksdb::Status status = db->CompactRange(options, nullptr, nullptr);
    if (!status.ok())
    {
        const std::string details = status.ToString();
        logger(ERROR) << "RocksDB compaction failed: " << details;
        return {make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR), details};
    }

    logger(INFO) << "RocksDB full compaction completed.";
    return {std::error_code(), std::string()};
}

std::error_code RocksDBWrapper::iterate(
    const std::string &keyPrefix,
    const std::function<bool(const std::string &key, const std::string &value)> &callback)
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    rocksdb::ReadOptions options;

    /* Pin a consistent view for the whole walk, so a scan running beside block
       processing cannot see half of a block's writes. Released below. */
    const rocksdb::Snapshot *snapshot = db->GetSnapshot();
    options.snapshot = snapshot;

    /* A long scan should not hold every block it touches in the block cache and
       evict the working set that block processing depends on. */
    options.fill_cache = false;

    std::error_code result;

    {
        std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options));

        for (it->Seek(keyPrefix); it->Valid(); it->Next())
        {
            const rocksdb::Slice key = it->key();

            /* Seek only positions us at the first key at or after the prefix, so
               the end of the range still has to be recognised. */
            if (key.size() < keyPrefix.size() || std::memcmp(key.data(), keyPrefix.data(), keyPrefix.size()) != 0)
            {
                break;
            }

            if (!callback(key.ToString(), it->value().ToString()))
            {
                break;
            }
        }

        if (!it->status().ok())
        {
            logger(ERROR) << "RocksDBWrapper::iterate failed: " << it->status().ToString();
            result = make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
        }
    }

    db->ReleaseSnapshot(snapshot);

    return result;
}

rocksdb::Options RocksDBWrapper::getDBOptions(const DataBaseConfig &config)
{
    rocksdb::DBOptions dbOptions;
    dbOptions.IncreaseParallelism(config.backgroundThreadsCount);
    dbOptions.info_log_level = rocksdb::InfoLogLevel::WARN_LEVEL;
    dbOptions.max_open_files = config.maxOpenFiles;
    // For spinning disk
    dbOptions.skip_stats_update_on_db_open = true;
    dbOptions.compaction_readahead_size  = 2 * 1024 * 1024;

    /* Almost every read here is a point lookup, and wallets syncing past the
     * same stretch of chain ask for the same keys over and over. A row cache
     * answers those from the finished key/value pair, skipping the block
     * decompression and index search a block cache hit still pays for. Carved
     * out of the configured read cache rather than added on top, so the memory
     * an operator asked for is the memory they get. */
    const uint64_t rowCacheSize = config.rowCachePercent > 0
        ? (config.readCacheSize / 100) * std::min<uint64_t>(config.rowCachePercent, 90)
        : config.readCacheSize / 8;

    if (rowCacheSize > 0)
    {
        dbOptions.row_cache = rocksdb::NewLRUCache(rowCacheSize);
    }

    rocksdb::ColumnFamilyOptions fOptions;
    fOptions.write_buffer_size = static_cast<size_t>(config.writeBufferSize);
    // merge two memtables when flushing to L0
    fOptions.min_write_buffer_number_to_merge = 2;
    // this means we'll use 50% extra memory in the worst case, but will reduce
    // write stalls.
    fOptions.max_write_buffer_number = 6;
    // start flushing L0->L1 as soon as possible. each file on level0 is
    // (memtable_memory_budget / 2). This will flush level 0 when it's bigger than
    // memtable_memory_budget.
    fOptions.level0_file_num_compaction_trigger = 20;

    fOptions.level0_slowdown_writes_trigger = 30;
    fOptions.level0_stop_writes_trigger = 40;

    // Keep SST files large enough to avoid excessive file churn.
    fOptions.target_file_size_base = std::max<uint64_t>(config.writeBufferSize / 2, 8ULL * 1024 * 1024);
    // Keep L1 reasonably sized relative to the memtable budget.
    fOptions.max_bytes_for_level_base = std::max<uint64_t>(config.writeBufferSize * 4, 64ULL * 1024 * 1024);
    fOptions.num_levels = 7;
    fOptions.target_file_size_multiplier = 2;
    // level style compaction
    fOptions.compaction_style = rocksdb::kCompactionStyleLevel;

    fOptions.compression_per_level.resize(fOptions.num_levels);

    const auto compressionLevel = config.compressionEnabled
        ? rocksdb::kZSTD
        : rocksdb::kNoCompression;

    for (int i = 0; i < fOptions.num_levels; ++i)
    {
        // don't compress l0 & l1
        fOptions.compression_per_level[i] = (i < 2 ? rocksdb::kNoCompression : compressionLevel);
    }

    // Keep bottom-most level compressed as well when compression is enabled.
    fOptions.bottommost_compression = compressionLevel;

    /* A per-SST ZSTD dictionary. Off unless asked for: it costs training time on
     * every compaction, and what it buys depends entirely on how repetitive the
     * records are. See DataBaseConfig::compressionDictBytes for why this database
     * is a good candidate. The trainer is fed a multiple of the dictionary size,
     * which is what RocksDB recommends over sampling less. */
    if (config.compressionEnabled && config.compressionDictBytes > 0)
    {
        fOptions.compression_opts.max_dict_bytes = static_cast<uint32_t>(config.compressionDictBytes);
        fOptions.compression_opts.zstd_max_train_bytes = static_cast<uint32_t>(config.compressionDictBytes * 100);
    }

    /* bottommost_compression_opts is ignored unless enabled is set, so this has
     * to be built whenever either knob is in play - otherwise raising the level
     * would silently do nothing to the level that holds nearly all the data. */
    if (config.compressionEnabled && (config.compressionDictBytes > 0 || config.compressionLevel > 0))
    {
        fOptions.bottommost_compression_opts = fOptions.compression_opts;
        fOptions.bottommost_compression_opts.enabled = true;

        if (config.compressionLevel > 0)
        {
            fOptions.bottommost_compression_opts.level = config.compressionLevel;
        }
    }

    rocksdb::BlockBasedTableOptions tableOptions;
    tableOptions.block_cache = rocksdb::NewLRUCache(config.readCacheSize - rowCacheSize);
    tableOptions.block_size = static_cast<size_t>(config.blockSize);

    /* This workload is almost entirely point lookups - block hash to index,
     * index to raw block, transaction hash to transaction. Without a bloom
     * filter every one of those has to binary search the index block of each
     * SST at every level before it can conclude the key is not there. Ten bits
     * per key gives roughly a 1% false positive rate for a little over 1MB of
     * filter per million keys. */
    tableOptions.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10));

    /* Keep the index and filter blocks inside the block cache rather than
     * outside it, so their memory is bounded by db-read-buffer-size instead of
     * growing without limit as the chain does. Pinning the L0 ones avoids
     * re-reading the most frequently consulted filters on every lookup. */
    tableOptions.cache_index_and_filter_blocks = true;
    tableOptions.pin_l0_filter_and_index_blocks_in_cache = true;

    /* Most block and transaction hash lookups are for keys that do exist, so by
     * default RocksDB skips building filters for the bottommost level and saves
     * that space. Spent key image checks are the exception - they ask about a key
     * image exactly when it should be absent - so a node that finds transaction
     * validation slow can buy those filters back. */
    fOptions.optimize_filters_for_hits = !config.bottommostFilters;

    std::shared_ptr<rocksdb::TableFactory> tfp(NewBlockBasedTableFactory(tableOptions));
    fOptions.table_factory = tfp;

    return rocksdb::Options(dbOptions, fOptions);
}

std::string RocksDBWrapper::getDataDir(const DataBaseConfig &config)
{
    return config.dataDir + '/' + DB_NAME;
}

void RocksDBWrapper::recreate()
{
    if (state.load() == INITIALIZED)
    {
        shutdown();
    }

    destroy();
    init();
}
