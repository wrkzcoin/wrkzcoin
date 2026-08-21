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
    if (rawKeys.size() > 0)
    {
        std::vector<rocksdb::Slice> keySlices;
        keySlices.reserve(rawKeys.size());
        for (const std::string &key : rawKeys)
        {
            keySlices.emplace_back(rocksdb::Slice(key));
        }

        std::vector<std::string> values;
        values.reserve(rawKeys.size());
        std::vector<rocksdb::Status> statuses = db->MultiGet(readOptions, keySlices, &values);

        std::error_code error;
        std::vector<bool> resultStates;
        for (const rocksdb::Status &status : statuses)
        {
            if (!status.ok() && !status.IsNotFound())
            {
                return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
            }
            resultStates.push_back(status.ok());
        }

        batch.submitRawResult(values, resultStates);
        return std::error_code();
    } else
    {
        logger(ERROR) << "RocksDBWrapper::read: detected rawKeys.size() == 0!!!";
        return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
    }
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
    return compactDetailed().first;
}

std::pair<std::error_code, std::string> RocksDBWrapper::compactDetailed()
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    logger(INFO) << "Starting RocksDB full compaction...";

    rocksdb::CompactRangeOptions options;
    options.change_level = true;
    options.target_level = -1;

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

rocksdb::Options RocksDBWrapper::getDBOptions(const DataBaseConfig &config)
{
    rocksdb::DBOptions dbOptions;
    dbOptions.IncreaseParallelism(config.backgroundThreadsCount);
    dbOptions.info_log_level = rocksdb::InfoLogLevel::WARN_LEVEL;
    dbOptions.max_open_files = config.maxOpenFiles;
    // For spinning disk
    dbOptions.skip_stats_update_on_db_open = true;
    dbOptions.compaction_readahead_size  = 2 * 1024 * 1024;

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

    rocksdb::BlockBasedTableOptions tableOptions;
    tableOptions.block_cache = rocksdb::NewLRUCache(config.readCacheSize);

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

    /* Most of our lookups are for keys that do exist, so RocksDB can skip
     * building filters for the bottommost level and save that space. */
    fOptions.optimize_filters_for_hits = true;

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
