// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "daemon/LiteSnapshot.h"

#include "crypto/hash.h"

#include <cstring>
#include <stdexcept>
#include <zstd.h>

namespace CryptoNote
{
    namespace LiteSnapshot
    {
        namespace
        {
            const char MAGIC[MAGIC_SIZE] = {'W', 'R', 'K', 'Z', 'L', 'I', 'T', 'E'};

            /* Deliberately modest. The payload is hundreds of millions of small
               records that agree on almost everything, so most of the win is
               already taken by the first few levels; the rest costs export time
               a producer pays once and every importer waits for. */
            constexpr int ZSTD_LEVEL = 10;

            /* A frame that claims to be larger than this is refused rather than
               allocated. Without it a corrupt or hostile length field is an
               allocation the size of whatever it says. */
            constexpr size_t MAX_FRAME_PAYLOAD = 64 * 1024 * 1024;

            void putU32(std::string &out, const uint32_t value)
            {
                for (int i = 0; i < 4; i++)
                {
                    out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
                }
            }

            void putU64(std::string &out, const uint64_t value)
            {
                for (int i = 0; i < 8; i++)
                {
                    out.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
                }
            }

            uint32_t getU32(const char *data)
            {
                uint32_t value = 0;

                for (int i = 0; i < 4; i++)
                {
                    value |= static_cast<uint32_t>(static_cast<unsigned char>(data[i])) << (8 * i);
                }

                return value;
            }

            uint64_t getU64(const char *data)
            {
                uint64_t value = 0;

                for (int i = 0; i < 8; i++)
                {
                    value |= static_cast<uint64_t>(static_cast<unsigned char>(data[i])) << (8 * i);
                }

                return value;
            }

            void putVarint(std::string &out, uint64_t value)
            {
                while (value >= 0x80)
                {
                    out.push_back(static_cast<char>((value & 0x7f) | 0x80));
                    value >>= 7;
                }

                out.push_back(static_cast<char>(value));
            }

            /* Returns false rather than throwing on a truncated or overlong
               varint: this runs over bytes a stranger produced, and the caller
               turns it into one message about a corrupt file. */
            bool getVarint(const std::string &in, size_t &offset, uint64_t &value)
            {
                value = 0;

                int shift = 0;

                while (offset < in.size())
                {
                    const auto byte = static_cast<unsigned char>(in[offset++]);

                    if (shift > 63)
                    {
                        return false;
                    }

                    value |= static_cast<uint64_t>(byte & 0x7f) << shift;

                    if ((byte & 0x80) == 0)
                    {
                        return true;
                    }

                    shift += 7;
                }

                return false;
            }

            /* running = H(running || H(frame)). Chaining fixed size inputs keeps
               the digest independent of how the payload happens to be split into
               frames only if the split is itself deterministic - which it is,
               because frames close on a byte count of the record stream. */
            void chainDigest(Crypto::Hash &running, const std::string &frame)
            {
                const Crypto::Hash frameHash = Crypto::cn_fast_hash(frame.data(), frame.size());

                char combined[sizeof(Crypto::Hash) * 2];

                std::memcpy(combined, &running, sizeof(Crypto::Hash));
                std::memcpy(combined + sizeof(Crypto::Hash), &frameHash, sizeof(Crypto::Hash));

                running = Crypto::cn_fast_hash(combined, sizeof(combined));
            }

            std::string serializeHeader(const Header &header)
            {
                std::string out;
                out.reserve(HEADER_SIZE);

                out.append(MAGIC, MAGIC_SIZE);
                putU32(out, header.formatVersion);
                out.append(reinterpret_cast<const char *>(&header.genesisHash), sizeof(Crypto::Hash));
                putU32(out, header.liteHeight);
                putU64(out, header.blockInfoRecords);
                putU64(out, header.keyImageRecords);
                putU64(out, header.amountCountRecords);
                putU64(out, header.keyOutputRecords);
                putU64(out, header.transactionsCount);
                putU64(out, header.keyOutputAmountsCount);
                out.append(reinterpret_cast<const char *>(&header.payloadDigest), sizeof(Crypto::Hash));

                if (out.size() != HEADER_SIZE)
                {
                    throw std::runtime_error("Lite snapshot header layout does not match HEADER_SIZE");
                }

                return out;
            }
        } // namespace

