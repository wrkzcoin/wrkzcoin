// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/*
 * The password-based encryption used by the wallet file and the wallet-api
 * password hash. This is the whole of what Crypto++ used to provide us.
 *
 * The parameters below are fixed by the on-disk wallet format and must not be
 * changed without a file format migration:
 *
 *   key derivation   PBKDF2-HMAC-SHA256
 *   key length       16 bytes (AES-128)
 *   salt length      16 bytes
 *   cipher           AES-128-CBC, PKCS#7 padding
 *   iv               the salt (see the note on encrypt() below)
 *   iterations       Constants::PBKDF2_ITERATIONS for wallet files (500000),
 *                    ApiConstants::PBKDF2_ITERATIONS for the api password
 *                    hash (10000) - these are different and not interchangeable
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace WalletCrypto
{
    constexpr size_t KEY_SIZE = 16;

    constexpr size_t SALT_SIZE = 16;

    /* Derives a key of keyLength bytes from the password with
       PBKDF2-HMAC-SHA256. */
    std::vector<uint8_t> deriveKey(
        const std::string &password,
        const uint8_t *salt,
        size_t saltLength,
        uint64_t iterations,
        size_t keyLength);

    /*
     * Encrypts with AES-128-CBC and PKCS#7 padding.
     *
     * The wallet format reuses the PBKDF2 salt as the CBC IV. That is not how
     * an IV is meant to be chosen, but it is what every wallet file on disk
     * was written with, so it stays. Since the salt is freshly random on every
     * save, the (key, iv) pair is still never reused across saves.
     */
    std::string encrypt(const std::string &plaintext, const uint8_t key[KEY_SIZE], const uint8_t iv[KEY_SIZE]);

    /*
     * Decrypts and strips PKCS#7 padding.
     *
     * Returns nullopt when the ciphertext length is wrong or the padding does
     * not verify. Callers must report a single generic failure for the empty
     * result - reporting "bad padding" separately from "wrong password" would
     * be a padding oracle.
     */
    std::optional<std::string>
        decrypt(const std::string &ciphertext, const uint8_t key[KEY_SIZE], const uint8_t iv[KEY_SIZE]);
} // namespace WalletCrypto
