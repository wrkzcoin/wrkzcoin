// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IDataBase.h"
#include "rocksdb/db.h"

#include <atomic>
#include <logging/LoggerRef.h>
#include <memory>
#include <string>

namespace CryptoNote
{
    class RocksDBWrapper : public IDataBase
    {
      public:
        RocksDBWrapper(
            std::shared_ptr<Logging::ILogger> logger,
            const DataBaseConfig &config);

        virtual ~RocksDBWrapper();

        RocksDBWrapper(const RocksDBWrapper &) = delete;

        RocksDBWrapper(RocksDBWrapper &&) = delete;

        RocksDBWrapper &operator=(const RocksDBWrapper &) = delete;

        RocksDBWrapper &operator=(RocksDBWrapper &&) = delete;

        void init();

        void shutdown() override;

        void destroy(); // Be careful with this method!

        std::error_code write(IWriteBatch &batch) override;

        std::error_code write(IWriteBatch &batch, bool sync) override;

        std::error_code read(IReadBatch &batch) override;

        std::error_code readThreadSafe(IReadBatch &batch) override;

        std::error_code compact() override;

        std::pair<std::error_code, std::string> compactDetailed(bool rewriteBottommost) override;

        void recreate() override;

        std::error_code iterate(
            const std::string &keyPrefix,
            const std::function<bool(const std::string &key, const std::string &value)> &callback) override;

      private:
        rocksdb::Options getDBOptions(const DataBaseConfig &config);

        std::string getDataDir(const DataBaseConfig &config);

        enum State
        {
            NOT_INITIALIZED,
            INITIALIZED
        };

        Logging::LoggerRef logger;

        std::unique_ptr<rocksdb::DB> db;

        std::atomic<State> state;

        const DataBaseConfig m_config;
    };
} // namespace CryptoNote
