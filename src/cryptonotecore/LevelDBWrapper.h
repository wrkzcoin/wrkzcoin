// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IDataBase.h"
#include "leveldb/db.h"

#include <atomic>
#include <logging/LoggerRef.h>
#include <memory>
#include <string>

namespace CryptoNote
{
    class LevelDBWrapper : public IDataBase
    {
      public:
        LevelDBWrapper(std::shared_ptr<Logging::ILogger> logger);

        virtual ~LevelDBWrapper();

        LevelDBWrapper(const LevelDBWrapper &) = delete;

        LevelDBWrapper(LevelDBWrapper &&) = delete;

        LevelDBWrapper &operator=(const LevelDBWrapper &) = delete;

        LevelDBWrapper &operator=(LevelDBWrapper &&) = delete;

        void init(const DataBaseConfig &config) override;

        void shutdown() override;

        void destroy(const DataBaseConfig &config) override; // Be careful with this method!

        std::error_code write(IWriteBatch &batch) override;

        std::error_code read(IReadBatch &batch) override;

        std::error_code readThreadSafe(IReadBatch &batch) override;

      private:
        std::error_code write(IWriteBatch &batch, bool sync);

        std::string getDataDir(const DataBaseConfig &config);

        enum State
        {
            NOT_INITIALIZED,
            INITIALIZED
        };

        Logging::LoggerRef logger;

        std::unique_ptr<leveldb::DB> db;

        std::atomic<State> state;
    };
} // namespace CryptoNote
