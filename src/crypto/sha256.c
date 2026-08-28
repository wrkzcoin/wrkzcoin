// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "crypto/sha256.h"

#include <string.h>

/* FIPS 180-4 section 4.2.2 - first 32 bits of the fractional parts of the
   cube roots of the first 64 primes. */
static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

static uint32_t rotr32(uint32_t x, unsigned int n)
{
    return (x >> n) | (x << (32 - n));
}

#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (rotr32((x), 2) ^ rotr32((x), 13) ^ rotr32((x), 22))
#define BSIG1(x) (rotr32((x), 6) ^ rotr32((x), 11) ^ rotr32((x), 25))
#define SSIG0(x) (rotr32((x), 7) ^ rotr32((x), 18) ^ ((x) >> 3))
#define SSIG1(x) (rotr32((x), 17) ^ rotr32((x), 19) ^ ((x) >> 10))

static void sha256_compress(uint32_t state[8], const uint8_t block[SHA256_BLOCK_SIZE])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    unsigned int i;

    for (i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16)
               | ((uint32_t)block[i * 4 + 2] << 8) | ((uint32_t)block[i * 4 + 3]);
    }

    for (i = 16; i < 64; i++)
    {
        w[i] = SSIG1(w[i - 2]) + w[i - 7] + SSIG0(w[i - 15]) + w[i - 16];
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 64; i++)
    {
        const uint32_t t1 = h + BSIG1(e) + CH(e, f, g) + K[i] + w[i];
        const uint32_t t2 = BSIG0(a) + MAJ(a, b, c);

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256_init(sha256_ctx *ctx)
{
    /* FIPS 180-4 section 5.3.3 */
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;

    ctx->length = 0;
    ctx->bufferLength = 0;
}

void sha256_update(sha256_ctx *ctx, const void *data, size_t length)
{
    const uint8_t *in = (const uint8_t *)data;

    ctx->length += (uint64_t)length;

    /* Top up a partially filled buffer first. */
    if (ctx->bufferLength > 0)
    {
        size_t want = SHA256_BLOCK_SIZE - ctx->bufferLength;

        if (want > length)
        {
            want = length;
        }

        memcpy(ctx->buffer + ctx->bufferLength, in, want);

        ctx->bufferLength += want;
        in += want;
        length -= want;

        if (ctx->bufferLength == SHA256_BLOCK_SIZE)
        {
            sha256_compress(ctx->state, ctx->buffer);
            ctx->bufferLength = 0;
        }
    }

    while (length >= SHA256_BLOCK_SIZE)
    {
        sha256_compress(ctx->state, in);
        in += SHA256_BLOCK_SIZE;
        length -= SHA256_BLOCK_SIZE;
    }

    if (length > 0)
    {
        memcpy(ctx->buffer, in, length);
        ctx->bufferLength = length;
    }
}

void sha256_final(sha256_ctx *ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
    const uint64_t bitLength = ctx->length * 8;
    unsigned int i;

    /* Append 0x80, then zeroes, then the 64 bit big endian bit count. */
    ctx->buffer[ctx->bufferLength++] = 0x80;

    if (ctx->bufferLength > SHA256_BLOCK_SIZE - 8)
    {
        memset(ctx->buffer + ctx->bufferLength, 0, SHA256_BLOCK_SIZE - ctx->bufferLength);
        sha256_compress(ctx->state, ctx->buffer);
        ctx->bufferLength = 0;
    }

    memset(ctx->buffer + ctx->bufferLength, 0, SHA256_BLOCK_SIZE - 8 - ctx->bufferLength);

    for (i = 0; i < 8; i++)
    {
        ctx->buffer[SHA256_BLOCK_SIZE - 1 - i] = (uint8_t)(bitLength >> (8 * i));
    }

    sha256_compress(ctx->state, ctx->buffer);

    for (i = 0; i < 8; i++)
    {
        digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void sha256(const void *data, size_t length, uint8_t digest[SHA256_DIGEST_SIZE])
{
    sha256_ctx ctx;

    sha256_init(&ctx);
    sha256_update(&ctx, data, length);
    sha256_final(&ctx, digest);
}

void hmac_sha256_init(hmac_sha256_ctx *ctx, const uint8_t *key, size_t keyLength)
{
    uint8_t padded[SHA256_BLOCK_SIZE];
    uint8_t block[SHA256_BLOCK_SIZE];
    unsigned int i;

    memset(padded, 0, sizeof(padded));

    /* RFC 2104: keys longer than the block size are hashed down first. */
    if (keyLength > SHA256_BLOCK_SIZE)
    {
        sha256(key, keyLength, padded);
    }
    else if (keyLength > 0)
    {
        memcpy(padded, key, keyLength);
    }

    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
    {
        block[i] = (uint8_t)(padded[i] ^ 0x36);
    }

    sha256_init(&ctx->inner);
    sha256_update(&ctx->inner, block, SHA256_BLOCK_SIZE);

    for (i = 0; i < SHA256_BLOCK_SIZE; i++)
    {
        block[i] = (uint8_t)(padded[i] ^ 0x5c);
    }

    sha256_init(&ctx->outer);
    sha256_update(&ctx->outer, block, SHA256_BLOCK_SIZE);

    /* Do not leave the padded key on the stack. */
    memset(padded, 0, sizeof(padded));
    memset(block, 0, sizeof(block));
}

void hmac_sha256_reset(hmac_sha256_ctx *ctx, const hmac_sha256_ctx *initialised)
{
    *ctx = *initialised;
}

void hmac_sha256_update(hmac_sha256_ctx *ctx, const void *data, size_t length)
{
    sha256_update(&ctx->inner, data, length);
}

void hmac_sha256_final(hmac_sha256_ctx *ctx, uint8_t mac[SHA256_DIGEST_SIZE])
{
    uint8_t innerDigest[SHA256_DIGEST_SIZE];

    sha256_final(&ctx->inner, innerDigest);

    sha256_update(&ctx->outer, innerDigest, sizeof(innerDigest));
    sha256_final(&ctx->outer, mac);

    memset(innerDigest, 0, sizeof(innerDigest));
}

void hmac_sha256(
    const uint8_t *key,
    size_t keyLength,
    const void *data,
    size_t dataLength,
    uint8_t mac[SHA256_DIGEST_SIZE])
{
    hmac_sha256_ctx ctx;

    hmac_sha256_init(&ctx, key, keyLength);
    hmac_sha256_update(&ctx, data, dataLength);
    hmac_sha256_final(&ctx, mac);

    memset(&ctx, 0, sizeof(ctx));
}

void pbkdf2_hmac_sha256(
    const uint8_t *password,
    size_t passwordLength,
    const uint8_t *salt,
    size_t saltLength,
    uint64_t iterations,
    uint8_t *output,
    size_t outputLength)
{
    hmac_sha256_ctx prf;
    uint32_t blockIndex = 1;

    if (outputLength == 0)
    {
        return;
    }

    /* The key schedule depends only on the password, so build it once and
       copy it for each of the 2 * iterations HMAC evaluations below. */
    hmac_sha256_init(&prf, password, passwordLength);

    while (outputLength > 0)
    {
        uint8_t accumulator[SHA256_DIGEST_SIZE];
        uint8_t current[SHA256_DIGEST_SIZE];
        uint8_t counter[4];
        hmac_sha256_ctx ctx;
        uint64_t iteration;
        size_t take;
        unsigned int i;

        counter[0] = (uint8_t)(blockIndex >> 24);
        counter[1] = (uint8_t)(blockIndex >> 16);
        counter[2] = (uint8_t)(blockIndex >> 8);
        counter[3] = (uint8_t)(blockIndex);

        /* U_1 = PRF(password, salt || INT_BE32(blockIndex)) */
        hmac_sha256_reset(&ctx, &prf);
        hmac_sha256_update(&ctx, salt, saltLength);
        hmac_sha256_update(&ctx, counter, sizeof(counter));
        hmac_sha256_final(&ctx, current);

        memcpy(accumulator, current, sizeof(accumulator));

        /* U_n = PRF(password, U_{n-1}), accumulated by xor. */
        for (iteration = 1; iteration < iterations; iteration++)
        {
            hmac_sha256_reset(&ctx, &prf);
            hmac_sha256_update(&ctx, current, sizeof(current));
            hmac_sha256_final(&ctx, current);

            for (i = 0; i < SHA256_DIGEST_SIZE; i++)
            {
                accumulator[i] ^= current[i];
            }
        }

        take = outputLength < SHA256_DIGEST_SIZE ? outputLength : SHA256_DIGEST_SIZE;
        memcpy(output, accumulator, take);

        output += take;
        outputLength -= take;
        blockIndex++;

        memset(accumulator, 0, sizeof(accumulator));
        memset(current, 0, sizeof(current));
        memset(&ctx, 0, sizeof(ctx));
    }

    memset(&prf, 0, sizeof(prf));
}
