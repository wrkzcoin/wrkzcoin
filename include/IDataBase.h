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

        /* Size of the ZSTD dictionary trained per SST file, in bytes. Zero, the
           default, disables dictionary compression entirely.

           Plain block compression only ever sees one block at a time - about
           fifty records at the default block size - so it relearns this
           database's repeated key framing in every block it writes. A trained
           dictionary is shared by the whole file, which is what this shape of
           data wants: hundreds of millions of small records that differ in a few
           high-entropy bytes and agree on everything else. Applies to newly
           written SST files, so an existing database needs a compaction that
           rewrites the bottommost level before it shows up. */
        uint64_t compressionDictBytes = 0;

        /* Uncompressed size of an SST data block. RocksDB's default is 4 KiB.

           Larger blocks give the compressor more context and shrink the index,
           at the cost of decompressing more bytes for a point lookup that misses
           the row and block caches. This workload is almost entirely point
           lookups, which is why the default is left where RocksDB put it. */
        uint64_t blockSize = 4 * 1024;

        /* ZSTD compression level for the bottommost level. Zero leaves RocksDB's
           own default, which is ZSTD's, which is 3.

           This is the one space knob that is free on the read path: ZSTD
           decompresses at much the same speed whatever level compressed it, so a
           harder setting costs compaction CPU and nothing else. Applied only to
           the bottommost level, which holds the great majority of the data and is
           rewritten rarely - the levels above churn constantly during a sync and
           are better left cheap. */
        int compressionLevel = 0;

        /* Share of readCacheSize given to the row cache, as a percentage. Zero
           keeps the built-in split of one eighth.

           The row cache holds finished key/value pairs, so a hit skips block
           decompression entirely - which is exactly the cost a larger blockSize
           adds. A node whose job is serving wallet reads may want more of the
           read buffer spent here. */
        uint64_t rowCachePercent = 0;

        /* Keep bloom filters on the bottommost level.

           Off by default, matching optimize_filters_for_hits: it saves the space
           those filters would take, on the reasoning that most lookups here are
           for keys that exist. That reasoning does not hold for spent key image
           checks, which ask about a key image precisely when it is expected to be
           absent - once per input of every transaction validated. Turning this on
           trades a few hundred megabytes for making those misses a filter probe
           rather than an index search and a block read. */
        bool bottommostFilters = false;
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

        /* rewriteBottommost forces the bottommost level to be rewritten. Needed
           after changing compression settings, which otherwise only apply to data
           written from then on. It rewrites the whole database, so it is slow and
           wants free space of about the database's size. */
        virtual std::pair<std::error_code, std::string> compactDetailed(bool rewriteBottommost) = 0;

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
