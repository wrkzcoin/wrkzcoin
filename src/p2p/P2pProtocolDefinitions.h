// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "P2pProtocolTypes.h"
#include "crypto/crypto.h"

#include <array>
#include <config/CryptoNoteConfig.h>

// new serialization
#include "serialization/CryptoNoteSerialization.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"

namespace CryptoNote
{
    const uint32_t NODE_CAPABILITY_FLAG_PRUNED = 0x1;

    /* A lite node is strictly weaker than a pruned one: it cannot serve any block
       below its lite height, not just old ones. It needs its own bit so peers can
       tell the two apart. See LITENODE.md. */
    const uint32_t NODE_CAPABILITY_FLAG_LITE = 0x2;

    struct network_config
    {
        void serialize(ISerializer &s) {KV_MEMBER(connections_count) KV_MEMBER(handshake_interval)
                                            KV_MEMBER(packet_max_size) KV_MEMBER(config_id)}

        uint32_t connections_count;

        uint32_t connection_timeout;

        uint32_t ping_connection_timeout;

        uint32_t handshake_interval;

        uint32_t packet_max_size;

        uint32_t config_id;

        uint32_t send_peerlist_sz;
    };

    struct basic_node_data
    {
        std::array<uint8_t, 16> network_id;

        uint8_t version;

        uint64_t local_time;

        uint32_t my_port;

        uint64_t peer_id;

        void serialize(ISerializer &s)
        {
            KV_MEMBER(network_id)
            if (s.type() == ISerializer::INPUT)
            {
                version = 0;
            }
            KV_MEMBER(version)
            KV_MEMBER(peer_id)
            KV_MEMBER(local_time)
            KV_MEMBER(my_port)
        }
    };

    struct CORE_SYNC_DATA
    {
        uint32_t current_height;

        Crypto::Hash top_id;

        uint32_t capability_flags;

        uint32_t pruned_node_height;

        /* Lowest height this node can serve a block for. Zero when it can serve
           every height. Zero-defaults on input, so peers running older builds
           that never send the field read as "serves everything", which is what
           they are. */
        uint32_t lite_start_height;

        void serialize(ISerializer &s)
        {
            if (s.type() == ISerializer::INPUT)
            {
                capability_flags = 0;
                pruned_node_height = 0;
                lite_start_height = 0;
            }

            KV_MEMBER(current_height)
            KV_MEMBER(top_id)
            KV_MEMBER(capability_flags)
            KV_MEMBER(pruned_node_height)
            KV_MEMBER(lite_start_height)
        }
    };

#define P2P_COMMANDS_POOL_BASE 1000

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct COMMAND_HANDSHAKE
    {
        enum
        {
            ID = P2P_COMMANDS_POOL_BASE + 1
        };

        struct request
        {
            basic_node_data node_data;

            CORE_SYNC_DATA payload_data;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(node_data)
                KV_MEMBER(payload_data)
            }
        };

        struct response
        {
            basic_node_data node_data;

            CORE_SYNC_DATA payload_data;

            std::list<PeerlistEntry> local_peerlist;

            std::list<PeerlistEntry6> local_peerlist6;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(node_data)
                KV_MEMBER(payload_data)
                serializeAsBinary(local_peerlist, "local_peerlist", s);
                serializeAsBinary(local_peerlist6, "local_peerlist6", s);
            }
        };
    };


    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct COMMAND_TIMED_SYNC
    {
        enum
        {
            ID = P2P_COMMANDS_POOL_BASE + 2
        };

        struct request
        {
            CORE_SYNC_DATA payload_data;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(payload_data)
            }
        };

        struct response
        {
            uint64_t local_time;

            CORE_SYNC_DATA payload_data;

            std::list<PeerlistEntry> local_peerlist;

            std::list<PeerlistEntry6> local_peerlist6;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(local_time)
                KV_MEMBER(payload_data)
                serializeAsBinary(local_peerlist, "local_peerlist", s);
                serializeAsBinary(local_peerlist6, "local_peerlist6", s);
            }
        };
    };

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    struct COMMAND_PING
    {
        /*
          Used to make "callback" connection, to be sure that opponent node
          have accessible connection point. Only other nodes can add peer to peerlist,
          and ONLY in case when peer has accepted connection and answered to ping.
        */
        enum
        {
            ID = P2P_COMMANDS_POOL_BASE + 3
        };

#define PING_OK_RESPONSE_STATUS_TEXT "OK"

        struct request
        {
            /*actually we don't need to send any real data*/
            void serialize(ISerializer &s) {}
        };

        struct response
        {
            std::string status;

            uint64_t peer_id;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(status)
                KV_MEMBER(peer_id)
            }
        };
    };
} // namespace CryptoNote
