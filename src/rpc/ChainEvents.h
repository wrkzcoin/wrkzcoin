// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <crypto/hash.h>
#include <cryptonotecore/BlockchainMessages.h>
#include <cryptonotecore/ICore.h>
#include <cstdint>
#include <logging/LoggerRef.h>
#include <string>
#include <utility>
#include <vector>

/* What happened to the chain and the pool, as the topics and JSON bodies the
   daemon publishes them under. The ZMQ socket (--zmq-pub) and the WebSocket
   stream (GET /ws, --enable-websocket) both send exactly these, so a body is
   byte for byte the same whichever way a subscriber receives it. */
namespace ChainEvents
{
    /* topic, JSON body */
    using Message = std::pair<std::string, std::string>;

    /* Every topic describe() can produce, in the order a WebSocket hello lists
       them. */
    const std::vector<std::string> &topics();

    /* The messages one BlockchainMessage is published as:

       NewBlock            hashblock     {"height":N,"hash":"..."}
                           chain_main    {"height":N,"hash":"...","transaction_hashes":[...]}
                                         coinbase first; left out below a lite
                                         node's lite height, where there is no
                                         stored body to list them from
       NewAlternativeBlock hashblock_alt {"height":N,"hash":"..."}
       ChainSwitch         chainswitch   {"common_root_height":R,"hashes":[...]}
       AddTransaction      txpool_add    {"hashes":[...]}
       DeleteTransaction   txpool_del    {"hashes":[...],"reason":"InBlock|Outdated|NotActual"}

       height is a block index. liteHeight is 0 for a full node. */
    std::vector<Message> describe(
        const CryptoNote::BlockchainMessage &message,
        const CryptoNote::ICore &core,
        const uint32_t liteHeight,
        Logging::LoggerRef &logger);

    /* The body of a WebSocket stream's first message: the tip, and the topics
       this subscription carries. */
    std::string hello(const uint32_t topIndex, const Crypto::Hash &topHash, const std::vector<std::string> &topics);

    std::string hashToString(const Crypto::Hash &hash);

    std::string hashesToJsonArray(const std::vector<Crypto::Hash> &hashes);
} // namespace ChainEvents
