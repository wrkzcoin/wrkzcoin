// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "LevelDBWrapper.h"

#include "DataBaseErrors.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/table.h"
#include "leveldb/write_batch.h"

using namespace CryptoNote;
using namespace Logging;

namespace
{
    const std::string DB_NAME = "LevelDB";
}

LevelDBWrapper::LevelDBWrapper(std::shared_ptr<Logging::ILogger> logger):
    logger(logger, "LevelDBWrapper"),
    state(NOT_INITIALIZED)
{
}

LevelDBWrapper::~LevelDBWrapper() {}

void LevelDBWrapper::init(const DataBaseConfig &config)
{
    // Set up database connection information and open database
    leveldb::DB* dbPtr;

    leveldb::Options dbOptions;
#if defined(USE_LEVELDB)
    logger(INFO) << "Daemon uses kSnappyCompression...";
    dbOptions.compression = leveldb::kSnappyCompression;
#else
    logger(INFO) << "Daemon uses kNoCompression...";
    dbOptions.compression = leveldb::kNoCompression;
#endif
    dbOptions.max_file_size = 1024 * 1024 * 1024; // 1024 MB

    dbOptions.write_buffer_size =  static_cast<size_t>(config.getWriteBufferSize());

    dbOptions.max_open_files =  config.getMaxOpenFiles();

    dbOptions.block_cache =  leveldb::NewLRUCache(config.getReadCacheSize());

    if (state.load() != NOT_INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::ALREADY_INITIALIZED));
    }

    std::string dataDir = getDataDir(config);

    leveldb::Status status = leveldb::DB::Open(dbOptions, dataDir, &dbPtr);

    logger(INFO) << "Opening DB in " << dataDir;

    if (true == status.ok())
    {
        logger(INFO) << "DB opened in " << dataDir;
    }
    else if (!status.ok() && status.IsInvalidArgument())
    {
        logger(INFO) << "DB not found in " << dataDir << ". Creating new DB...";
        dbOptions.create_if_missing = true;
        leveldb::Status status = leveldb::DB::Open(dbOptions, dataDir, &dbPtr);
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

void LevelDBWrapper::shutdown()
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    logger(INFO) << "Closing DB.";
	
    db.reset();

    state.store(NOT_INITIALIZED);
}

void LevelDBWrapper::destroy(const DataBaseConfig &config)
{
    if (state.load() != NOT_INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::ALREADY_INITIALIZED));
    }

    std::string dataDir = getDataDir(config);

    logger(WARNING) << "Destroying DB in " << dataDir;

    leveldb::Status status = leveldb::DestroyDB(dataDir, leveldb::Options());

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

std::error_code LevelDBWrapper::write(IWriteBatch &batch)
{
    if (state.load() != INITIALIZED)
    {
        throw std::system_error(make_error_code(CryptoNote::error::DataBaseErrorCodes::NOT_INITIALIZED));
    }

    return write(batch, false);
}

std::error_code LevelDBWrapper::write(IWriteBatch &batch, bool sync)
{
    leveldb::WriteOptions writeOptions;
    writeOptions.sync = sync;

    leveldb::WriteBatch LevelDBbBatch;
    std::vector<std::pair<std::string, std::string>> rawData(batch.extractRawDataToInsert());
    for (const std::pair<std::string, std::string> &kvPair : rawData)
    {
        LevelDBbBatch.Put(leveldb::Slice(kvPair.first), leveldb::Slice(kvPair.second));
    }

    std::vector<std::string> rawKeys(batch.extractRawKeysToRemove());
    for (const std::string &key : rawKeys)
    {
        LevelDBbBatch.Delete(leveldb::Slice(key));
    }

    leveldb::Status status = db->Write(writeOptions, &LevelDBbBatch);

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

std::error_code LevelDBWrapper::read(IReadBatch &batch)
{
    if (state.load() != INITIALIZED)
    {
        throw std::runtime_error("Not initialized.");
    }

    leveldb::ReadOptions readOptions;

    std::vector<std::string> rawKeys(batch.getRawKeys());
    std::vector<leveldb::Slice> keySlices;
    keySlices.reserve(rawKeys.size());

    std::vector<std::string> values;
    std::error_code error;
    std::vector<bool> resultStates;

    for (const std::string &key : rawKeys)
    {
        std::string tmp_value;
        leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &tmp_value);
        if (!s.ok() && !s.IsNotFound())
        {
            return make_error_code(CryptoNote::error::DataBaseErrorCodes::INTERNAL_ERROR);
        }
        values.push_back(tmp_value);
        resultStates.push_back(s.ok());
    }

    values.reserve(rawKeys.size());

    batch.submitRawResult(values, resultStates);
    return std::error_code();
}

std::string LevelDBWrapper::getDataDir(const DataBaseConfig &config)
{
    return config.getDataDir() + '/' + DB_NAME;
}
