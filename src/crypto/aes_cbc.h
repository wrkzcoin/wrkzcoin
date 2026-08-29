// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/*
 * AES-128 in CBC mode with PKCS#7 padding (FIPS 197, NIST SP 800-38A).
 *
 * This is the wallet file cipher. It replaces CryptoPP::CBC_Mode<AES> driven
 * through a StreamTransformationFilter, whose default padding is PKCS#7, so
 * the ciphertext layout is unchanged and existing wallet files still open.
 *
 * Note on the existing AES code in this directory: neither implementation is
 * reusable here. aesb.c exposes only the CryptoNight round functions, which
 * take an already-expanded key and do not perform the FIPS key schedule;
 * oaes_lib.c does have a real key schedule, but its encrypt entry point wraps
 * the output in an OAES container with its own header rather than emitting a
 * raw CBC stream.
 *
 * SIDE CHANNELS: this is a table-based implementation, so S-box lookups are
 * key dependent and observable through the data cache. Crypto++ dispatched to
 * AES-NI at runtime and was constant time on hardware that has it. The threat
 * model here is a wallet file encrypted under a local password, where an
 * attacker able to observe our cache can already read the file and the
 * keystrokes, so this is an acceptable trade for removing a 20 MB dependency
 * built from 196 translation units - but it is a real difference, and adding
 * an AES-NI path with runtime dispatch would close it.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define AES_CBC_BLOCK_SIZE 16
#define AES_CBC_KEY_SIZE 16

    /*
     * Encrypts with PKCS#7 padding.
     *
     * output must have room for inputLength rounded up to the next multiple of
     * AES_CBC_BLOCK_SIZE. Note that a whole extra block is appended when
     * inputLength is already a multiple of the block size, as PKCS#7 requires.
     * Returns the number of bytes written.
     */
    size_t aes128_cbc_encrypt(
        const uint8_t key[AES_CBC_KEY_SIZE],
        const uint8_t iv[AES_CBC_BLOCK_SIZE],
        const uint8_t *input,
        size_t inputLength,
        uint8_t *output);

    /*
     * Decrypts and strips PKCS#7 padding.
     *
     * output must have room for inputLength bytes. On success writes the
     * plaintext length to *outputLength and returns 1. Returns 0 if the input
     * length is not a positive multiple of the block size, or if the padding
     * is not well formed - which for our callers means a wrong password.
     *
     * Callers MUST NOT distinguish a padding failure from any other decryption
     * failure in what they report, or they hand an attacker a padding oracle.
     */
    int aes128_cbc_decrypt(
        const uint8_t key[AES_CBC_KEY_SIZE],
        const uint8_t iv[AES_CBC_BLOCK_SIZE],
        const uint8_t *input,
        size_t inputLength,
        uint8_t *output,
        size_t *outputLength);

#if defined(__cplusplus)
}
#endif
