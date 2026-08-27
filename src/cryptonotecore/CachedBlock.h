// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <optional>

namespace CryptoNote
{
    class CachedBlock
    {
      public:
        explicit CachedBlock(const BlockTemplate &block);

        const BlockTemplate &getBlock() const;

        const Crypto::Hash &getTransactionTreeHash() const;

        const Crypto::Hash &getBlockHash() const;

        const Crypto::Hash &getBlockLongHash() const;

        const Crypto::Hash &getAuxiliaryBlockHeaderHash() const;

        const BinaryArray &getBlockHashingBinaryArray() const;

        const BinaryArray &getParentBlockBinaryArray(bool headerOnly) const;

        const BinaryArray &getParentBlockHashingBinaryArray(bool headerOnly) const;

        uint32_t getBlockIndex() const;

      private:
        const BlockTemplate &block;

        mutable std::optional<BinaryArray> blockHashingBinaryArray;

        mutable std::optional<BinaryArray> parentBlockBinaryArray;

        mutable std::optional<BinaryArray> parentBlockHashingBinaryArray;

        mutable std::optional<BinaryArray> parentBlockBinaryArrayHeaderOnly;

        mutable std::optional<BinaryArray> parentBlockHashingBinaryArrayHeaderOnly;

        mutable std::optional<uint32_t> blockIndex;

        mutable std::optional<Crypto::Hash> transactionTreeHash;

        mutable std::optional<Crypto::Hash> blockHash;

        mutable std::optional<Crypto::Hash> blockLongHash;

        mutable std::optional<Crypto::Hash> auxiliaryBlockHeaderHash;
    };

} // namespace CryptoNote
