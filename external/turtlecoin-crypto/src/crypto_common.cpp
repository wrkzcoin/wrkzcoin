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

#include "crypto.h"

static const crypto_scalar_t DERIVATION_DOMAIN_0 = {0x79, 0x6f, 0x75, 0x20, 0x66, 0x75, 0x6e, 0x64, 0x73, 0x20, 0x61,
                                                    0x72, 0x65, 0x20, 0x69, 0x6e, 0x73, 0x69, 0x64, 0x65, 0x20, 0x74,
                                                    0x68, 0x69, 0x73, 0x20, 0x62, 0x6f, 0x78, 0x20, 0x20, 0x20};

static const crypto_scalar_t SUBWALLET_DOMAIN_0 = Crypto::hash_to_scalar(DERIVATION_DOMAIN_0);

static const crypto_scalar_t VIEWKEY_DOMAIN_0 = Crypto::hash_to_scalar(SUBWALLET_DOMAIN_0);

namespace Crypto
{
    crypto_scalar_t derivation_to_scalar(const crypto_derivation_t &derivation, uint64_t output_index)
    {
        struct
        {
            crypto_scalar_t domain;
            uint8_t derivation[32] = {0};
            uint64_t output_index = 0;
        } buffer;

        buffer.domain = DERIVATION_DOMAIN_0;

        std::memmove(buffer.derivation, derivation.data(), derivation.size());

        buffer.output_index = output_index;

        // Ds = [H(D || n)] mod l
        return hash_to_scalar(&buffer, sizeof(buffer));
    }

    crypto_public_key_t
        derive_public_key(const crypto_scalar_t &derivation_scalar, const crypto_public_key_t &public_key)
    {
        // P = [A + (Ds * G)] mod l
        return (derivation_scalar * G) + public_key;
    }

    crypto_secret_key_t
        derive_secret_key(const crypto_scalar_t &derivation_scalar, const crypto_secret_key_t &secret_key)
    {
        // p = (Ds + a) mod l
        return derivation_scalar + secret_key;
    }

    crypto_derivation_t
        generate_key_derivation(const crypto_public_key_t &public_key, const crypto_secret_key_t &secret_key)
    {
        // D = (a * B) mod l
        return (secret_key * public_key).mul8();
    }

    crypto_key_image_t
        generate_key_image(const crypto_public_key_t &public_ephemeral, const crypto_secret_key_t &secret_ephemeral)
    {
        // I = [Hp(P) * x] mod l
        return secret_ephemeral * hash_to_point(public_ephemeral);
    }

    crypto_key_image_t generate_key_image(
        const crypto_public_key_t &public_ephemeral,
        const crypto_scalar_t &derivation_scalar,
        const std::vector<crypto_key_image_t> &partial_key_images)
    {
        crypto_point_vector_t key_images(partial_key_images);

        // I_d = (Ds * P) mod l
        const auto base_key_image = generate_key_image(public_ephemeral, derivation_scalar);

        key_images.append(base_key_image);

        // I = [I_d + (pk1 + pk2 + pk3 ...)] mod l
        return key_images.dedupe_sort().sum();
    }

    std::tuple<crypto_public_key_t, crypto_secret_key_t> generate_keys()
    {
        crypto_secret_key_t secret_key = random_scalar();

        // A = (a * G) mod l
        return {secret_key * G, secret_key};
    }

    std::tuple<crypto_public_key_t, crypto_secret_key_t>
        generate_subwallet_keys(const crypto_secret_key_t &spend_secret_key, uint64_t subwallet_index)
    {
        if (subwallet_index == 0)
            return {spend_secret_key * G, spend_secret_key};

        struct
        {
            crypto_scalar_t domain;
            crypto_secret_key_t base;
            uint64_t idx = 0;
        } buffer;

        buffer.domain = SUBWALLET_DOMAIN_0;

        buffer.base = spend_secret_key;

        buffer.idx = subwallet_index;

        const auto secret_key = hash_to_scalar(&buffer, sizeof(buffer));

        // A = (a * G) mod l
        return {secret_key * G, secret_key};
    }

    crypto_secret_key_t generate_view_from_spend(const crypto_secret_key_t &spend_secret_key)
    {
        struct
        {
            crypto_scalar_t domain;
            crypto_secret_key_t base;
        } buffer;

        buffer.domain = VIEWKEY_DOMAIN_0;

        buffer.base = spend_secret_key;

        return hash_to_scalar(&buffer, sizeof(buffer));
    }

    crypto_point_t hash_to_point(const void *data, size_t length)
    {
        // hash the data to a scalar
        const auto scalar = hash_to_scalar(data, length);

        // multiply it by the base point and then group of 8 it
        return (scalar * G).mul8();
    }

    crypto_scalar_t hash_to_scalar(const void *data, size_t length)
    {
        return crypto_scalar_t(Crypto::Hashing::sha3(data, length).bytes, true);
    }

    size_t pow2_round(size_t value)
    {
        size_t count = 0;

        if (value && !(value & (value - 1)))
            return value;

        while (value != 0)
        {
            value >>= uint64_t(1);

            count++;
        }

        return uint64_t(1) << count;
    }

    crypto_point_t random_point()
    {
        uint8_t bytes[32];

        // Retreive some random bytes
        Random::random_bytes(sizeof(bytes), bytes);

        return hash_to_point(bytes, sizeof(bytes));
    }

    std::vector<crypto_point_t> random_points(size_t count)
    {
        std::vector<crypto_point_t> result(count);

        for (size_t i = 0; i < count; ++i)
            result[i] = random_point();

        return result;
    }

    crypto_scalar_t random_scalar()
    {
        uint8_t bytes[32];

        // Retrieve some random bytes
        Random::random_bytes(sizeof(bytes), bytes);

        // hash it and return it as a scalar
        return hash_to_scalar(bytes, sizeof(bytes));
    }

    std::vector<crypto_scalar_t> random_scalars(size_t count)
    {
        std::vector<crypto_scalar_t> result(count);

        for (size_t i = 0; i < count; ++i)
            result[i] = random_scalar();

        return result;
    }

    crypto_public_key_t secret_key_to_public_key(const crypto_secret_key_t &secret_key)
    {
        // A = (a * G) mod l
        return secret_key * G;
    }

    crypto_public_key_t underive_public_key(
        const crypto_derivation_t &derivation,
        uint8_t output_index,
        const crypto_public_key_t &public_ephemeral)
    {
        const auto scalar = derivation_to_scalar(derivation, output_index);

        // A = [P - (Ds * G)] mod l
        return public_ephemeral - (scalar * G);
    }
} // namespace Crypto
