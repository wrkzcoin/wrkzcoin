// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "common/StringTools.h"
#include "crypto/hash.h"
#include "p2p/PendingLiteBlock.h"

#include <boost/uuid/uuid.hpp>
#include <chrono>
#include <deque>
#include <optional>
#include <ostream>
#include <unordered_set>

namespace CryptoNote
{
    struct CryptoNoteConnectionContext
    {
        uint8_t version;
        boost::uuids::uuid m_connection_id;
        uint32_t m_remote_ip = 0;
        uint32_t m_remote_port = 0;
        std::string m_remote_ipv6; // non-empty for pure IPv6 connections (m_remote_ip == 0)
        bool m_is_income = false;

        // Returns the display address string regardless of IP version.
        std::string remoteAddressStr() const
        {
            return m_remote_ipv6.empty() ? Common::ipAddressToString(m_remote_ip) : m_remote_ipv6;
        }
        time_t m_started = 0;

        enum state
        {
            state_befor_handshake = 0, // default state
            state_synchronizing,
            state_idle,
            state_normal,
            state_sync_required,
            state_pool_sync_required,
            state_shutdown
        };

        state m_state = state_befor_handshake;
        std::optional<PendingLiteBlock> m_pending_lite_block;
        std::deque<Crypto::Hash> m_needed_objects;
        std::unordered_set<Crypto::Hash> m_requested_objects;
        uint32_t m_remote_blockchain_height = 0;
        uint32_t m_last_response_height = 0;
        bool m_remote_is_pruned_node = false;
        uint32_t m_remote_pruned_node_height = 0;

        uint32_t m_sync_batch_size = 100;
        uint64_t m_sync_blocks_received = 0;
        uint64_t m_sync_bytes_received = 0;
        uint64_t m_sync_avg_block_bytes = 0;
        uint32_t m_sync_failures = 0;
        uint32_t m_sync_orphan_retries = 0;
        uint64_t m_last_sync_progress_ts = 0;
        std::chrono::steady_clock::time_point m_sync_chunk_start_time {};
        float m_sync_blocks_per_second = 0.0f;

        /* True while we have asked the peer for the next batch of blocks before
           finishing with the batch it already sent us, so that the transfer
           overlaps validation instead of following it. */
        bool m_pipelined_objects_outstanding = false;

        /* Set when we abandon a batch we had already re-requested - the peer's
           reply is still on its way and is no longer wanted, but it is not
           misbehaviour and must not cost the peer its connection. */
        bool m_discard_next_objects_response = false;
    };

    inline std::string get_protocol_state_string(CryptoNoteConnectionContext::state s)
    {
        switch (s)
        {
            case CryptoNoteConnectionContext::state_befor_handshake:
                return "state_befor_handshake";
            case CryptoNoteConnectionContext::state_synchronizing:
                return "state_synchronizing";
            case CryptoNoteConnectionContext::state_idle:
                return "state_idle";
            case CryptoNoteConnectionContext::state_normal:
                return "state_normal";
            case CryptoNoteConnectionContext::state_sync_required:
                return "state_sync_required";
            case CryptoNoteConnectionContext::state_pool_sync_required:
                return "state_pool_sync_required";
            case CryptoNoteConnectionContext::state_shutdown:
                return "state_shutdown";
            default:
                return "unknown";
        }
    }

} // namespace CryptoNote

namespace std
{
    inline std::ostream &operator<<(std::ostream &s, const CryptoNote::CryptoNoteConnectionContext &context)
    {
        return s << "[" << context.remoteAddressStr() << ":" << context.m_remote_port
                 << (context.m_is_income ? " INC" : " OUT") << "] ";
    }
} // namespace std
