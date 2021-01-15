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

#include "crypto_bp.h"

#include <chrono>
#include <iomanip>
#include <iostream>

#define PERFORMANCE_ITERATIONS 1000
#define PERFORMANCE_ITERATIONS_LONG_MULTIPLIER 60
#define PERFORMANCE_ITERATIONS_LONG PERFORMANCE_ITERATIONS *PERFORMANCE_ITERATIONS_LONG_MULTIPLIER
#define RING_SIZE 4

const crypto_hash_t INPUT_DATA = {0xcf, 0xc7, 0x65, 0xd9, 0x05, 0xc6, 0x5e, 0x2b, 0x61, 0x81, 0x6d,
                                  0xc1, 0xf0, 0xfd, 0x69, 0xf6, 0xf6, 0x77, 0x9f, 0x36, 0xed, 0x62,
                                  0x39, 0xac, 0x7e, 0x21, 0xff, 0x51, 0xef, 0x2c, 0x89, 0x1e};

const crypto_hash_t SHA3_HASH = {0x97, 0x45, 0x06, 0x60, 0x1a, 0x60, 0xdc, 0x46, 0x5e, 0x6e, 0x9a,
                                 0xcd, 0xdb, 0x56, 0x38, 0x89, 0xe6, 0x34, 0x71, 0x84, 0x9e, 0xc4,
                                 0x19, 0x86, 0x56, 0x55, 0x03, 0x54, 0xb8, 0x54, 0x1f, 0xcb};

const auto SHA3_SLOW_0 = crypto_hash_t("974506601a60dc465e6e9acddb563889e63471849ec4198656550354b8541fcb");

const auto SHA3_SLOW_4096 = crypto_hash_t("c031be420e429992443c33c2a453287e2678e70b8bce95dfe7357bcbf36ca86c");

template<typename T>
void benchmark(T &&function, const std::string &functionName, uint64_t iterations = PERFORMANCE_ITERATIONS)
{
    std::cout << std::setw(70) << functionName << ": ";

    auto tenth = iterations / 10;

    auto startTimer = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < iterations; ++i)
    {
        if (i % tenth == 0)
            std::cout << ".";

        function();
    }

    auto elapsedTime =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTimer)
            .count();

    auto timePer = elapsedTime / iterations;

    std::cout << "  " << std::fixed << std::setprecision(3) << std::setw(8) << timePer / 1000.0 << " ms" << std::endl;
}

