// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNote.h"
#include "P2pProtocolTypes.h"

#include <array>
#include <functional>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace CryptoNote
{
    struct CryptoNoteConnectionContext;

    struct IP2pEndpoint
    {
        virtual ~IP2pEndpoint() {};

        virtual void relay_notify_to_all(
            int command,
            const BinaryArray &data_buff,
            const std::array<uint8_t, 16> *excludeConnection) = 0;

        virtual bool invoke_notify_to_peer(
            int command,
            const BinaryArray &req_buff,
            const CryptoNote::CryptoNoteConnectionContext &context) = 0;

        virtual uint64_t get_connections_count() = 0;

        virtual void
            for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext &, uint64_t)> f) = 0;

        // can be called from external threads
        virtual void externalRelayNotifyToAll(
            int command,
            const BinaryArray &data_buff,
            const std::array<uint8_t, 16> *excludeConnection) = 0;

        virtual void externalRelayNotifyToList(
            int command,
            const BinaryArray &data_buff,
            const std::list<std::array<uint8_t, 16>> relayList) = 0;

        virtual bool ban_host(uint32_t ip, uint64_t seconds) = 0;

        virtual bool unban_host(uint32_t ip) = 0;

        /* A pure IPv6 peer has no uint32_t address - its m_remote_ip is 0 - so
           it is banned by its text address instead. */
        virtual bool ban_host6(const std::string &addr, uint64_t seconds) = 0;

        virtual std::vector<std::pair<uint32_t, uint64_t>> get_banned_hosts() = 0;

        /* Charge the peer behind a connection for misbehaving. Points add up
           per address and the address is banned once they reach the threshold;
           0 points means "drop only" and records nothing. */
        virtual void report_misbehaviour(
            const CryptoNoteConnectionContext &context,
            uint32_t points,
            const std::string &reason) = 0;
    };

    struct p2p_endpoint_stub : public IP2pEndpoint
    {
        ~p2p_endpoint_stub() {};

        virtual void relay_notify_to_all(
            int command,
            const BinaryArray &data_buff,
            const std::array<uint8_t, 16> *excludeConnection) override
        {
        }

        virtual bool invoke_notify_to_peer(
            int command,
            const BinaryArray &req_buff,
            const CryptoNote::CryptoNoteConnectionContext &context) override
        {
            return true;
        }

        virtual void
            for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext &, uint64_t)> f) override
        {
        }

        virtual uint64_t get_connections_count() override
        {
            return 0;
        }

        virtual void externalRelayNotifyToAll(
            int command,
            const BinaryArray &data_buff,
            const std::array<uint8_t, 16> *excludeConnection) override
        {
        }

        virtual void externalRelayNotifyToList(
            int command,
            const BinaryArray &data_buff,
            const std::list<std::array<uint8_t, 16>> relayList) override
        {
        }

        virtual bool ban_host(uint32_t ip, uint64_t seconds) override
        {
            return false;
        }

        virtual bool unban_host(uint32_t ip) override
        {
            return false;
        }

        virtual bool ban_host6(const std::string &addr, uint64_t seconds) override
        {
            return false;
        }

        virtual std::vector<std::pair<uint32_t, uint64_t>> get_banned_hosts() override
        {
            return {};
        }

        virtual void report_misbehaviour(
            const CryptoNoteConnectionContext &context,
            uint32_t points,
            const std::string &reason) override
        {
        }
    };
} // namespace CryptoNote
