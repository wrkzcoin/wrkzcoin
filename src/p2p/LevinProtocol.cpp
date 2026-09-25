// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "LevinProtocol.h"

#include "P2pProtocolDefinitions.h"
#include "cryptonoteprotocol/CryptoNoteProtocolDefinitions.h"

#include <string>
#include <system/TcpConnection.h>

using namespace CryptoNote;

namespace
{
    const uint64_t LEVIN_SIGNATURE = 0x0101010101012101LL; // Bender's nightmare
    const uint32_t LEVIN_PACKET_REQUEST = 0x00000001;

    const uint32_t LEVIN_PACKET_RESPONSE = 0x00000002;

    const uint32_t LEVIN_DEFAULT_MAX_PACKET_SIZE = 100000000; // 100MB by default
    const uint32_t LEVIN_PROTOCOL_VER_1 = 1;

#pragma pack(push)
#pragma pack(1)
    struct bucket_head2
    {
        uint64_t m_signature;
        uint64_t m_cb;
        bool m_have_to_return_data;
        uint32_t m_command;
        int32_t m_return_code;
        uint32_t m_flags;
        uint32_t m_protocol_version;
    };
#pragma pack(pop)

    /* The body is allocated in full from the header's length before a byte of
       it is read, so one limit for every command let a peer make us reserve
       100 MB for a ping. Commands whose honest bodies are always small get a
       cap of their own, each several times the largest honest body: a 250 +
       250 entry peer list for handshake and timed sync, 10,000 block ids for
       chain traffic, a whole block for block announcements. The ones that
       really are large - a pool's worth of transactions, a batch of blocks -
       and any command not listed here (other branches add their own) keep the
       general limit. */
    uint64_t maxPayloadSize(const uint32_t command)
    {
        constexpr uint64_t KiB = 1024;
        constexpr uint64_t MiB = 1024 * KiB;

        switch (command)
        {
            case COMMAND_HANDSHAKE::ID:
            case COMMAND_TIMED_SYNC::ID:
                return 256 * KiB;
            case COMMAND_PING::ID:
                return 16 * KiB;
            case NOTIFY_REQUEST_CHAIN::ID:
            case NOTIFY_RESPONSE_CHAIN_ENTRY::ID:
            case NOTIFY_MISSING_TXS::ID:
                return 1 * MiB;
            case NOTIFY_REQUEST_GET_OBJECTS::ID:
                return 2 * MiB;
            case NOTIFY_REQUEST_TX_POOL::ID:
                return 4 * MiB;
            case NOTIFY_NEW_BLOCK::ID:
            case NOTIFY_NEW_LITE_BLOCK::ID:
                return 16 * MiB;
            default:
                return LEVIN_DEFAULT_MAX_PACKET_SIZE;
        }
    }

} // namespace

bool LevinProtocol::Command::needReply() const
{
    return !(isNotify || isResponse);
}

LevinProtocol::LevinProtocol(System::TcpConnection &connection): m_conn(connection) {}

void LevinProtocol::sendMessage(uint32_t command, const BinaryArray &out, bool needResponse)
{
    bucket_head2 head = {0};
    head.m_signature = LEVIN_SIGNATURE;
    head.m_cb = out.size();
    head.m_have_to_return_data = needResponse;
    head.m_command = command;
    head.m_protocol_version = LEVIN_PROTOCOL_VER_1;
    head.m_flags = LEVIN_PACKET_REQUEST;

    // write header and body in one operation
    BinaryArray writeBuffer;
    writeBuffer.reserve(sizeof(head) + out.size());

    Common::VectorOutputStream stream(writeBuffer);
    stream.writeSome(&head, sizeof(head));
    stream.writeSome(out.data(), out.size());

    writeStrict(writeBuffer.data(), writeBuffer.size());
}

bool LevinProtocol::readCommand(Command &cmd)
{
    bucket_head2 head = {0};

    if (!readStrict(reinterpret_cast<uint8_t *>(&head), sizeof(head)))
    {
        return false;
    }

    if (head.m_signature != LEVIN_SIGNATURE)
    {
        throw std::runtime_error("Levin signature mismatch");
    }

    if (head.m_cb > maxPayloadSize(head.m_command))
    {
        throw std::runtime_error(
            "Levin packet size is too big for command " + std::to_string(head.m_command) + ": "
            + std::to_string(head.m_cb) + " bytes");
    }

    BinaryArray buf;

    if (head.m_cb != 0)
    {
        buf.resize(head.m_cb);
        if (!readStrict(&buf[0], head.m_cb))
        {
            return false;
        }
    }

    cmd.command = head.m_command;
    cmd.buf = std::move(buf);
    cmd.isNotify = !head.m_have_to_return_data;
    cmd.isResponse = (head.m_flags & LEVIN_PACKET_RESPONSE) == LEVIN_PACKET_RESPONSE;

    return true;
}

void LevinProtocol::sendReply(uint32_t command, const BinaryArray &out, int32_t returnCode)
{
    bucket_head2 head = {0};
    head.m_signature = LEVIN_SIGNATURE;
    head.m_cb = out.size();
    head.m_have_to_return_data = false;
    head.m_command = command;
    head.m_protocol_version = LEVIN_PROTOCOL_VER_1;
    head.m_flags = LEVIN_PACKET_RESPONSE;
    head.m_return_code = returnCode;

    BinaryArray writeBuffer;
    writeBuffer.reserve(sizeof(head) + out.size());

    Common::VectorOutputStream stream(writeBuffer);
    stream.writeSome(&head, sizeof(head));
    stream.writeSome(out.data(), out.size());

    writeStrict(writeBuffer.data(), writeBuffer.size());
}

void LevinProtocol::writeStrict(const uint8_t *ptr, size_t size)
{
    size_t offset = 0;
    while (offset < size)
    {
        offset += m_conn.write(ptr + offset, size - offset);
    }
}

bool LevinProtocol::readStrict(uint8_t *ptr, size_t size)
{
    size_t offset = 0;
    while (offset < size)
    {
        size_t read = m_conn.read(ptr + offset, size - offset);
        if (read == 0)
        {
            return false;
        }

        offset += read;
    }

    return true;
}
