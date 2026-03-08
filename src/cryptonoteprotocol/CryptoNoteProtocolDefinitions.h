// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cryptonotecore/Core.h>
#include "cryptonotecore/CryptoNoteBasic.h"

#include <list>

// ISerializer-based serialization
#include "serialization/CryptoNoteSerialization.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"

namespace CryptoNote
{
#define BC_COMMANDS_POOL_BASE 2000

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/

    // just to keep backward compatibility with BlockCompleteEntry serialization
    struct RawBlockLegacy
    {
        std::vector<uint8_t> blockTemplate;
        std::vector<std::vector<uint8_t>> transactions;

        RawBlockLegacy() {};

        RawBlockLegacy(
            const std::vector<uint8_t> blockTemplate_,
            const std::vector<std::vector<uint8_t>> transactions_):
            blockTemplate(blockTemplate_),
            transactions(transactions_)
        {
        }

        RawBlockLegacy(
            const std::vector<uint8_t> &rawBlob,
            const BlockTemplate blockTmp,
            const std::shared_ptr<CryptoNote::Core> core)
        {
            blockTemplate = rawBlob;

            if (!blockTmp.transactionHashes.empty())
            {
                transactions.reserve(blockTmp.transactionHashes.size());

                std::vector<Crypto::Hash> ignore;

                core->getTransactions(blockTmp.transactionHashes, transactions, ignore);
            }
        }
    };

    struct NOTIFY_NEW_BLOCK_request
    {
        RawBlockLegacy block;
        uint32_t current_blockchain_height;
        uint32_t hop;
    };

