// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <miner/BlockUtilities.h>
/////////////////////////////////

#include <common/CryptoNoteTools.h>
#include <common/Varint.h>
#include <config/CryptoNoteConfig.h>
#include <cstring>
#include <serialization/CryptoNoteSerialization.h>
#include <serialization/SerializationTools.h>

namespace
{
    /* The bytes the proof of work is actually run over, which is the parent
       block for every version that has one. */
    std::vector<uint8_t> rawBlockHashingBlob(const CryptoNote::BlockTemplate &block)
    {
        return block.majorVersion == CryptoNote::BLOCK_MAJOR_VERSION_1
                   ? getBlockHashingBinaryArray(block)
                   : getParentBlockHashingBinaryArray(block, true);
    }

    void writeNonce(PreparedBlockHashingBlob &prepared, const uint32_t nonce)
    {
        std::memcpy(prepared.blob.data() + prepared.nonceOffset, &nonce, sizeof(nonce));
    }
} // namespace

std::vector<uint8_t> getParentBlockHashingBinaryArray(const CryptoNote::BlockTemplate &block, const bool headerOnly)
{
    return getParentBinaryArray(block, true, headerOnly);
}

std::vector<uint8_t> getParentBlockBinaryArray(const CryptoNote::BlockTemplate &block, const bool headerOnly)
{
    return getParentBinaryArray(block, false, headerOnly);
}

std::vector<uint8_t>
    getParentBinaryArray(const CryptoNote::BlockTemplate &block, const bool hashTransaction, const bool headerOnly)
{
    std::vector<uint8_t> binaryArray;

    auto serializer = makeParentBlockSerializer(block, hashTransaction, headerOnly);

    if (!toBinaryArray(serializer, binaryArray))
    {
        throw std::runtime_error("Can't serialize parent block");
    }

    return binaryArray;
}

std::vector<uint8_t> getBlockHashingBinaryArray(const CryptoNote::BlockTemplate &block)
{
    std::vector<uint8_t> blockHashingBinaryArray;

    if (!toBinaryArray(static_cast<const CryptoNote::BlockHeader &>(block), blockHashingBinaryArray))
    {
        throw std::runtime_error("Can't serialize BlockHeader");
    }

    std::vector<Crypto::Hash> transactionHashes;
    transactionHashes.reserve(block.transactionHashes.size() + 1);
    transactionHashes.push_back(getObjectHash(block.baseTransaction));
    transactionHashes.insert(transactionHashes.end(), block.transactionHashes.begin(), block.transactionHashes.end());

    Crypto::Hash treeHash;

    Crypto::tree_hash(transactionHashes.data(), transactionHashes.size(), treeHash);

    blockHashingBinaryArray.insert(blockHashingBinaryArray.end(), treeHash.data, treeHash.data + 32);

    auto transactionCount = Common::asBinaryArray(Tools::get_varint_data(block.transactionHashes.size() + 1));

    blockHashingBinaryArray.insert(blockHashingBinaryArray.end(), transactionCount.begin(), transactionCount.end());

    return blockHashingBinaryArray;
}

Crypto::Hash getBlockHash(const CryptoNote::BlockTemplate &block)
{
    auto blockHashingBinaryArray = getBlockHashingBinaryArray(block);

    if (block.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_2)
    {
        const auto &parentBlock = getParentBlockHashingBinaryArray(block, false);
        blockHashingBinaryArray.insert(blockHashingBinaryArray.end(), parentBlock.begin(), parentBlock.end());
    }

    return CryptoNote::getObjectHash(blockHashingBinaryArray);
}

Crypto::Hash getMerkleRoot(const CryptoNote::BlockTemplate &block)
{
    return CryptoNote::getObjectHash(getBlockHashingBinaryArray(block));
}

PreparedBlockHashingBlob prepareBlockHashingBlob(const CryptoNote::BlockTemplate &block)
{
    /* Two nonces sharing no bytes, so every byte the nonce reaches differs
       between the two blobs and the run of differing bytes is the nonce
       field itself, wherever the serializer decided to put it. */
    const uint32_t firstNonce = 0x11111111;

    const uint32_t secondNonce = 0xEEEEEEEE;

    CryptoNote::BlockTemplate probe = block;

    probe.nonce = firstNonce;
    std::vector<uint8_t> first = rawBlockHashingBlob(probe);

    probe.nonce = secondNonce;
    const std::vector<uint8_t> second = rawBlockHashingBlob(probe);

    if (first.size() != second.size() || first.size() < sizeof(uint32_t))
    {
        throw std::runtime_error("The block hashing blob changes size with the nonce");
    }

    size_t offset = 0;

    while (offset < first.size() && first[offset] == second[offset])
    {
        offset++;
    }

    if (offset + sizeof(uint32_t) > first.size())
    {
        throw std::runtime_error("Could not locate the nonce in the block hashing blob");
    }

    PreparedBlockHashingBlob prepared;
    prepared.blob = std::move(first);
    prepared.nonceOffset = offset;

    const auto algorithm = CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.find(block.majorVersion);

    if (algorithm == CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.end())
    {
        throw std::runtime_error("Unknown block major version.");
    }

    prepared.algorithm = algorithm->second;

    /* The blob still holds firstNonce, so writing secondNonce over those four
       bytes has to land on exactly the blob the serializer produced for it -
       byte order included, and with nothing else in the blob having moved.
       That is the entire correctness argument for reusing the blob instead of
       re-serializing, so it is proven once per block template rather than
       assumed. */
    writeNonce(prepared, secondNonce);

    if (prepared.blob != second)
    {
        throw std::runtime_error("Writing the nonce into the block hashing blob does not match serializing it");
    }

    writeNonce(prepared, block.nonce);

    return prepared;
}

Crypto::Hash hashPreparedBlob(PreparedBlockHashingBlob &prepared, const uint32_t nonce)
{
    writeNonce(prepared, nonce);

    Crypto::Hash hash;

    prepared.algorithm(prepared.blob.data(), prepared.blob.size(), hash);

    return hash;
}

Crypto::Hash getBlockLongHash(const CryptoNote::BlockTemplate &block)
{
    const std::vector<uint8_t> rawHashingBlock = rawBlockHashingBlob(block);

    Crypto::Hash hash;

    try
    {
        const auto hashingAlgorithm = CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.at(block.majorVersion);

        hashingAlgorithm(rawHashingBlock.data(), rawHashingBlock.size(), hash);

        return hash;
    }
    catch (const std::out_of_range &)
    {
        throw std::runtime_error("Unknown block major version.");
    }
}
