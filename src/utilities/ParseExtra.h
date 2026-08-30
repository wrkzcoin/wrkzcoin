// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <string>
#include <vector>

namespace Utilities
{
    struct MergedMiningTag
    {
        uint8_t depth;
        Crypto::Hash merkleRoot;
    };

    struct ParsedExtra
    {
        Crypto::PublicKey transactionPublicKey;
        std::string paymentID;
        MergedMiningTag mergedMiningTag;
        std::vector<uint8_t> extraData;
        uint64_t transactionPowNonce = 0;

        /* True when paymentID holds ciphertext rather than plaintext - i.e.
           the transaction carried a short (8 byte) encrypted payment ID.
           Recovering the plaintext needs the private view key of one of the
           two parties, so it can only happen wallet side.

           Note this field is deliberately NOT sent over the wallet sync
           protocol. It does not need to be: a long payment ID is always 64
           hex characters and is always plaintext, and since we no longer
           surface the legacy plaintext short payment ID, a 16 character
           payment ID is always encrypted. Length alone is sufficient.

           New members must be appended here - ParsedExtra is aggregate
           initialised positionally in parseExtra(). */
        bool paymentIDEncrypted = false;
    };

    std::string getPaymentIDFromExtra(const std::vector<uint8_t> &extra);

    Crypto::PublicKey getTransactionPublicKeyFromExtra(const std::vector<uint8_t> &extra);

    MergedMiningTag getMergedMiningTagFromExtra(const std::vector<uint8_t> &extra);

    std::vector<uint8_t> getExtraDataFromExtra(const std::vector<uint8_t> &extra);

    ParsedExtra parseExtra(const std::vector<uint8_t> &extra);
} // namespace Utilities
