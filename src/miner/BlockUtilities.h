// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNote.h"
#include "CryptoTypes.h"

#include <cstdint>
#include <functional>
#include <vector>

/* A block's hashing blob, with the offset of the four nonce bytes inside it.
   Everything else in the blob is the same for every attempt at the same block,
   so a worker builds this once per job and then writes each nonce straight
   into the blob rather than re-serializing the whole block per hash. */
struct PreparedBlockHashingBlob
{
    std::vector<uint8_t> blob;

    size_t nonceOffset = 0;

    std::function<void(const void *data, size_t length, Crypto::Hash &hash)> algorithm;
};

/* Throws if patching the nonce in place would not reproduce exactly what
   serializing the block produces - see the check in the implementation. */
PreparedBlockHashingBlob prepareBlockHashingBlob(const CryptoNote::BlockTemplate &block);

Crypto::Hash hashPreparedBlob(PreparedBlockHashingBlob &prepared, const uint32_t nonce);

std::vector<uint8_t> getParentBlockBinaryArray(const CryptoNote::BlockTemplate &block, const bool headerOnly);

std::vector<uint8_t> getParentBlockHashingBinaryArray(const CryptoNote::BlockTemplate &block, const bool headerOnly);

std::vector<uint8_t>
    getParentBinaryArray(const CryptoNote::BlockTemplate &block, const bool hashTransaction, const bool headerOnly);

std::vector<uint8_t> getBlockHashingBinaryArray(const CryptoNote::BlockTemplate &block);

Crypto::Hash getBlockHash(const CryptoNote::BlockTemplate &block);

Crypto::Hash getMerkleRoot(const CryptoNote::BlockTemplate &block);

Crypto::Hash getBlockLongHash(const CryptoNote::BlockTemplate &block);
