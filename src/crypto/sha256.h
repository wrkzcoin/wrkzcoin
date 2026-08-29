// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/*
 * SHA-256 (FIPS 180-4), HMAC-SHA256 (RFC 2104) and PBKDF2-HMAC-SHA256
 * (RFC 8018 section 5.2).
 *
 * These exist to encrypt and decrypt the wallet file and to hash the
 * wallet-api password, which is all the Crypto++ dependency was ever used
 * for. The wallet file format is fixed, so this must stay byte-for-byte
 * compatible with CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256>.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE 64

    typedef struct
    {
        uint32_t state[8];

        /* Message length in bytes. SHA-256 tops out at 2^64 bits, which we
           will never approach - a wallet file is measured in megabytes. */
        uint64_t length;

        uint8_t buffer[SHA256_BLOCK_SIZE];

        size_t bufferLength;
    } sha256_ctx;

    void sha256_init(sha256_ctx *ctx);

    void sha256_update(sha256_ctx *ctx, const void *data, size_t length);

    void sha256_final(sha256_ctx *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

    /* One-shot convenience wrapper. */
    void sha256(const void *data, size_t length, uint8_t digest[SHA256_DIGEST_SIZE]);

    /*
     * HMAC-SHA256 with the key schedule kept between calls.
     *
     * PBKDF2 applies HMAC with the same key hundreds of thousands of times,
     * and re-deriving the padded key every time doubles the work. Splitting the
     * context lets the two block compressions for ipad/opad happen once.
     */
    typedef struct
    {
        /* State after absorbing the ipad block. */
        sha256_ctx inner;

        /* State after absorbing the opad block. */
        sha256_ctx outer;
    } hmac_sha256_ctx;

    void hmac_sha256_init(hmac_sha256_ctx *ctx, const uint8_t *key, size_t keyLength);

    /* Resets to just-after-ipad so the same key schedule can be reused. */
    void hmac_sha256_reset(hmac_sha256_ctx *ctx, const hmac_sha256_ctx *initialised);

    void hmac_sha256_update(hmac_sha256_ctx *ctx, const void *data, size_t length);

    void hmac_sha256_final(hmac_sha256_ctx *ctx, uint8_t mac[SHA256_DIGEST_SIZE]);

    void hmac_sha256(
        const uint8_t *key,
        size_t keyLength,
        const void *data,
        size_t dataLength,
        uint8_t mac[SHA256_DIGEST_SIZE]);

    /*
     * PBKDF2-HMAC-SHA256.
     *
     * iterations must be >= 1. outputLength may be any positive value; the
     * wallet uses 16 (an AES-128 key).
     */
    void pbkdf2_hmac_sha256(
        const uint8_t *password,
        size_t passwordLength,
        const uint8_t *salt,
        size_t saltLength,
        uint64_t iterations,
        uint8_t *output,
        size_t outputLength);

#if defined(__cplusplus)
}
#endif