        std::string defaultFileName(const uint32_t liteHeight)
        {
            /* No timestamp. Identical content must produce an identical name, or
               two copies of one snapshot cannot be told from two different ones.
               The extension is not .zst on purpose: this is a container that
               happens to hold zstd frames, and inviting anyone to run unzstd on
               it would only waste their time. */
            return "wrkz-lite-base-h" + std::to_string(liteHeight) + "-v" + std::to_string(FORMAT_VERSION)
                   + ".litesnap";
        }

        Writer::Writer(const std::string &path): m_path(path) {}

        Writer::~Writer()
        {
            /* An export that threw or was cancelled leaves a partial file, and a
               partial snapshot that looks like a whole one is exactly the thing
               a digest exists to catch - but only if someone checks. Remove it
               instead. */
            if (m_begun && !m_finished)
            {
                m_file.close();

                std::remove(m_path.c_str());
            }
        }

        void Writer::begin()
        {
            if (m_begun)
            {
                throw std::runtime_error("Lite snapshot writer already started");
            }

            m_file.open(m_path, std::ios::binary | std::ios::trunc);

            if (!m_file.is_open())
            {
                throw std::runtime_error("Could not open " + m_path + " for writing");
            }

            /* Reserved and written over by finish(). Neither the record counts
               nor the digest are knowable until the payload has been written,
               and the counts belong to the caller doing the walking. */
            const std::string reserved = serializeHeader(Header {});

            m_file.write(reserved.data(), static_cast<std::streamsize>(reserved.size()));

            if (!m_file)
            {
                throw std::runtime_error("Could not write the header of " + m_path);
            }

            m_bytesWritten = reserved.size();
            m_begun = true;

            m_frame.clear();
            m_frame.reserve(FRAME_PAYLOAD_TARGET + 64 * 1024);
        }

        void Writer::add(const std::string &key, const std::string &value)
        {
            if (!m_begun || m_finished)
            {
                throw std::runtime_error("Lite snapshot writer is not open");
            }

            /* The importer builds SST files straight from this stream, which is
               only valid if it really is sorted. Checking here turns a silent
               ingest failure much later into an error naming the export that
               produced it. */
            if (m_haveLastKey && !(m_lastKey < key))
            {
                throw std::runtime_error("Lite snapshot records are not in ascending key order");
            }

            m_lastKey = key;
            m_haveLastKey = true;

            putVarint(m_frame, key.size());
            m_frame.append(key);
            putVarint(m_frame, value.size());
            m_frame.append(value);

            if (m_frame.size() >= FRAME_PAYLOAD_TARGET)
            {
                flushFrame();
            }
        }

        void Writer::flushFrame()
        {
            if (m_frame.empty())
            {
                return;
            }

            chainDigest(m_digest, m_frame);

            const size_t bound = ZSTD_compressBound(m_frame.size());

            std::string compressed;
            compressed.resize(bound);

            const size_t written =
                ZSTD_compress(compressed.data(), bound, m_frame.data(), m_frame.size(), ZSTD_LEVEL);

            if (ZSTD_isError(written))
            {
                throw std::runtime_error(std::string("Failed to compress a lite snapshot frame: ")
                                         + ZSTD_getErrorName(written));
            }

            std::string lengths;
            putU32(lengths, static_cast<uint32_t>(m_frame.size()));
            putU32(lengths, static_cast<uint32_t>(written));

            m_file.write(lengths.data(), static_cast<std::streamsize>(lengths.size()));
            m_file.write(compressed.data(), static_cast<std::streamsize>(written));

            if (!m_file)
            {
                throw std::runtime_error("Failed writing to " + m_path + " - out of disk space?");
            }

            m_bytesWritten += lengths.size() + written;

            m_frame.clear();
        }

        Header Writer::finish(Header header)
        {
            if (!m_begun || m_finished)
            {
                throw std::runtime_error("Lite snapshot writer is not open");
            }

            flushFrame();

            m_header = header;
            m_header.formatVersion = FORMAT_VERSION;

            /* A zero raw length is the end of the payload. Without it a truncated
               file and a complete one look the same to a reader that has simply
               run out of frames. */
            std::string terminator;
            putU32(terminator, 0);
            putU32(terminator, 0);

            m_file.write(terminator.data(), static_cast<std::streamsize>(terminator.size()));

            m_header.payloadDigest = m_digest;

            const std::string serialized = serializeHeader(m_header);

            m_file.seekp(0, std::ios::beg);
            m_file.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
            m_file.flush();

            if (!m_file)
            {
                throw std::runtime_error("Failed finalising " + m_path);
            }

            m_file.close();
            m_finished = true;
            m_bytesWritten += terminator.size();

            return m_header;
        }

