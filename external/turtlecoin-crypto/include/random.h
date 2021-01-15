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
//
// Adapted from code by Zpalmtree found at
// https://github.com/turtlecoin/turtlecoin/blob/26e395fadcaa1326dab595431526db3899e5df6e/src/crypto/random.h

#ifndef CRYPTO_RANDOM_H
#define CRYPTO_RANDOM_H

#include <random>

namespace Random
{
    /* Used to obtain a random seed */
    static thread_local std::random_device device;

    /* Generator, seeded with the random device */
    static thread_local std::mt19937 gen(device());

    /* The distribution to get numbers for - in this case, uint8_t */
    static std::uniform_int_distribution<int> distribution {0, 255};

    /**
     * Generate n random bytes (uint8_t), and place them in *result. Result should be large
     * enough to contain the bytes.
     */
    inline void random_bytes(size_t n, uint8_t *result)
    {
        for (size_t i = 0; i < n; i++)
            result[i] = distribution(gen);
    }
} // namespace Random

#endif // CRYPTO_RANDOM_H
