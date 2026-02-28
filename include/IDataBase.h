// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IReadBatch.h"
#include "IWriteBatch.h"

#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

namespace CryptoNote
{
    struct DataBaseConfig
    {
        DataBaseConfig(
            const std::string dataDirectory,
            const uint64_t backgroundThreads,
            const uint64_t openFiles,
            const uint64_t writeBufferMB,
            const uint64_t readCacheMB,
            const bool enableDbCompression) :
            dataDir(dataDirectory),
            backgroundThreadsCount(backgroundThreads),
            maxOpenFiles(openFiles),
            writeBufferSize(writeBufferMB * 1024 * 1024),
            readCacheSize(readCacheMB * 1024 * 1024),
            compressionEnabled(enableDbCompression)
        {
        }

        std::string dataDir;

        uint64_t backgroundThreadsCount;

        uint64_t maxOpenFiles;

        uint64_t writeBufferSize;

        uint64_t readCacheSize;

        bool compressionEnabled;
    };

    class IDataBase
    {
      public:
        virtual ~IDataBase() {}

        virtual void init() = 0;

        virtual void shutdown() = 0;

        virtual void destroy() = 0;

        virtual std::error_code write(IWriteBatch &batch) = 0;

        virtual std::error_code read(IReadBatch &batch) = 0;

        virtual std::error_code readThreadSafe(IReadBatch &batch) = 0;

        virtual std::error_code compact() = 0;

        virtual std::pair<std::error_code, std::string> compactDetailed() = 0;

        virtual void recreate() = 0;
    };
} // namespace CryptoNote
