// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <cryptonoteprotocol/ICryptoNoteProtocolQuery.h>
#include <vector>

namespace CryptoNote
{
    struct NOTIFY_NEW_BLOCK_request;
    struct NOTIFY_CHAINLOCK_VOTE_request;
    struct NOTIFY_INSTANTSEND_VOTE_request;

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct ICryptoNoteProtocol
    {
        virtual void relayBlock(NOTIFY_NEW_BLOCK_request &arg) = 0;

        virtual void relayTransactions(const std::vector<BinaryArray> &transactions) = 0;

        // Relay a ChainLock vote originating from this node's own MN signer.
        virtual void relayChainLockVote(NOTIFY_CHAINLOCK_VOTE_request &arg) = 0;

        // Relay an InstantSend vote originating from this node's own MN signer.
        virtual void relayInstantSendVote(NOTIFY_INSTANTSEND_VOTE_request &arg) = 0;
    };

    struct ICryptoNoteProtocolHandler : ICryptoNoteProtocol, public ICryptoNoteProtocolQuery
    {
        virtual ~ICryptoNoteProtocolHandler() {};
    };
} // namespace CryptoNote
