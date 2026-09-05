// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace CryptoNote
{
    /* The on-disk format for a lite node base snapshot: the index-only region
       [0, H) of a lite database, packed so it can be moved between machines
       instead of rebuilt from the chain.

       See LITESNAPSHOT.md for the design. Two properties drive every decision
       here:

       Determinism. Two nodes at different tips exporting at the same H must
       produce byte-identical files, because a digest nobody can reproduce is a
       digest nobody can check. So this format is defined here in explicit
       little-endian bytes rather than reusing the KV binary serializer: that
       serializer is free to change with the database scheme, and a published
       snapshot must outlive such a change or the pinned digests all die with
       it.

       Sorted output. Records are written in RocksDB key order, so the importer
       can build SST files directly and ingest them rather than paying the write
       path's amplification. Writer::add enforces the ordering rather than
       trusting the caller. */
    namespace LiteSnapshot
    {
        /* Bumped only for an incompatible layout change. A reader refuses a
           version it does not know rather than guessing. */
        constexpr uint32_t FORMAT_VERSION = 1;

        constexpr size_t MAGIC_SIZE = 8;

        constexpr size_t HEADER_SIZE = 128;

        /* Records accumulate here before being compressed as one frame. Big
           enough that zstd has real context to work with, small enough that a
           reader never has to hold much. */
        constexpr size_t FRAME_PAYLOAD_TARGET = 4 * 1024 * 1024;

        struct Header
        {
            uint32_t formatVersion = FORMAT_VERSION;

            /* Identifies the chain. A snapshot for one network must not be
               importable into another, and the genesis hash is the cheapest
               thing that says which is which. */
            Crypto::Hash genesisHash {};

            /* H. The snapshot describes the chain as it stood with a top block
               of H - 1, whatever height the exporting node was actually at. */
            uint32_t liteHeight = 0;

            uint64_t blockInfoRecords = 0;

            uint64_t keyImageRecords = 0;

            uint64_t amountCountRecords = 0;

            uint64_t keyOutputRecords = 0;

            /* Chain-wide totals as of H - 1, so the importer can restore the
               counters a resumed sync depends on without deriving them from
               tables a lite node does not carry. Both are cross-checked against
               what the payload actually contains. */
            uint64_t transactionsCount = 0;

            uint64_t keyOutputAmountsCount = 0;

            /* Chained hash over the uncompressed payload. Zero in the file until
               the export finishes, then written back over. */
            Crypto::Hash payloadDigest {};

            uint64_t totalRecords() const
            {
                return blockInfoRecords + keyImageRecords + amountCountRecords + keyOutputRecords;
            }
        };

        /* Streaming writer. Records must arrive in ascending key order.

           The digest is a chain rather than one hash over 21 GB, so neither side
           has to hold the payload or depend on a streaming hash primitive:
           running = H(running || H(frame)). Frame boundaries are a function of
           the record stream alone, so the chain is as deterministic as the
           payload it covers. */
        class Writer
        {
          public:
            explicit Writer(const std::string &path);

            ~Writer();

            Writer(const Writer &) = delete;

            Writer &operator=(const Writer &) = delete;

            /* Reserves the header, which finish() writes over once the counts
               and the digest are known. Throws on failure to open. */
            void begin();

            void add(const std::string &key, const std::string &value);

            /* Flushes the last frame, stamps the payload digest into the header
               it is given and writes that over the reserved bytes. Returns the
               header as written. */
            Header finish(Header header);

            uint64_t bytesWritten() const
            {
                return m_bytesWritten;
            }

          private:
            void flushFrame();

            std::string m_path;

            std::ofstream m_file;

            Header m_header;

            std::string m_frame;

            std::string m_lastKey;

            bool m_haveLastKey = false;

            Crypto::Hash m_digest {};

            uint64_t m_bytesWritten = 0;

            bool m_begun = false;

            bool m_finished = false;
        };

        /* Streaming reader. Verifies the magic and format version on open, and
           the digest chain as it goes; next() returns false at the end of the
           payload, at which point digestMatches() is meaningful. */
        class Reader
        {
          public:
            explicit Reader(const std::string &path);

            Reader(const Reader &) = delete;

            Reader &operator=(const Reader &) = delete;

            /* Throws std::runtime_error with a message meant to be shown to an
               operator if the file is not a snapshot this build understands. */
            Header open();

            bool next(std::string &key, std::string &value);

            const Crypto::Hash &computedDigest() const
            {
                return m_digest;
            }

            uint64_t recordsRead() const
            {
                return m_recordsRead;
            }

          private:
            bool loadFrame();

            std::string m_path;

            std::ifstream m_file;

            Header m_header;

            std::string m_frame;

            size_t m_frameOffset = 0;

            Crypto::Hash m_digest {};

            uint64_t m_recordsRead = 0;

            bool m_opened = false;

            bool m_exhausted = false;
        };

        /* The conventional name for a snapshot at this height. No timestamp:
           identical content has to produce an identical name, or two copies of
           one snapshot cannot be told apart from two different ones. */
        std::string defaultFileName(uint32_t liteHeight);
    } // namespace LiteSnapshot
} // namespace CryptoNote
