// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "crypto/WalletCrypto.h"

#include "crypto/aes_cbc.h"
#include "crypto/sha256.h"

#include <cstring>

namespace WalletCrypto
{
    std::vector<uint8_t> deriveKey(
        const std::string &password,
        const uint8_t *salt,
        const size_t saltLength,
        const uint64_t iterations,
        const size_t keyLength)
    {
        std::vector<uint8_t> key(keyLength);

        pbkdf2_hmac_sha256(
            reinterpret_cast<const uint8_t *>(password.data()),
            password.size(),
            salt,
            saltLength,
            iterations,
            key.data(),
            key.size());

        return key;
    }

    std::string encrypt(const std::string &plaintext, const uint8_t key[KEY_SIZE], const uint8_t iv[KEY_SIZE])
    {
        /* PKCS#7 always appends at least one byte and pads out to a whole
           block, so the output is one block longer than the input rounded
           down to a block boundary. */
        std::string output(plaintext.size() + AES_CBC_BLOCK_SIZE - (plaintext.size() % AES_CBC_BLOCK_SIZE), '\0');

        const size_t written = aes128_cbc_encrypt(
            key,
            iv,
            reinterpret_cast<const uint8_t *>(plaintext.data()),
            plaintext.size(),
            reinterpret_cast<uint8_t *>(&output[0]));

        output.resize(written);

        return output;
    }

    std::optional<std::string>
        decrypt(const std::string &ciphertext, const uint8_t key[KEY_SIZE], const uint8_t iv[KEY_SIZE])
    {
        if (ciphertext.empty() || ciphertext.size() % AES_CBC_BLOCK_SIZE != 0)
        {
            return std::nullopt;
        }

        std::string output(ciphertext.size(), '\0');

        size_t plaintextLength = 0;

        const int ok = aes128_cbc_decrypt(
            key,
            iv,
            reinterpret_cast<const uint8_t *>(ciphertext.data()),
            ciphertext.size(),
            reinterpret_cast<uint8_t *>(&output[0]),
            &plaintextLength);

        if (!ok)
        {
            /* Do not leave a partially decrypted buffer around, and do not
               tell the caller why this failed. */
            std::memset(&output[0], 0, output.size());

            return std::nullopt;
        }

        output.resize(plaintextLength);

        return output;
    }
} // namespace WalletCrypto