        Reader::Reader(const std::string &path): m_path(path) {}

        Header Reader::open()
        {
            m_file.open(m_path, std::ios::binary);

            if (!m_file.is_open())
            {
                throw std::runtime_error("Could not open " + m_path);
            }

            char raw[HEADER_SIZE];

            m_file.read(raw, HEADER_SIZE);

            if (m_file.gcount() != static_cast<std::streamsize>(HEADER_SIZE))
            {
                throw std::runtime_error(m_path + " is too short to be a lite node snapshot");
            }

            if (std::memcmp(raw, MAGIC, MAGIC_SIZE) != 0)
            {
                throw std::runtime_error(m_path + " is not a lite node snapshot");
            }

            m_header.formatVersion = getU32(raw + 8);

            if (m_header.formatVersion != FORMAT_VERSION)
            {
                throw std::runtime_error(
                    m_path + " is a version " + std::to_string(m_header.formatVersion)
                    + " lite node snapshot, and this build reads version " + std::to_string(FORMAT_VERSION));
            }

            std::memcpy(&m_header.genesisHash, raw + 12, sizeof(Crypto::Hash));
            m_header.liteHeight = getU32(raw + 44);
            m_header.blockInfoRecords = getU64(raw + 48);
            m_header.keyImageRecords = getU64(raw + 56);
            m_header.amountCountRecords = getU64(raw + 64);
            m_header.keyOutputRecords = getU64(raw + 72);
            m_header.transactionsCount = getU64(raw + 80);
            m_header.keyOutputAmountsCount = getU64(raw + 88);
            std::memcpy(&m_header.payloadDigest, raw + 96, sizeof(Crypto::Hash));

            m_opened = true;

            return m_header;
        }

        bool Reader::loadFrame()
        {
            char lengths[8];

            m_file.read(lengths, sizeof(lengths));

            if (m_file.gcount() != static_cast<std::streamsize>(sizeof(lengths)))
            {
                throw std::runtime_error(m_path + " ends in the middle of a frame - the file is truncated");
            }

            const uint32_t rawLength = getU32(lengths);
            const uint32_t compressedLength = getU32(lengths + 4);

            if (rawLength == 0)
            {
                m_exhausted = true;

                return false;
            }

            if (rawLength > MAX_FRAME_PAYLOAD || compressedLength > MAX_FRAME_PAYLOAD)
            {
                throw std::runtime_error(m_path + " declares an implausible frame size - the file is corrupt");
            }

            std::string compressed;
            compressed.resize(compressedLength);

            m_file.read(compressed.data(), compressedLength);

            if (m_file.gcount() != static_cast<std::streamsize>(compressedLength))
            {
                throw std::runtime_error(m_path + " ends in the middle of a frame - the file is truncated");
            }

            m_frame.resize(rawLength);

            const size_t decompressed =
                ZSTD_decompress(m_frame.data(), rawLength, compressed.data(), compressedLength);

            if (ZSTD_isError(decompressed) || decompressed != rawLength)
            {
                throw std::runtime_error(m_path + " holds a frame that does not decompress - the file is corrupt");
            }

            chainDigest(m_digest, m_frame);

            m_frameOffset = 0;

            return true;
        }

        bool Reader::next(std::string &key, std::string &value)
        {
            if (!m_opened)
            {
                throw std::runtime_error("Lite snapshot reader is not open");
            }

            while (true)
            {
                if (m_exhausted)
                {
                    return false;
                }

                if (m_frameOffset >= m_frame.size())
                {
                    if (!loadFrame())
                    {
                        return false;
                    }

                    continue;
                }

                uint64_t keyLength = 0;
                uint64_t valueLength = 0;

                if (!getVarint(m_frame, m_frameOffset, keyLength)
                    || m_frameOffset + keyLength > m_frame.size())
                {
                    throw std::runtime_error(m_path + " holds a malformed record - the file is corrupt");
                }

                key.assign(m_frame, m_frameOffset, keyLength);
                m_frameOffset += keyLength;

                if (!getVarint(m_frame, m_frameOffset, valueLength)
                    || m_frameOffset + valueLength > m_frame.size())
                {
                    throw std::runtime_error(m_path + " holds a malformed record - the file is corrupt");
                }

                value.assign(m_frame, m_frameOffset, valueLength);
                m_frameOffset += valueLength;

                m_recordsRead++;

                return true;
            }
        }
    } // namespace LiteSnapshot
} // namespace CryptoNote
