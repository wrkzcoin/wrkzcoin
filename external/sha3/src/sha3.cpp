/**
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include "sha3.h"

#include <stdexcept>

namespace SHA3
{
    static const uint64_t rndc[24] = {0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
                                      0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
                                      0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
                                      0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
                                      0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
                                      0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008};

    static const int rotc[24] = {1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
                                 27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44};

    static const int piln[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};

    typedef struct
    {
        union
        {
            uint8_t b[200];
            uint64_t q[25];
        } state;

        int pt, rsiz, message_digest_length;
    } sha_ctx_t;

    static void keccakf(uint64_t st[25])
    {
        int i, j, r;

        uint64_t t, bc[5];

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        uint8_t *v;

        // endianess conversion. this is redundant on little-endian targets
        for (i = 0; i < 25; i++)
        {
            v = (uint8_t *)&st[i];

            st[i] = ((uint64_t)v[0]) | (((uint64_t)v[1]) << 8) | (((uint64_t)v[2]) << 16) | (((uint64_t)v[3]) << 24)
                    | (((uint64_t)v[4]) << 32) | (((uint64_t)v[5]) << 40) | (((uint64_t)v[6]) << 48)
                    | (((uint64_t)v[7]) << 56);
        }
#endif

        for (r = 0; r < SHA3_KECCAKF_ROUNDS; r++)
        {
            for (i = 0; i < 5; i++)
                bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

            for (i = 0; i < 5; i++)
            {
                t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);

                for (j = 0; j < 25; j += 5)
                    st[j + i] ^= t;
            }

            t = st[1];

            for (i = 0; i < 24; i++)
            {
                j = piln[i];

                bc[0] = st[j];

                st[j] = ROTL64(t, rotc[i]);

                t = bc[0];
            }

            for (j = 0; j < 25; j += 5)
            {
                for (i = 0; i < 5; i++)
                    bc[i] = st[j + i];

                for (i = 0; i < 5; i++)
                    st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }

            st[0] ^= rndc[r];
        }

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        for (i = 0; i < 25; i++)
        {
            v = reinterpret_cast<uint8_t *>(&st[i]);

            v[0] = t & 0xFF;

            v[1] = (t >> 8) & 0xFF;

            v[2] = (t >> 16) & 0xFF;

            v[3] = (t >> 24) & 0xFF;

            v[4] = (t >> 32) & 0xFF;

            v[5] = (t >> 40) & 0xFF;

            v[6] = (t >> 48) & 0xFF;

            v[7] = (t >> 56) & 0xFF;
        }
#endif
    }

    static void shake_xof(sha_ctx_t *c)
    {
        c->state.b[c->pt] ^= 0x1f;

        c->state.b[c->rsiz - 1] ^= 0x80;

        keccakf(c->state.q);

        c->pt = 0;
    }

    static void shake_out(sha_ctx_t *c, void *out, size_t len)
    {
        int j = c->pt;

        for (size_t i = 0; i < len; i++)
        {
            if (j >= c->rsiz)
            {
                keccakf(c->state.q);

                j = 0;
            }

            reinterpret_cast<uint8_t *>(out)[0] = c->state.b[j++];
        }

        c->pt = j;
    }

    static int init(sha_ctx_t *c, int message_digest_length)
    {
        for (int i = 0; i < 25; i++)
            c->state.q[i] = 0;

        c->message_digest_length = message_digest_length;

        c->rsiz = 200 - 2 * message_digest_length;

        c->pt = 0;

        return 1;
    }

    static int update(sha_ctx_t *c, const void *input, size_t len)
    {
        int j = c->pt;

        for (size_t i = 0; i < len; i++)
        {
            c->state.b[j++] ^= reinterpret_cast<const uint8_t *>(input)[i];

            if (j >= c->rsiz)
            {
                keccakf(c->state.q);

                j = 0;
            }
        }


        c->pt = j;

        return 1;
    }

    static int final(void *message_digest, sha_ctx_t *c)
    {
        c->state.b[c->pt] ^= 0x06;

        c->state.b[c->rsiz - 1] ^= 0x80;

        keccakf(c->state.q);

        for (int i = 0; i < c->message_digest_length; i++)
            reinterpret_cast<uint8_t *>(message_digest)[i] = c->state.b[i];

        return 1;
    }

    void hash(const void *input, const size_t input_length, void *message_digest, const int message_digest_bits)
    {
        sha_ctx_t sha3;

        if (message_digest_bits % 8 != 0)
            throw std::range_error("message bit length is not divisible by 8");

        const int message_digest_length = message_digest_bits / 8;

        init(&sha3, message_digest_length);

        update(&sha3, input, input_length);

        final(message_digest, &sha3);
    }
} // namespace SHA3
