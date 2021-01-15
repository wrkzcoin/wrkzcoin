// Copyright (c) 2020, Brandon Lehmann <brandonlehmann@gmail.com>
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CRYPTO_SCALAR_TRANSCRIPT_H
#define CRYPTO_SCALAR_TRANSCRIPT_H

#include "crypto_common.h"
#include "hashing.h"

static const crypto_scalar_t TRANSCRIPT_BASE = {0x20, 0x20, 0x20, 0x20, 0x69, 0x62, 0x75, 0x72, 0x6e, 0x6d, 0x79,
                                                0x63, 0x64, 0x40, 0x74, 0x75, 0x72, 0x74, 0x6c, 0x65, 0x63, 0x6f,
                                                0x69, 0x6e, 0x2e, 0x64, 0x65, 0x76, 0x20, 0x20, 0x20, 0x20};

namespace Crypto
{
    /**
     * Structure provides a transcript for hashing arbitrary values in a determinisic way
     * that can be used for constructing challenge scalars during commitments
     */
    typedef struct ScalarTranscript
    {
      public:
        ScalarTranscript() {}

        template<typename T> ScalarTranscript(const T &seed)
        {
            update(seed);
        }

        template<typename T, typename U> ScalarTranscript(const T &seed, const U &seed2)
        {
            update(seed, seed2);
        }

        template<typename T, typename U, typename V> ScalarTranscript(const T &seed, const U &seed2, const V &seed3)
        {
            update(seed, seed2, seed3);
        }

        template<typename T, typename U, typename V, typename W>
        ScalarTranscript(const T &seed, const U &seed2, const V &seed3, const W &seed4)
        {
            update(seed, seed2, seed3, seed4);
        }

        template<typename T, typename U, typename V>
        ScalarTranscript(const T &seed, const U &seed2, const std::vector<V> &seed3)
        {
            update(seed3, seed, seed2);
        }

        crypto_scalar_t challenge()
        {
            return state;
        }

        void reset()
        {
            state = TRANSCRIPT_BASE;
        }

        template<typename T> void update(const T &input)
        {
            struct
            {
                crypto_scalar_t seed;
                uint8_t input[32] = {0};
            } buf;

            buf.seed = state;

            std::memmove(buf.input, input.data(), sizeof(buf.input));

            state = Crypto::hash_to_scalar(&buf, sizeof(buf));
        }

        template<typename T, typename U> void update(const T &input, const U &input2)
        {
            struct
            {
                crypto_scalar_t seed;
                uint8_t input[32] = {0};
                uint8_t input2[32] = {0};
            } buf;

            buf.seed = state;

            std::memmove(buf.input, input.data(), sizeof(buf.input));

            std::memmove(buf.input2, input2.data(), sizeof(buf.input2));

            state = Crypto::hash_to_scalar(&buf, sizeof(buf));
        }

        template<typename T, typename U, typename V> void update(const T &input, const U &input2, const V &input3)
        {
            struct
            {
                crypto_scalar_t seed;
                uint8_t input[32] = {0};
                uint8_t input2[32] = {0};
                uint8_t input3[32] = {0};
            } buf;

            buf.seed = state;

            std::memmove(buf.input, input.data(), sizeof(buf.input));

            std::memmove(buf.input2, input2.data(), sizeof(buf.input2));

            std::memmove(buf.input3, input3.data(), sizeof(buf.input3));

            state = Crypto::hash_to_scalar(&buf, sizeof(buf));
        }

        template<typename T, typename U, typename V, typename W>
        void update(const T &input, const U &input2, const V &input3, const W &input4)
        {
            struct
            {
                crypto_scalar_t seed;
                uint8_t input[32] = {0};
                uint8_t input2[32] = {0};
                uint8_t input3[32] = {0};
                uint8_t input4[32] = {0};
            } buf;

            buf.seed = state;

            std::memmove(buf.input, input.data(), sizeof(buf.input));

            std::memmove(buf.input2, input2.data(), sizeof(buf.input2));

            std::memmove(buf.input3, input3.data(), sizeof(buf.input3));

            std::memmove(buf.input4, input4.data(), sizeof(buf.input4));

            state = Crypto::hash_to_scalar(&buf, sizeof(buf));
        }

        void update(const std::vector<crypto_scalar_t> &input)
        {
            std::vector<crypto_scalar_t> tmp(1, state.data());

            tmp.resize(input.size() + 1);

            std::copy(input.begin(), input.end(), tmp.begin() + 1);

            state = Crypto::hash_to_scalar(tmp.data(), tmp.size() * sizeof(crypto_scalar_t));
        }

        void update(const std::vector<crypto_point_t> &input)
        {
            std::vector<crypto_scalar_t> tmp(1, state.data());

            for (const auto &point : input)
                tmp.push_back(PointToScalar(point));

            state = Crypto::hash_to_scalar(tmp.data(), tmp.size() * sizeof(crypto_scalar_t));
        }

      private:
        // default seed state for scalar transcripts
        crypto_scalar_t state = TRANSCRIPT_BASE;
    } crypto_scalar_transcript_t;
} // namespace Crypto

#endif // CRYPTO_SCALAR_TRANSCRIPT_H
