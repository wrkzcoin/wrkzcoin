// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"
#include "json_fwd.hpp"

#include <variant>
#include <cstdint>
#include <common/StringTools.h>
#include <vector>

namespace CryptoNote
{
    struct BaseInput
    {
        uint32_t blockIndex;
    };

    struct KeyInput
    {
        uint64_t amount;
        std::vector<uint32_t> outputIndexes;
        Crypto::KeyImage keyImage;
    };

    struct KeyOutput
    {
        Crypto::PublicKey key;
    };

    typedef std::variant<BaseInput, KeyInput> TransactionInput;

    typedef std::variant<KeyOutput> TransactionOutputTarget;

    struct TransactionOutput
    {
        uint64_t amount;
        TransactionOutputTarget target;
    };

    struct TransactionPrefix
    {
        uint8_t version;
        uint64_t unlockTime;
        std::vector<TransactionInput> inputs;
        std::vector<TransactionOutput> outputs;
        std::vector<uint8_t> extra;
    };

    struct Transaction : public TransactionPrefix
    {
        std::vector<std::vector<Crypto::Signature>> signatures;
    };

    struct BaseTransaction : public TransactionPrefix
    {
    };

    struct ParentBlock
    {
        uint8_t majorVersion;
        uint8_t minorVersion;
        Crypto::Hash previousBlockHash;
        uint16_t transactionCount;
        std::vector<Crypto::Hash> baseTransactionBranch;
        BaseTransaction baseTransaction;
        std::vector<Crypto::Hash> blockchainBranch;
    };

    struct BlockHeader
    {
        uint8_t majorVersion;
        uint8_t minorVersion;
        uint32_t nonce;
        uint64_t timestamp;
        Crypto::Hash previousBlockHash;
    };

    struct BlockTemplate : public BlockHeader
    {
        ParentBlock parentBlock;
        Transaction baseTransaction;
        std::vector<Crypto::Hash> transactionHashes;
    };

    struct AccountPublicAddress
    {
        Crypto::PublicKey spendPublicKey;
        Crypto::PublicKey viewPublicKey;
    };

    struct AccountKeys
    {
        AccountPublicAddress address;
        Crypto::SecretKey spendSecretKey;
        Crypto::SecretKey viewSecretKey;
    };

    struct KeyPair
    {
        Crypto::PublicKey publicKey;
        Crypto::SecretKey secretKey;
    };

    using BinaryArray = std::vector<uint8_t>;

    struct RawBlock
    {
        BinaryArray block; // BlockTemplate
        std::vector<BinaryArray> transactions;

    };

    /* JSON (de)serialisation for the wire types above. The bodies live in
       src/common/CryptoNoteJson.cpp so this header only needs json_fwd.hpp. */
    void to_json(nlohmann::json &j, const CryptoNote::KeyInput &k);

    void from_json(const nlohmann::json &j, CryptoNote::KeyInput &k);

    void to_json(nlohmann::json &j, const CryptoNote::RawBlock &block);

    void from_json(const nlohmann::json &j, CryptoNote::RawBlock &block);

} // namespace CryptoNote
