// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <utilities/PaymentIdEncryption.h>
/////////////////////////////////////////////

#include <common/StringTools.h>
#include <crypto/crypto.h>
#include <crypto/hash.h>
#include <cstring>

namespace Utilities
{
    bool encryptPaymentId(
        std::vector<uint8_t> &paymentId,
        const Crypto::PublicKey &publicKey,
        const Crypto::SecretKey &secretKey)
    {
        if (paymentId.size() != SHORT_PAYMENT_ID_SIZE)
        {
            return false;
        }

        /* The shared secret. generate_key_derivation() already multiplies by
           the cofactor, so this is 8 * secret * public, exactly as Monero
           computes it. */
        Crypto::KeyDerivation derivation;

        if (!Crypto::generate_key_derivation(publicKey, secretKey, derivation))
        {
            return false;
        }

        /* keccak(derivation || 0x8d) gives us the keystream. We only ever use
           the first 8 bytes of it. */
        uint8_t buffer[sizeof(Crypto::KeyDerivation) + 1];

        std::memcpy(buffer, &derivation, sizeof(derivation));

        buffer[sizeof(derivation)] = ENCRYPTED_PAYMENT_ID_TAIL;

        Crypto::Hash keystream;

        Crypto::cn_fast_hash(buffer, sizeof(buffer), keystream);

        for (size_t i = 0; i < SHORT_PAYMENT_ID_SIZE; i++)
        {
            paymentId[i] ^= static_cast<uint8_t>(keystream.data[i]);
        }

        return true;
    }

    std::string encryptPaymentIdHex(
        const std::string &paymentIdHex,
        const Crypto::PublicKey &publicKey,
        const Crypto::SecretKey &secretKey)
    {
        std::vector<uint8_t> paymentId;

        if (!Common::fromHex(paymentIdHex, paymentId) || paymentId.size() != SHORT_PAYMENT_ID_SIZE)
        {
            return std::string();
        }

        if (!encryptPaymentId(paymentId, publicKey, secretKey))
        {
            return std::string();
        }

        return Common::toHex(paymentId);
    }
} // namespace Utilities
