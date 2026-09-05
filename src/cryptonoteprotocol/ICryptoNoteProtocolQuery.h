// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstddef>
#include <cstdint>

namespace CryptoNote
{
    class ICryptoNoteProtocolObserver;

    class ICryptoNoteProtocolQuery
    {
      public:
        virtual bool addObserver(ICryptoNoteProtocolObserver *observer) = 0;

        virtual bool removeObserver(ICryptoNoteProtocolObserver *observer) = 0;

        virtual uint32_t getObservedHeight() const = 0;

        virtual uint32_t getBlockchainHeight() const = 0;

        virtual size_t getPeerCount() const = 0;

        virtual bool isSynchronized() const = 0;

        virtual bool isPrunedNode() const = 0;

        virtual uint32_t getPrunedNodeDepth() const = 0;

        /* 0 for a full node; otherwise the height this node stores full blocks
           from. See LITENODE.md. */
        virtual uint32_t getLiteNodeHeight() const = 0;

        virtual bool isPruneCapabilityActive() const = 0;

        virtual uint32_t getSyncActivePeers() const = 0;

        virtual uint32_t getSyncAvgBatchSize() const = 0;

        virtual uint32_t getSyncDemotedPeers() const = 0;
    };

} // namespace CryptoNote