int main()
{
    std::cout << std::endl << std::endl << "Cryptographic Primitive Unit Tests" << std::endl << std::endl;

    // SHA-3 test
    {
        const auto hash = TurtleCoinCrypto::Hashing::sha3(INPUT_DATA);

        if (hash != SHA3_HASH)
        {
            std::cout << "Hashing::sha3: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Hashing::sha3: Passed!" << std::endl << std::endl;
    }

    // SHA-3 slow hash
    {
        auto hash = TurtleCoinCrypto::Hashing::sha3_slow_hash(INPUT_DATA);

        if (hash != SHA3_SLOW_0)
        {
            std::cout << "Hashing::sha3_slow_hash: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Hashing::sha3_slow_hash: Passed!" << std::endl << std::endl;

        hash = TurtleCoinCrypto::Hashing::sha3_slow_hash(INPUT_DATA, 4096);

        if (hash != SHA3_SLOW_4096)
        {
            std::cout << "Hashing::sha3_slow_hash[4096]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Hashing::sha3_slow_hash[4096]: Passed!" << std::endl << std::endl;
    }

    // 2^n rounding test
    {
        const auto val = TurtleCoinCrypto::pow2_round(13);

        if (val != 16)
        {
            std::cout << "pow2_round: Failed!" << std::endl;

            return 1;
        }

        std::cout << "pow2_round: Passed!" << std::endl;
    }

    // check tests
    {
        const auto scalar = std::string("a03681f038b1aee4d417874fa551aaa8f4a608a70ddff0257dd93f932b8fef0e");

        const auto point = std::string("d555bf22bce71d4eff27aa7597b5590969e7eccdb67a52188d0d73d5ab82d414");

        if (!TurtleCoinCrypto::check_scalar(scalar))
        {
            std::cout << "check_scalar: Failed! " << scalar << std::endl;

            return 1;
        }

        if (TurtleCoinCrypto::check_scalar(point))
        {
            std::cout << "check_scalar: Failed! " << point << std::endl;

            return 1;
        }

        std::cout << "check_scalar: Passed!" << std::endl;

        if (!TurtleCoinCrypto::check_point(point))
        {
            std::cout << "check_point: Failed! " << point << std::endl;

            return 1;
        }

        if (TurtleCoinCrypto::check_point(scalar))
        {
            std::cout << "check_point: Failed! " << scalar << std::endl;

            return 1;
        }

        std::cout << "check_point: Passed!" << std::endl;
    }

    // Scalar bit vector test
    {
        const auto a = TurtleCoinCrypto::random_scalar();

        const auto bits = a.to_bits();

        crypto_scalar_t b(bits);

        if (b != a)
        {
            std::cout << "Scalar Bit Vector Test: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Scalar Bit Vector Test: Passed!" << std::endl << std::endl;
    }

    const auto [public_key, secret_key] = TurtleCoinCrypto::generate_keys();

    std::cout << "S: " << secret_key << std::endl << "P: " << public_key << std::endl;

    {
        const auto check = TurtleCoinCrypto::secret_key_to_public_key(secret_key);

        if (check != public_key)
        {
            std::cout << "secret_key_to_public_key: Failed!" << std::endl;

            return 1;
        }

        std::cout << "secret_key_to_public_key: " << secret_key << std::endl << "\t -> " << public_key << std::endl;
    }

    // test subwallet-0
    {
        const auto [pub, subwallet] = TurtleCoinCrypto::generate_subwallet_keys(secret_key, 0);

        if (subwallet != secret_key)
        {
            std::cout << "generate_deterministic_subwallet_key(0): Failed!" << std::endl;

            return 1;
        }

        std::cout << "generate_deterministic_subwallet_key(0): " << subwallet << std::endl;
    }

    // test subwallet-32
    {
        const auto [pub, subwallet] = TurtleCoinCrypto::generate_subwallet_keys(secret_key, 32);

        if (subwallet == secret_key)
        {
            std::cout << "generate_deterministic_subwallet_key(32): Failed!" << std::endl;

            return 1;
        }

        std::cout << "generate_deterministic_subwallet_key(32): " << subwallet << std::endl;
    }

    const auto secret_key2 = TurtleCoinCrypto::generate_view_from_spend(secret_key);

    if (secret_key2 == secret_key)
    {
        std::cout << "generate_view_from_spend: Failed!" << std::endl;

        return 1;
    }

    std::cout << std::endl << "generate_view_from_spend: Passed!" << std::endl;

    const auto public_key2 = TurtleCoinCrypto::secret_key_to_public_key(secret_key2);

    std::cout << "S2: " << secret_key2 << std::endl << "P2: " << public_key2 << std::endl;

    // save these for later
    crypto_public_key_t public_ephemeral;

    crypto_secret_key_t secret_ephemeral;

    crypto_key_image_t key_image;

    {
        std::cout << std::endl << "Stealth Checks..." << std::endl;

        std::cout << std::endl << "Sender..." << std::endl;

        const auto derivation = TurtleCoinCrypto::generate_key_derivation(public_key2, secret_key);

        std::cout << "generate_key_derivation: " << derivation << std::endl;

        const auto derivation_scalar = TurtleCoinCrypto::derivation_to_scalar(derivation, 64);

        std::cout << "derivation_to_scalar: " << derivation_scalar << std::endl;

        const auto expected_public_ephemeral = TurtleCoinCrypto::derive_public_key(derivation_scalar, public_key2);

        std::cout << "derive_public_key: " << expected_public_ephemeral << std::endl;

        std::cout << std::endl << "Receiver..." << std::endl;

        const auto derivation2 = TurtleCoinCrypto::generate_key_derivation(public_key, secret_key2);

        std::cout << "generate_key_derivation: " << derivation2 << std::endl;

        const auto derivation_scalar2 = TurtleCoinCrypto::derivation_to_scalar(derivation2, 64);

        std::cout << "derivation_to_scalar: " << derivation_scalar2 << std::endl;

        public_ephemeral = TurtleCoinCrypto::derive_public_key(derivation_scalar2, public_key2);

        std::cout << "derive_public_key: " << public_ephemeral << std::endl;

        secret_ephemeral = TurtleCoinCrypto::derive_secret_key(derivation_scalar2, secret_key2);

        std::cout << "derive_secret_key: " << secret_ephemeral << std::endl;

        {
            const auto check = TurtleCoinCrypto::secret_key_to_public_key(secret_ephemeral);

            if (check != expected_public_ephemeral)
            {
                std::cout << "public_ephemeral does not match expected value" << std::endl;

                return 1;
            }
        }

        // check underive_public_key
        {
            const auto underived_public_key = TurtleCoinCrypto::underive_public_key(derivation, 64, public_ephemeral);

            std::cout << "underive_public_key: " << underived_public_key << std::endl;

            if (underived_public_key != public_key2)
            {
                std::cout << "underived_public_key does not match expected value" << std::endl;

                return 1;
            }
        }

        key_image = TurtleCoinCrypto::generate_key_image(public_ephemeral, secret_ephemeral);

        if (!key_image.check_subgroup())
        {
            std::cout << "Invalid Key Image!" << std::endl;

            return 1;
        }

        std::cout << "generate_key_image: " << key_image << std::endl;
    }

    // Single Signature
    {
        std::cout << std::endl << std::endl << "Message Signing" << std::endl;

        const auto signature = TurtleCoinCrypto::Signature::generate_signature(SHA3_HASH, secret_key);

        std::cout << "Signature::generate_signature: Passed!" << std::endl;

        if (!TurtleCoinCrypto::Signature::check_signature(SHA3_HASH, public_key, signature))
        {
            std::cout << "Signature::check_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Signature::check_signature: Passed!" << std::endl;
    }

    // Borromean
    {
        std::cout << std::endl << std::endl << "Borromean Ring Signatures" << std::endl;

        auto public_keys = TurtleCoinCrypto::random_points(RING_SIZE);

        public_keys[RING_SIZE / 2] = public_ephemeral;

        const auto [gen_success, signature] =
            TurtleCoinCrypto::RingSignature::Borromean::generate_ring_signature(SHA3_HASH, secret_ephemeral, public_keys);

        if (!gen_success)
        {
            std::cout << "Borromean::generate_ring_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Borromean::generate_ring_signature: " << std::endl;

        for (const auto sig : signature)
        {
            std::cout << "\t" << sig << std::endl;
        }

        std::cout << "\tSignature Size: " << (sizeof(crypto_signature_t) * signature.size()) << std::endl << std::endl;

        if (!TurtleCoinCrypto::RingSignature::Borromean::check_ring_signature(SHA3_HASH, key_image, public_keys, signature))
        {
            std::cout << "Borromean::check_ring_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "Borromean::check_ring_signature: Passed!" << std::endl;
    }

    // CLSAG
    {
        std::cout << std::endl << std::endl << "CLSAG Ring Signatures" << std::endl;

        auto public_keys = TurtleCoinCrypto::random_points(RING_SIZE);

        public_keys[RING_SIZE / 2] = public_ephemeral;

        const auto [gen_sucess, signature] =
            TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature(SHA3_HASH, secret_ephemeral, public_keys);

        if (!gen_sucess)
        {
            std::cout << "CLSAG::generate_ring_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "CLSAG::generate_ring_signature: Passed!" << std::endl;

        std::cout << signature << std::endl;

        std::cout << "Encoded Size: " << signature.size() << std::endl
                  << signature.to_string() << std::endl
                  << std::endl;

        if (!TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature(SHA3_HASH, key_image, public_keys, signature))
        {
            std::cout << "CLSAG::check_ring_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "CLSAG::check_ring_signature: Passed!" << std::endl;
    }

    // CLSAG w/ Commitments
    {
        std::cout << std::endl << std::endl << "CLSAG Ring Signatures w/ Commitments" << std::endl;

        auto public_keys = TurtleCoinCrypto::random_points(RING_SIZE);

        public_keys[RING_SIZE / 2] = public_ephemeral;

        const auto input_blinding = TurtleCoinCrypto::random_scalar();

        const auto input_commitment = TurtleCoinCrypto::RingCT::generate_pedersen_commitment(input_blinding, 100);

        std::vector<crypto_pedersen_commitment_t> public_commitments = TurtleCoinCrypto::random_points(RING_SIZE);

        public_commitments[RING_SIZE / 2] = input_commitment;

        const auto [ps_blindings, ps_commitments] =
            TurtleCoinCrypto::RingCT::generate_pseudo_commitments({100}, TurtleCoinCrypto::random_scalars(1));

        const auto [gen_sucess, signature] = TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature(
            SHA3_HASH,
            secret_ephemeral,
            public_keys,
            input_blinding,
            public_commitments,
            ps_blindings[0],
            ps_commitments[0]);

        if (!gen_sucess)
        {
            std::cout << "CLSAG::generate_ring_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "CLSAG::generate_ring_signature: Passed!" << std::endl;

        std::cout << signature << std::endl;

        std::cout << "Encoded Size: " << signature.size() << std::endl
                  << signature.to_string() << std::endl
                  << std::endl;

        if (!TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature(
                SHA3_HASH, key_image, public_keys, signature, public_commitments, ps_commitments[0]))
        {
            std::cout << "CLSAG::check_ring_signature: Failed!" << std::endl;

            return 1;
        }

        std::cout << "CLSAG::check_ring_signature: Passed!" << std::endl;
    }

    // RingCT Basics
    {
        std::cout << std::endl << std::endl << "RingCT" << std::endl;

        /**
         * Generate two random scalars, and then feed them to our blinding factor
         * generator -- normally these are computed based on the derivation scalar
         * calculated for the destination one-time key
         */
        auto blinding_factors = TurtleCoinCrypto::random_scalars(2);

        for (auto &factor : blinding_factors)
            factor = TurtleCoinCrypto::RingCT::generate_commitment_blinding_factor(factor);

        /**
         * Generate two fake output commitments using the blinding factors calculated above
         */
        const auto C_1 = TurtleCoinCrypto::RingCT::generate_pedersen_commitment(blinding_factors[0], 1000);

        const auto C_2 = TurtleCoinCrypto::RingCT::generate_pedersen_commitment(blinding_factors[1], 1000);

        // Generate the pedersen commitment for the transaction fee with a ZERO blinding factor
        const auto C_fee = TurtleCoinCrypto::RingCT::generate_pedersen_commitment({0}, 100);

        std::cout << "RingCT::generate_pedersen_commitment:" << std::endl
                  << "\t" << C_1 << std::endl
                  << "\t" << C_2 << std::endl
                  << "\t" << C_fee << std::endl;

        /**
         * Add up the commitments of the "real" output commitments plus
         * the commitment to the transaction fee
         */
        const auto CT = C_1 + C_2 + C_fee;

        /**
         * Generate the pseudo output commitments and blinding factors
         */
        const auto [pseudo_blinding_factors, pseudo_commitments] =
            TurtleCoinCrypto::RingCT::generate_pseudo_commitments({2000, 100}, blinding_factors);

        std::cout << std::endl << "RingCT::generate_pseudo_commitments:" << std::endl;

        for (const auto &commitment : pseudo_commitments)
            std::cout << "\t" << commitment << std::endl;

        std::cout << std::endl;

        // Add all of the pseudo commitments together
        const auto PT = crypto_point_vector_t(pseudo_commitments).sum();

        // And check that they match the total from the "real" output commitments
        if (PT != CT)
        {
            std::cout << "RingCT::generate_pseudo_commitments: Failed!" << std::endl;

            return 1;
        }

        std::cout << "RingCT::generate_pseudo_commitments: Passed!" << std::endl;

        if (!TurtleCoinCrypto::RingCT::check_commitments_parity(pseudo_commitments, {C_1, C_2}, 100))
        {
            std::cout << "RingCT::check_commitments_parity: Failed!" << std::endl;

            return 1;
        }

        std::cout << "RingCT::check_commitments_parity: Passed!" << std::endl;

        const auto derivation_scalar = TurtleCoinCrypto::random_scalar();

        // amount masking (hiding)
        {
            const auto amount_mask = TurtleCoinCrypto::RingCT::generate_amount_mask(derivation_scalar);

            const crypto_scalar_t amount = 13371337;

            const auto masked_amount = TurtleCoinCrypto::RingCT::toggle_masked_amount(amount_mask, amount);

            const auto unmasked_amount = TurtleCoinCrypto::RingCT::toggle_masked_amount(amount_mask, masked_amount);

            if (masked_amount.to_uint64_t() == amount.to_uint64_t()
                || unmasked_amount.to_uint64_t() != amount.to_uint64_t())
            {
                std::cout << "RingCT::toggle_masked_amount: Failed!" << std::endl;

                return 1;
            }

            std::cout << "RingCT::toggle_masked_amount: Passed!" << std::endl;
        }
    }

    // Bulletproofs
    {
        std::cout << std::endl << std::endl << "Bulletproofs" << std::endl;

        auto [proof, commitments] = TurtleCoinCrypto::RangeProofs::Bulletproofs::prove({1000}, TurtleCoinCrypto::random_scalars(1));

        if (!TurtleCoinCrypto::RangeProofs::Bulletproofs::verify({proof}, {commitments}))
        {
            std::cout << "TurtleCoinCrypto::RangeProofs::Bulletproofs[1]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "TurtleCoinCrypto::RangeProofs::Bulletproofs[1]: Passed!" << std::endl;

        std::cout << proof << std::endl;

        std::cout << "Encoded Size: " << proof.size() << std::endl << proof.to_string() << std::endl << std::endl;

        proof.taux *= TurtleCoinCrypto::TWO;

        if (TurtleCoinCrypto::RangeProofs::Bulletproofs::verify({proof}, {commitments}))
        {
            std::cout << "TurtleCoinCrypto::RangeProofs::Bulletproofs[2]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "TurtleCoinCrypto::RangeProofs::Bulletproofs[2]: Passed!" << std::endl;

        // verify that value out of range fails proof
        auto [proof2, commitments2] = TurtleCoinCrypto::RangeProofs::Bulletproofs::prove({1000}, TurtleCoinCrypto::random_scalars(1), 8);

        if (TurtleCoinCrypto::RangeProofs::Bulletproofs::verify({proof2}, {commitments2}, 8))
        {
            std::cout << "TurtleCoinCrypto::RangeProofs::Bulletproofs[3]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "TurtleCoinCrypto::RangeProofs::Bulletproofs[3]: Passed!" << std::endl;
    }

    // Bulletproofs+
    {
        std::cout << std::endl << std::endl << "Bulletproofs+" << std::endl;

        auto [proof, commitments] = TurtleCoinCrypto::RangeProofs::BulletproofsPlus::prove({1000}, TurtleCoinCrypto::random_scalars(1));

        if (!TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify({proof}, {commitments}))
        {
            std::cout << "TurtleCoinCrypto::RangeProofs::BulletproofsPlus[1]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "TurtleCoinCrypto::RangeProofs::BulletproofsPlus[1]: Passed!" << std::endl;

        std::cout << proof << std::endl;

        std::cout << "Encoded Size: " << proof.size() << std::endl << proof.to_string() << std::endl << std::endl;

        proof.d1 *= TurtleCoinCrypto::TWO;

        if (TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify({proof}, {commitments}))
        {
            std::cout << "TurtleCoinCrypto::RangeProofs::BulletproofsPlus[2]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "TurtleCoinCrypto::RangeProofs::BulletproofsPlus[2]: Passed!" << std::endl;

        // verify that value out of range fails proof
        auto [proof2, commitments2] =
            TurtleCoinCrypto::RangeProofs::BulletproofsPlus::prove({1000}, TurtleCoinCrypto::random_scalars(1), 8);

        if (TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify({proof2}, {commitments2}, 8))
        {
            std::cout << "TurtleCoinCrypto::RangeProofs::BulletproofsPlus[3]: Failed!" << std::endl;

            return 1;
        }

        std::cout << "TurtleCoinCrypto::RangeProofs::BulletproofsPlus[3]: Passed!" << std::endl;
    }

    // Benchmarks
    {
        std::cout << std::endl << std::endl << std::endl << "Operation Benchmarks" << std::endl << std::endl;

        const auto [point, scalar] = TurtleCoinCrypto::generate_keys();
        const auto ds = TurtleCoinCrypto::derivation_to_scalar(point, 64);
        const auto key_image = TurtleCoinCrypto::generate_key_image(point, scalar);

        benchmark([]() { TurtleCoinCrypto::Hashing::sha3(INPUT_DATA); }, "TurtleCoinCrypto::Hashing::sha3", PERFORMANCE_ITERATIONS_LONG);

        benchmark(
            [&point = point, &scalar = scalar]() { TurtleCoinCrypto::generate_key_derivation(point, scalar); },
            "TurtleCoinCrypto::generate_key_derivation");

        benchmark([&ds, &point = point]() { TurtleCoinCrypto::derive_public_key(ds, point); }, "TurtleCoinCrypto::derive_public_key");

        benchmark([&ds, &scalar = scalar]() { TurtleCoinCrypto::derive_secret_key(ds, scalar); }, "TurtleCoinCrypto::derive_secret_key");

        benchmark([&point = point]() { TurtleCoinCrypto::underive_public_key(point, 64, point); }, "TurtleCoinCrypto::underive_public_key");

        benchmark(
            [&point = point, &scalar = scalar]() { TurtleCoinCrypto::generate_key_image(point, scalar); },
            "TurtleCoinCrypto::generate_key_image");

        benchmark([&key_image]() { key_image.check_subgroup(); }, "crypto_point_t::check_subgroup()");

        // signing
        {
            crypto_signature_t sig;

            std::cout << std::endl;

            benchmark(
                [&sig, &scalar = scalar]() { sig = TurtleCoinCrypto::Signature::generate_signature(SHA3_HASH, scalar); },
                "TurtleCoinCrypto::Signature::generate_signature");

            benchmark(
                [&sig, &point = point]() { TurtleCoinCrypto::Signature::check_signature(SHA3_HASH, point, sig); },
                "TurtleCoinCrypto::Signature::check_signature");
        }

        // Borromean
        {
            auto public_keys = TurtleCoinCrypto::random_points(RING_SIZE);

            public_keys[RING_SIZE / 2] = public_ephemeral;

            std::vector<crypto_signature_t> signature;

            const auto image = TurtleCoinCrypto::generate_key_image(public_ephemeral, secret_ephemeral);

            std::cout << std::endl;

            benchmark(
                [&public_keys, &secret_ephemeral, &signature]() {
                    const auto [succes, sigs] = TurtleCoinCrypto::RingSignature::Borromean::generate_ring_signature(
                        SHA3_HASH, secret_ephemeral, public_keys);
                    signature = sigs;
                },
                "TurtleCoinCrypto::RingSignature::Borromean::generate_ring_signature",
                100);

            benchmark(
                [&public_keys, &image, &signature]() {
                    TurtleCoinCrypto::RingSignature::Borromean::check_ring_signature(SHA3_HASH, image, public_keys, signature);
                },
                "TurtleCoinCrypto::RingSignature::Borromean::check_ring_signature",
                100);
        }

        // CLSAG
        {
            auto public_keys = TurtleCoinCrypto::random_points(RING_SIZE);

            public_keys[RING_SIZE / 2] = public_ephemeral;

            crypto_clsag_signature_t signature;

            const auto image = TurtleCoinCrypto::generate_key_image(public_ephemeral, secret_ephemeral);

            std::cout << std::endl;

            benchmark(
                [&public_keys, &secret_ephemeral, &signature]() {
                    const auto [success, sig] =
                        TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature(SHA3_HASH, secret_ephemeral, public_keys);
                    signature = sig;
                },
                "TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature",
                100);

            benchmark(
                [&public_keys, &image, &signature]() {
                    TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature(SHA3_HASH, image, public_keys, signature);
                },
                "TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature",
                100);
        }

        // CLSAG w/ Commitments
        {
            auto public_keys = TurtleCoinCrypto::random_points(RING_SIZE);

            public_keys[RING_SIZE / 2] = public_ephemeral;

            crypto_clsag_signature_t signature;

            const auto image = TurtleCoinCrypto::generate_key_image(public_ephemeral, secret_ephemeral);

            const auto input_blinding = TurtleCoinCrypto::random_scalar();

            const auto input_commitment = TurtleCoinCrypto::RingCT::generate_pedersen_commitment(input_blinding, 100);

            std::vector<crypto_pedersen_commitment_t> public_commitments = TurtleCoinCrypto::random_points(RING_SIZE);

            public_commitments[RING_SIZE / 2] = input_commitment;

            const auto [ps_blindings, ps_commitments] =
                TurtleCoinCrypto::RingCT::generate_pseudo_commitments({100}, TurtleCoinCrypto::random_scalars(1));

            std::cout << std::endl;

            benchmark(
                [&public_keys,
                 &secret_ephemeral,
                 &signature,
                 &input_blinding,
                 &public_commitments,
                 &ps_blindings = ps_blindings,
                 &ps_commitments = ps_commitments]() {
                    const auto [success, sig] = TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature(
                        SHA3_HASH,
                        secret_ephemeral,
                        public_keys,
                        input_blinding,
                        public_commitments,
                        ps_blindings[0],
                        ps_commitments[0]);
                    signature = sig;
                },
                "TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature[commitments]",
                100);

            benchmark(
                [&public_keys, &image, &signature, &public_commitments, &ps_commitments = ps_commitments]() {
                    TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature(
                        SHA3_HASH, image, public_keys, signature, public_commitments, ps_commitments[0]);
                },
                "TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature[commitments]",
                100);
        }

        // RingCT
        {
            const auto blinding_factor = TurtleCoinCrypto::random_scalar();

            std::cout << std::endl;

            benchmark(
                [&blinding_factor]() { TurtleCoinCrypto::RingCT::generate_pedersen_commitment(blinding_factor, 10000); },
                "TurtleCoinCrypto::RingCT::generate_pedersen_commitment");

            benchmark(
                [&blinding_factor]() { TurtleCoinCrypto::RingCT::generate_pseudo_commitments({10000}, {blinding_factor}); },
                "TurtleCoinCrypto::RingCT::generate_pseudo_commitments");
        }

        // Bulletproofs
        {
            const auto blinding_factors = TurtleCoinCrypto::random_scalars(1);

            // seed the memory cache as to not taint the benchmark
            const auto [p, c] = TurtleCoinCrypto::RangeProofs::Bulletproofs::prove({1000}, blinding_factors);

            crypto_bulletproof_t proof;

            std::vector<crypto_pedersen_commitment_t> commitments;

            std::cout << std::endl;

            benchmark(
                [&proof, &blinding_factors, &commitments]() {
                    const auto [p, c] = TurtleCoinCrypto::RangeProofs::Bulletproofs::prove({1000}, blinding_factors);
                    proof = p;
                    commitments = c;
                },
                "TurtleCoinCrypto::RangeProofs::Bulletproofs::prove",
                10);

            benchmark(
                [&proof, &commitments]() { TurtleCoinCrypto::RangeProofs::Bulletproofs::verify({proof}, {commitments}); },
                "TurtleCoinCrypto::RangeProofs::Bulletproofs::verify",
                10);

            benchmark(
                [&proof, &commitments]() {
                    TurtleCoinCrypto::RangeProofs::Bulletproofs::verify({proof, proof}, {commitments, commitments});
                },
                "TurtleCoinCrypto::RangeProofs::Bulletproofs::verify[batched]",
                10);
        }

        // Bulletproofs+
        {
            const auto blinding_factors = TurtleCoinCrypto::random_scalars(1);

            // seed the memory cache as to not taint the benchmark
            const auto [p, c] = TurtleCoinCrypto::RangeProofs::BulletproofsPlus::prove({1000}, blinding_factors);

            crypto_bulletproof_plus_t proof;

            std::vector<crypto_pedersen_commitment_t> commitments;

            std::cout << std::endl;

            benchmark(
                [&proof, &blinding_factors, &commitments]() {
                    const auto [p, c] = TurtleCoinCrypto::RangeProofs::BulletproofsPlus::prove({1000}, blinding_factors);
                    proof = p;
                    commitments = c;
                },
                "TurtleCoinCrypto::RangeProofs::BulletproofsPlus::prove",
                10);

            benchmark(
                [&proof, &commitments]() { TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify({proof}, {commitments}); },
                "TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify",
                10);

            benchmark(
                [&proof, &commitments]() {
                    TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify({proof, proof}, {commitments, commitments});
                },
                "TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify[batched]",
                10);
        }
    }
}
