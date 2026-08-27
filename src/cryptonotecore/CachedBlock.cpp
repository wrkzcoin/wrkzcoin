// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "CachedBlock.h"

#include "common/CryptoNoteTools.h"

#include <common/Varint.h>
#include <config/CryptoNoteConfig.h>
#include <variant>

using namespace Crypto;
using namespace CryptoNote;

CachedBlock::CachedBlock(const BlockTemplate &block): block(block) {}

const BlockTemplate &CachedBlock::getBlock() const
{
    return block;
}

const Crypto::Hash &CachedBlock::getTransactionTreeHash() const
{
    if (!transactionTreeHash.has_value())
    {
        std::vector<Crypto::Hash> transactionHashes;
        transactionHashes.reserve(block.transactionHashes.size() + 1);
        transactionHashes.push_back(getObjectHash(block.baseTransaction));
        transactionHashes.insert(
            transactionHashes.end(), block.transactionHashes.begin(), block.transactionHashes.end());
        transactionTreeHash = Crypto::Hash();
        Crypto::tree_hash(transactionHashes.data(), transactionHashes.size(), transactionTreeHash.value());
    }

    return transactionTreeHash.value();
}

const Crypto::Hash &CachedBlock::getBlockHash() const
{
    if (!blockHash.has_value())
    {
        BinaryArray blockBinaryArray = getBlockHashingBinaryArray();
        if (BLOCK_MAJOR_VERSION_2 <= block.majorVersion)
        {
            const auto &parentBlock = getParentBlockHashingBinaryArray(false);
            blockBinaryArray.insert(blockBinaryArray.end(), parentBlock.begin(), parentBlock.end());
        }

        blockHash = getObjectHash(blockBinaryArray);
    }

    return blockHash.value();
}

const Crypto::Hash &CachedBlock::getBlockLongHash() const
{
    if (blockLongHash.has_value())
    {
        return blockLongHash.value();
    }

    const std::vector<uint8_t> &rawHashingBlock = block.majorVersion == CryptoNote::BLOCK_MAJOR_VERSION_1
                                                      ? getBlockHashingBinaryArray()
                                                      : getParentBlockHashingBinaryArray(true);

    blockLongHash = Hash();

    try
    {
        const auto hashingAlgorithm = CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.at(block.majorVersion);

        hashingAlgorithm(rawHashingBlock.data(), rawHashingBlock.size(), blockLongHash.value());

        return blockLongHash.value();
    }
    catch (const std::out_of_range &)
    {
        throw std::runtime_error("Unknown block major version.");
    }
}

const Crypto::Hash &CachedBlock::getAuxiliaryBlockHeaderHash() const
{
    if (!auxiliaryBlockHeaderHash.has_value())
    {
        auxiliaryBlockHeaderHash = getObjectHash(getBlockHashingBinaryArray());
    }

    return auxiliaryBlockHeaderHash.value();
}

const BinaryArray &CachedBlock::getBlockHashingBinaryArray() const
{
    if (!blockHashingBinaryArray.has_value())
    {
        blockHashingBinaryArray = BinaryArray();
        auto &result = blockHashingBinaryArray.value();
        if (!toBinaryArray(static_cast<const BlockHeader &>(block), result))
        {
            blockHashingBinaryArray.reset();
            throw std::runtime_error("Can't serialize BlockHeader");
        }

        const auto &treeHash = getTransactionTreeHash();
        result.insert(result.end(), treeHash.data, treeHash.data + 32);
        auto transactionCount = Common::asBinaryArray(Tools::get_varint_data(block.transactionHashes.size() + 1));
        result.insert(result.end(), transactionCount.begin(), transactionCount.end());
    }

    return blockHashingBinaryArray.value();
}

const BinaryArray &CachedBlock::getParentBlockBinaryArray(bool headerOnly) const
{
    if (headerOnly)
    {
        if (!parentBlockBinaryArrayHeaderOnly.has_value())
        {
            auto serializer = makeParentBlockSerializer(block, false, true);
            parentBlockBinaryArrayHeaderOnly = BinaryArray();
            if (!toBinaryArray(serializer, parentBlockBinaryArrayHeaderOnly.value()))
            {
                parentBlockBinaryArrayHeaderOnly.reset();
                throw std::runtime_error("Can't serialize parent block header.");
            }
        }

        return parentBlockBinaryArrayHeaderOnly.value();
    }
    else
    {
        if (!parentBlockBinaryArray.has_value())
        {
            auto serializer = makeParentBlockSerializer(block, false, false);
            parentBlockBinaryArray = BinaryArray();
            if (!toBinaryArray(serializer, parentBlockBinaryArray.value()))
            {
                parentBlockBinaryArray.reset();
                throw std::runtime_error("Can't serialize parent block.");
            }
        }

        return parentBlockBinaryArray.value();
    }
}

const BinaryArray &CachedBlock::getParentBlockHashingBinaryArray(bool headerOnly) const
{
    if (headerOnly)
    {
        if (!parentBlockHashingBinaryArrayHeaderOnly.has_value())
        {
            auto serializer = makeParentBlockSerializer(block, true, true);
            parentBlockHashingBinaryArrayHeaderOnly = BinaryArray();
            if (!toBinaryArray(serializer, parentBlockHashingBinaryArrayHeaderOnly.value()))
            {
                parentBlockHashingBinaryArrayHeaderOnly.reset();
                throw std::runtime_error("Can't serialize parent block header for hashing.");
            }
        }

        return parentBlockHashingBinaryArrayHeaderOnly.value();
    }
    else
    {
        if (!parentBlockHashingBinaryArray.has_value())
        {
            auto serializer = makeParentBlockSerializer(block, true, false);
            parentBlockHashingBinaryArray = BinaryArray();
            if (!toBinaryArray(serializer, parentBlockHashingBinaryArray.value()))
            {
                parentBlockHashingBinaryArray.reset();
                throw std::runtime_error("Can't serialize parent block for hashing.");
            }
        }

        return parentBlockHashingBinaryArray.value();
    }
}

uint32_t CachedBlock::getBlockIndex() const
{
    if (!blockIndex.has_value())
    {
        if (block.baseTransaction.inputs.size() != 1)
        {
            blockIndex = 0;
        }
        else
        {
            const auto &in = block.baseTransaction.inputs[0];
            if (!std::holds_alternative<BaseInput>(in))
            {
                blockIndex = 0;
            }
            else
            {
                blockIndex = std::get<BaseInput>(in).blockIndex;
            }
        }
    }

    return blockIndex.value();
}
