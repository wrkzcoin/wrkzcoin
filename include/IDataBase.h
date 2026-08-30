// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IReadBatch.h"
#include "IWriteBatch.h"

#include <cstdint>
#include <functional>
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

        /* Durable write - the batch is fsynced to disk before returning. */
        virtual std::error_code write(IWriteBatch &batch) = 0;

        /* Write with explicit durability. With sync == false the batch is still
         * written atomically and still goes to the write-ahead log, but the log
         * is left in the operating system's page cache rather than being
         * fsynced. A crash of this process loses nothing; a machine crash or
         * power loss may lose whole trailing batches, never a partial one.
         * Used to avoid an fsync per block while catching up on the chain. */
        virtual std::error_code write(IWriteBatch &batch, bool sync) = 0;

        virtual std::error_code read(IReadBatch &batch) = 0;

        virtual std::error_code readThreadSafe(IReadBatch &batch) = 0;

        virtual std::error_code compact() = 0;

        virtual std::pair<std::error_code, std::string> compactDetailed() = 0;

        virtual void recreate() = 0;

        /* Walks every key that begins with keyPrefix, in key order, calling the
           callback with the raw key and value. Return false from the callback to
           stop early.

           Reads elsewhere in this interface take an explicit list of keys, which
           works because almost everything in the database is reachable from a
           counter. Spent key images are not: they are hashes with no counter, and
           the block index that used to enumerate them is one of the things a lite
           node does not store. Exporting them needs a scan.

           The walk sees a consistent point-in-time view: writes made after it
           starts are not visible to it, so a scan running beside block
           processing cannot observe a half-applied block. */
        virtual std::error_code iterate(
            const std::string &keyPrefix,
            const std::function<bool(const std::string &key, const std::string &value)> &callback) = 0;
    };
} // namespace CryptoNote