    struct NOTIFY_NEW_BLOCK
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 1;
        typedef NOTIFY_NEW_BLOCK_request request;
    };

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct NOTIFY_NEW_TRANSACTIONS_request
    {
        std::vector<BinaryArray> txs;
    };

    struct NOTIFY_NEW_TRANSACTIONS
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 2;
        typedef NOTIFY_NEW_TRANSACTIONS_request request;
    };

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct NOTIFY_REQUEST_GET_OBJECTS_request
    {
        std::vector<Crypto::Hash> txs;

        std::vector<Crypto::Hash> blocks;

        void serialize(ISerializer &s)
        {
            serializeAsBinary(txs, "txs", s);
            serializeAsBinary(blocks, "blocks", s);
        }
    };

    struct NOTIFY_REQUEST_GET_OBJECTS
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 3;
        typedef NOTIFY_REQUEST_GET_OBJECTS_request request;
    };

    struct NOTIFY_RESPONSE_GET_OBJECTS_request
    {
        std::vector<std::string> txs;
        std::vector<RawBlockLegacy> blocks;
        std::vector<Crypto::Hash> missed_ids;
        uint32_t current_blockchain_height;
    };

    struct NOTIFY_RESPONSE_GET_OBJECTS
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 4;
        typedef NOTIFY_RESPONSE_GET_OBJECTS_request request;
    };

    struct NOTIFY_REQUEST_CHAIN
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 6;

        struct request
        {
            std::vector<Crypto::Hash>
                block_ids; /*IDs of the first 10 blocks are sequential, next goes with pow(2,n) offset, like 2, 4, 8,
                              16, 32, 64 and so on, and the last one is always genesis block */

            void serialize(ISerializer &s)
            {
                serializeAsBinary(block_ids, "block_ids", s);
            }
        };
    };

    struct NOTIFY_RESPONSE_CHAIN_ENTRY_request
    {
        uint32_t start_height;

        uint32_t total_height;

        std::vector<Crypto::Hash> m_block_ids;

        void serialize(ISerializer &s)
        {
            KV_MEMBER(start_height)
            KV_MEMBER(total_height)
            serializeAsBinary(m_block_ids, "m_block_ids", s);
        }
    };

    struct NOTIFY_RESPONSE_CHAIN_ENTRY
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 7;
        typedef NOTIFY_RESPONSE_CHAIN_ENTRY_request request;
    };

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct NOTIFY_REQUEST_TX_POOL_request
    {
        std::vector<Crypto::Hash> txs;

        void serialize(ISerializer &s)
        {
            serializeAsBinary(txs, "txs", s);
        }
    };

    struct NOTIFY_REQUEST_TX_POOL
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 8;
        typedef NOTIFY_REQUEST_TX_POOL_request request;
    };

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    struct NOTIFY_NEW_LITE_BLOCK_request
    {
        BinaryArray blockTemplate;
        uint32_t current_blockchain_height;
        uint32_t hop;
    };

    struct NOTIFY_NEW_LITE_BLOCK
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 9;
        typedef NOTIFY_NEW_LITE_BLOCK_request request;
    };

    struct NOTIFY_MISSING_TXS_request
    {
        Crypto::Hash blockHash;
        uint32_t current_blockchain_height;
        std::vector<Crypto::Hash> missing_txs;
    };

    struct NOTIFY_MISSING_TXS
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 10;
        typedef NOTIFY_MISSING_TXS_request request;
    };

    // -------------------------------------------------------------------------
    // ChainLock messages (IDs 2011-2012)
    // -------------------------------------------------------------------------

    // A single masternode's vote to ChainLock a block at a given height.
    struct NOTIFY_CHAINLOCK_VOTE_request
    {
        uint32_t height;
        Crypto::Hash blockHash;
        Crypto::Hash masternodeId;
        Crypto::PublicKey signingKey;
        Crypto::Signature signature;

        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
            s.binary(&blockHash, sizeof(blockHash), "block_hash");
            s.binary(&masternodeId, sizeof(masternodeId), "mn_id");
            s.binary(&signingKey, sizeof(signingKey), "signing_key");
            s.binary(&signature, sizeof(signature), "sig");
        }
    };

    struct NOTIFY_CHAINLOCK_VOTE
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 11;
        typedef NOTIFY_CHAINLOCK_VOTE_request request;
    };

    // An assembled ChainLock (>= threshold votes for the same height + blockHash).
    struct NOTIFY_CHAINLOCK_request
    {
        uint32_t height;
        Crypto::Hash blockHash;
        // votes: each entry is [mnId(32) | signingKey(32) | sig(64)]
        std::vector<BinaryArray> votes;

        void serialize(ISerializer &s)
        {
            KV_MEMBER(height)
            s.binary(&blockHash, sizeof(blockHash), "block_hash");
            // votes: each entry is mnId(32)|signingKey(32)|sig(64) = 128 bytes
            // serializeAsBinary cannot handle vector<BinaryArray> (non-POD element),
            // so we serialize as count + flat packed blob.
            constexpr uint64_t VOTE_SIZE = 128;
            uint64_t voteCount = static_cast<uint64_t>(votes.size());
            s(voteCount, "vote_count");
            if (voteCount > 0)
            {
                if (s.type() == ISerializer::OUTPUT)
                {
                    BinaryArray flat;
                    flat.reserve(static_cast<size_t>(voteCount) * VOTE_SIZE);
                    for (const auto &v : votes)
                        flat.insert(flat.end(), v.begin(), v.end());
                    s.binary(flat.data(), flat.size(), "votes_data");
                }
                else
                {
                    BinaryArray flat(static_cast<size_t>(voteCount) * VOTE_SIZE, 0);
                    s.binary(flat.data(), flat.size(), "votes_data");
                    votes.resize(static_cast<size_t>(voteCount));
                    for (uint64_t i = 0; i < voteCount; ++i)
                        votes[i].assign(flat.begin() + i * VOTE_SIZE,
                                        flat.begin() + (i + 1) * VOTE_SIZE);
                }
            }
        }
    };

    struct NOTIFY_CHAINLOCK
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 12;
        typedef NOTIFY_CHAINLOCK_request request;
    };

    // -------------------------------------------------------------------------
    // InstantSend messages (IDs 2013-2014)
    // -------------------------------------------------------------------------

    // A single masternode's vote to IS-lock a transaction.
    struct NOTIFY_INSTANTSEND_VOTE_request
    {
        Crypto::Hash txHash;
        Crypto::Hash masternodeId;
        Crypto::PublicKey signingKey;
        Crypto::Signature signature;

        void serialize(ISerializer &s)
        {
            s.binary(&txHash, sizeof(txHash), "tx_hash");
            s.binary(&masternodeId, sizeof(masternodeId), "mn_id");
            s.binary(&signingKey, sizeof(signingKey), "signing_key");
            s.binary(&signature, sizeof(signature), "sig");
        }
    };

    struct NOTIFY_INSTANTSEND_VOTE
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 13;
        typedef NOTIFY_INSTANTSEND_VOTE_request request;
    };

    // An assembled InstantSend lock.
    struct NOTIFY_INSTANTSEND_LOCK_request
    {
        Crypto::Hash txHash;
        // key_images: each is 32 bytes serialized as BinaryArray
        std::vector<BinaryArray> keyImages;
        // votes: each entry is [mnId(32) | signingKey(32) | sig(64)]
        std::vector<BinaryArray> votes;

        void serialize(ISerializer &s)
        {
            s.binary(&txHash, sizeof(txHash), "tx_hash");
            // keyImages: each is a 32-byte Crypto::KeyImage packed as BinaryArray.
            // votes: each entry is mnId(32)|signingKey(32)|sig(64) = 128 bytes.
            // serializeAsBinary cannot handle vector<BinaryArray> (non-POD element),
            // so serialize each collection as count + flat packed blob.
            constexpr uint64_t KI_SIZE   = 32;
            constexpr uint64_t VOTE_SIZE = 128;

            uint64_t kiCount = static_cast<uint64_t>(keyImages.size());
            s(kiCount, "ki_count");
            if (kiCount > 0)
            {
                if (s.type() == ISerializer::OUTPUT)
                {
                    BinaryArray flat;
                    flat.reserve(static_cast<size_t>(kiCount) * KI_SIZE);
                    for (const auto &ki : keyImages)
                        flat.insert(flat.end(), ki.begin(), ki.end());
                    s.binary(flat.data(), flat.size(), "key_images");
                }
                else
                {
                    BinaryArray flat(static_cast<size_t>(kiCount) * KI_SIZE, 0);
                    s.binary(flat.data(), flat.size(), "key_images");
                    keyImages.resize(static_cast<size_t>(kiCount));
                    for (uint64_t i = 0; i < kiCount; ++i)
                        keyImages[i].assign(flat.begin() + i * KI_SIZE,
                                            flat.begin() + (i + 1) * KI_SIZE);
                }
            }

            uint64_t voteCount = static_cast<uint64_t>(votes.size());
            s(voteCount, "vote_count");
            if (voteCount > 0)
            {
                if (s.type() == ISerializer::OUTPUT)
                {
                    BinaryArray flat;
                    flat.reserve(static_cast<size_t>(voteCount) * VOTE_SIZE);
                    for (const auto &v : votes)
                        flat.insert(flat.end(), v.begin(), v.end());
                    s.binary(flat.data(), flat.size(), "votes_data");
                }
                else
                {
                    BinaryArray flat(static_cast<size_t>(voteCount) * VOTE_SIZE, 0);
                    s.binary(flat.data(), flat.size(), "votes_data");
                    votes.resize(static_cast<size_t>(voteCount));
                    for (uint64_t i = 0; i < voteCount; ++i)
                        votes[i].assign(flat.begin() + i * VOTE_SIZE,
                                        flat.begin() + (i + 1) * VOTE_SIZE);
                }
            }
        }
    };

    struct NOTIFY_INSTANTSEND_LOCK
    {
        const static int ID = BC_COMMANDS_POOL_BASE + 14;
        typedef NOTIFY_INSTANTSEND_LOCK_request request;
    };

} // namespace CryptoNote
