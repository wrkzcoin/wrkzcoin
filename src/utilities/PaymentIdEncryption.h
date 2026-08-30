// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Utilities
{
    /* The number of bytes in a short payment ID. */
    constexpr size_t SHORT_PAYMENT_ID_SIZE = 8;

    /* Domain separation byte appended to the key derivation before hashing.
       This is the same constant Monero uses (ENCRYPTED_PAYMENT_ID_TAIL), so
       our scheme is byte for byte identical to theirs. */
    constexpr uint8_t ENCRYPTED_PAYMENT_ID_TAIL = 0x8d;

    /* XOR a short payment ID with a keystream derived from the shared secret
       between the sender and the receiver.

       The sender passes the receiver's public view key and the transaction
       private key. The receiver passes the transaction public key and their
       own private view key. Both arrive at the same key derivation, and
       therefore the same keystream.

       The operation is symmetric - calling this once encrypts, calling it
       again with the same keys decrypts.

       Returns false (leaving paymentId untouched) if the payment ID is not
       exactly SHORT_PAYMENT_ID_SIZE bytes, or the derivation failed because
       the public key was not a valid curve point. */
    bool encryptPaymentId(
        std::vector<uint8_t> &paymentId,
        const Crypto::PublicKey &publicKey,
        const Crypto::SecretKey &secretKey);

    /* Convenience wrapper taking and returning a 16 character hex string.
       Returns an empty string if the input is not valid hex of the right
       length, or the derivation failed. */
    std::string encryptPaymentIdHex(
        const std::string &paymentIdHex,
        const Crypto::PublicKey &publicKey,
        const Crypto::SecretKey &secretKey);
} // namespace Utilities
