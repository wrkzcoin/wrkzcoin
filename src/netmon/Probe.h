// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "NodeStore.h"

#include <CryptoTypes.h>
#include <array>
#include <cstdint>
#include <string>
#include <system/IpAddress.h>
#include <vector>

namespace System
{
    class Dispatcher;
}

namespace NetMon
{
    /* One address learned from a peer's local_peerlist. Flattened out of
       PeerlistEntry / PeerlistEntry6 so the crawler never has to care which
       family it came from. */
    struct HarvestedPeer
    {
        std::string address;

        uint16_t port = 0;
    };

    struct ProbeResult
    {
        std::string address;

        uint16_t port = 0;

        Reach reach = Reach::Unknown;

        uint64_t rttMs = 0;

        /* Everything the handshake response claimed. Only meaningful when
           reach == Open. */
        NodeRecord reported;

        std::vector<HarvestedPeer> harvested;

        /* Set on any non-Open outcome; goes to the log at debug level. */
        std::string error;
    };

    /* Identity this crawler presents. Fixed for the process lifetime.

       The three values here are the whole of the crawler's etiquette contract
       with the network, and each one is load-bearing:

       - myPort is always 0. A peer that receives a handshake with my_port == 0
         skips its back ping and does not add the sender to its white list, so
         the crawler is never gossiped around as a peer. This mirrors what the
         daemon's own --hide-my-port does.

       - claimedHeight is always 0. The receiving node feeds a handshake's
         current_height into its observed-height maximum, so an inflated value
         poisons network_height in /info on every node it touches until that
         node restarts. Zero is stronger than merely honest: the daemon's lite
         node depth check is guarded on current_height > 0, and that check can
         call exit(1) after only four samples - so a crawler claiming any
         height at all could shut down a booting lite node.

       - topId is the genesis hash, which every node on the network has. That
         makes the receiver's hasBlock() succeed and take the "nothing to do
         here" branch, instead of deciding it might need to sync from us and
         opening a chain request the crawler has already hung up on. */
    struct CrawlerIdentity
    {
        uint64_t peerId = 0;

        uint8_t p2pVersion = 0;

        Crypto::Hash genesisHash {};

        std::array<uint8_t, 16> networkId {};
    };

    /* Builds the identity from the compiled-in network constants. Returns
       false, with a reason, if the genesis hash cannot be recovered - which
       would mean CryptoNoteCheckpoints.h has no height-0 entry. */
    bool buildCrawlerIdentity(CrawlerIdentity &identity, std::string &error);

    /* Connect, send one COMMAND_HANDSHAKE, read the response, hang up.
       Never throws: every failure comes back as a non-Open reach with error
       set. Must be called from inside a context on the given dispatcher. */
    ProbeResult probePeer(
        System::Dispatcher &dispatcher,
        const System::IpAddress &address,
        uint16_t port,
        uint32_t timeoutMs,
        const CrawlerIdentity &identity);

    /* Asks a peer's RPC port for /info and returns the "version" field, or an
       empty string when the port is closed, the body is not what we expect, or
       the peer simply is not a daemon. Never throws. */
    std::string probeSoftwareVersion(const std::string &address, uint16_t rpcPort, uint32_t timeoutMs);
} // namespace NetMon
