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
// Adapted from Python code by Sarang Noether found at
// https://github.com/SarangNoether/skunkworks/tree/clsag

#include "ring_signature_clsag.h"

/**
 * Separate hash domains are used at different points in the construction and verification
 * processes to avoid scalar collisions in different stages of the construction and verification
 * TLDR; these are hash salts
 */
static const crypto_scalar_t CLSAG_DOMAIN_0 = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x44,
                                               0x6f, 0x6e, 0x27, 0x74, 0x20, 0x50, 0x61, 0x6e, 0x69, 0x63, 0x2e,
                                               0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};

static const crypto_scalar_t CLSAG_DOMAIN_1 = TurtleCoinCrypto::hash_to_scalar(CLSAG_DOMAIN_0);

static const crypto_scalar_t CLSAG_DOMAIN_2 = TurtleCoinCrypto::hash_to_scalar(CLSAG_DOMAIN_1);

namespace TurtleCoinCrypto::RingSignature::CLSAG
{
    bool check_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_key_image_t &key_image,
        const std::vector<crypto_public_key_t> &public_keys,
        const crypto_clsag_signature_t &signature,
        const std::vector<crypto_pedersen_commitment_t> &commitments,
        const crypto_pedersen_commitment_t &pseudo_commitment)
    {
        const auto use_commitments =
            (signature.commitment_image != TurtleCoinCrypto::Z && commitments.size() == public_keys.size()
             && pseudo_commitment != TurtleCoinCrypto::Z);

        const auto ring_size = public_keys.size();

        if (signature.scalars.size() < ring_size)
            return false;

        if (!key_image.check_subgroup())
            return false;

        if (use_commitments && !signature.commitment_image.check_subgroup())
            return false;

        const auto h0 = signature.challenge;

        // the computational hash vector is only as big as our ring (not including the check hash)
        std::vector<crypto_scalar_t> h(ring_size);

        crypto_scalar_t mu_P, mu_C;

        // generate mu_P
        {
            crypto_scalar_transcript_t transcript(CLSAG_DOMAIN_0, key_image);

            transcript.update(public_keys);

            if (use_commitments)
            {
                transcript.update(signature.commitment_image);

                transcript.update(commitments);

                transcript.update(pseudo_commitment);
            }

            mu_P = transcript.challenge();
        }

        // generate mu_C
        if (use_commitments)
        {
            crypto_scalar_transcript_t transcript(CLSAG_DOMAIN_2, key_image);

            transcript.update(public_keys);

            transcript.update(signature.commitment_image);

            transcript.update(commitments);

            transcript.update(pseudo_commitment);

            mu_C = transcript.challenge();
        }

        /**
         * This transcript is the same for each round so re-computing the state of the
         * transcript for each round is a waste of processing power, instead we'll
         * preload this information and make a copy of the state before we use it
         * for each round's computation
         */
        crypto_scalar_transcript_t transcript(CLSAG_DOMAIN_1, message_digest);

        transcript.update(public_keys);

        if (use_commitments)
        {
            transcript.update(commitments);

            transcript.update(pseudo_commitment);
        }

        for (size_t i = 0; i < ring_size; i++)
        {
            auto temp_h = h[i];

            if (i == 0)
                temp_h = h0;

            const auto idx = i % ring_size;

            // r = (temp_h * mu_P) mod l
            const auto r = temp_h * mu_P;

            // L = [(r * P[idx]) + (s[idx] * G)] mod l
            auto L = (r * public_keys[idx]) + (signature.scalars[idx] * G);

            // HP = [Hp(P[idx])] mod l
            const auto HP = TurtleCoinCrypto::hash_to_point(public_keys[idx]);

            // R = [(s[idx] * HP) + (r * I)] mod l
            auto R = (signature.scalars[idx] * HP) + (r * key_image);

            if (use_commitments)
            {
                // r2 = (temp_h * mu_C) mod l
                const auto r2 = temp_h * mu_C;

                /**
                 * Here we're calculating the offset commitments based upon the input
                 * commitments minus the pseudo commitment that was provided thus
                 * allowing us to verify their signers knowledge of z (the delta between the
                 * input blinding scalar and the pseudo blinding scalar) while committing
                 * to a "zero" amount difference between the two commitments
                 */
                // C = (C[idx] - PS) mod l
                const auto C = commitments[idx] - pseudo_commitment;

                // L += [r2 * (C[idx] - PS)] mod l
                L += (r2 * C);

                // R += (r2 * D) mod l
                R += (r2 * signature.commitment_image);
            }

            auto sub_transcript = transcript;

            sub_transcript.update(L, R);

            h[(i + 1) % ring_size] = sub_transcript.challenge();
        }

        return h[0] == h0;
    }

    crypto_scalar_t
        generate_partial_signing_scalar(const crypto_scalar_t &mu_P, const crypto_secret_key_t &spend_secret_key)
    {
        return mu_P * spend_secret_key;
    }

    std::tuple<bool, crypto_clsag_signature_t> complete_ring_signature(
        const crypto_scalar_t &signing_scalar,
        size_t real_output_index,
        const crypto_clsag_signature_t &signature,
        const std::vector<crypto_scalar_t> &h,
        const crypto_scalar_t &mu_P,
        const std::vector<crypto_scalar_t> &partial_signing_scalars)
    {
        if (signature.scalars.empty() || real_output_index >= signature.scalars.size()
            || h.size() != signature.scalars.size())
            return {false, {}};

        std::vector<crypto_scalar_t> finalized_signature(signature.scalars);

        /**
         * If we have the full signing scalar (secret_ephemeral) then we can complete the signature quickly
         */
        if (partial_signing_scalars.empty())
        {
            // s = [alpha - (h[real_output_index] * (p * mu_P))] mod l
            finalized_signature[real_output_index] -= (h[real_output_index] * signing_scalar * mu_P);
        }
        else /** Otherwise, we're using partial signing scalars (multisig) */
        {
            const auto partial_scalar = generate_partial_signing_scalar(mu_P, signing_scalar);

            // create a copy of our partial signing scalars for computation and handling
            crypto_scalar_vector_t keys(partial_signing_scalars);

            // add the partial scalar to the vector
            keys.append(partial_scalar);

            /**
             * Remove any duplicate keys from the scalars that are being added together as we only use
             * unique scalars when computing the resultant scalar
             * p = [pk1 + pk2 + pk3 + ...] mod l
             */
            const auto derived_scalar = keys.dedupe_sort().sum();

            // s = [alpha - (h[real_output_index] * (p * mu_P))] mod l
            finalized_signature[real_output_index] -= (h[real_output_index] * derived_scalar);
        }

        return {true, crypto_clsag_signature_t(finalized_signature, signature.challenge, signature.commitment_image)};
    }

    std::tuple<bool, crypto_clsag_signature_t> generate_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_secret_key_t &secret_ephemeral,
        const std::vector<crypto_public_key_t> &public_keys,
        const crypto_blinding_factor_t &input_blinding_factor,
        const std::vector<crypto_pedersen_commitment_t> &public_commitments,
        const crypto_blinding_factor_t &pseudo_blinding_factor,
        const crypto_pedersen_commitment_t &pseudo_commitment)
    {
        const auto use_commitments =
            (input_blinding_factor != TurtleCoinCrypto::ZERO && public_commitments.size() == public_keys.size()
             && pseudo_blinding_factor != TurtleCoinCrypto::ZERO && pseudo_commitment != TurtleCoinCrypto::Z);

        const auto ring_size = public_keys.size();

        // find our real input in the list
        size_t real_output_index = -1;

        // P = (p * G) mod l
        const auto public_ephemeral = secret_ephemeral * TurtleCoinCrypto::G;

        for (size_t i = 0; i < ring_size; i++)
        {
            if (use_commitments)
            {
                const auto public_commitment = (input_blinding_factor - pseudo_blinding_factor) * TurtleCoinCrypto::G;

                const auto derived_commitment = public_commitments[i] - pseudo_commitment;

                if (public_ephemeral == public_keys[i] && public_commitment == derived_commitment)
                    real_output_index = i;
            }
            else
            {
                if (public_ephemeral == public_keys[i])
                    real_output_index = i;
            }
        }

        /**
         * if we could not find the related public key(s) in the list or the proper
         * commitments provided, then fail as we cannot generate a valid signature
         */
        if (real_output_index == -1)
            return {false, {}};

        const auto key_image = TurtleCoinCrypto::generate_key_image(public_ephemeral, secret_ephemeral);

        const auto [prep_success, signature, h, mu_P] = prepare_ring_signature(
            message_digest,
            key_image,
            public_keys,
            real_output_index,
            input_blinding_factor,
            public_commitments,
            pseudo_blinding_factor,
            pseudo_commitment);

        if (!prep_success)
            return {false, {}};

        return complete_ring_signature(secret_ephemeral, real_output_index, signature, h, mu_P);
    }

    std::tuple<bool, crypto_clsag_signature_t, std::vector<crypto_scalar_t>, crypto_scalar_t> prepare_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_key_image_t &key_image,
        const std::vector<crypto_public_key_t> &public_keys,
        size_t real_output_index,
        const crypto_blinding_factor_t &input_blinding_factor,
        const std::vector<crypto_pedersen_commitment_t> &public_commitments,
        const crypto_blinding_factor_t &pseudo_blinding_factor,
        const crypto_pedersen_commitment_t &pseudo_commitment)
    {
        const auto ring_size = public_keys.size();

        const auto use_commitments =
            (input_blinding_factor != TurtleCoinCrypto::ZERO && public_commitments.size() == public_keys.size()
             && pseudo_blinding_factor != TurtleCoinCrypto::ZERO && pseudo_commitment != TurtleCoinCrypto::Z);

        if (real_output_index >= ring_size)
            return {false, {}, {}, {0}};

        if (!key_image.check_subgroup())
            return {false, {}, {}, {0}};

        // help to provide stronger RNG for the alpha scalar
        crypto_scalar_transcript_t alpha_transcript(message_digest, key_image, TurtleCoinCrypto::random_scalar());

        alpha_transcript.update(input_blinding_factor, pseudo_blinding_factor, pseudo_commitment);

        alpha_transcript.update(public_commitments);

        const auto alpha_scalar = alpha_transcript.challenge();

        auto signature = TurtleCoinCrypto::random_scalars(ring_size);

        // See below for more detail
        const auto z = (input_blinding_factor - pseudo_blinding_factor);

        crypto_key_image_t commitment_image;

        if (use_commitments)
        {
            /**
             * TLDR: If we know the difference between the input blinding scalar and the
             * pseudo output blinding scalar then we can use that difference as the secret
             * key for the difference between the input commitment and the pseudo commitment
             * thus providing no amount component differences in the commitments between the
             * two and hence we are committing (in a non-revealing way) that the pseudo output
             * commitment is equivalent to ONE of the input commitments in the set
             */
            const auto commitment = public_commitments[real_output_index] - pseudo_commitment;

            /**
             * Quick sanity check to make sure that the computed z value (blinding scalar) delta
             * resulting public point is the same as the commitment that we can sign for above
             */
            if (commitment != z * TurtleCoinCrypto::G)
                return {false, {}, {}, {0}};

            /**
             * This likely looks a bit goofy; however, the commitment image is based upon
             * the public output key not the commitment point to prevent a whole bunch
             * of frivolous math that only makes this far worse later
             */
            commitment_image = TurtleCoinCrypto::generate_key_image(public_keys[real_output_index], z);
        }

        std::vector<crypto_scalar_t> h(ring_size);

        crypto_scalar_t mu_P, mu_C;

        // generate mu_P
        {
            crypto_scalar_transcript_t transcript(CLSAG_DOMAIN_0, key_image);

            transcript.update(public_keys);

            if (use_commitments)
            {
                transcript.update(commitment_image);

                transcript.update(public_commitments);

                transcript.update(pseudo_commitment);
            }

            mu_P = transcript.challenge();
        }

        // generate mu_C
        if (use_commitments)
        {
            crypto_scalar_transcript_t transcript(CLSAG_DOMAIN_2, key_image);

            transcript.update(public_keys);

            transcript.update(commitment_image);

            transcript.update(public_commitments);

            transcript.update(pseudo_commitment);

            mu_C = transcript.challenge();
        }

        /**
         * This transcript is the same for each round so re-computing the state of the
         * transcript for each round is a waste of processing power, instead we'll
         * preload this information and make a copy of the state before we use it
         * for each round's computation
         */
        crypto_scalar_transcript_t transcript(CLSAG_DOMAIN_1, message_digest);

        transcript.update(public_keys);

        if (use_commitments)
        {
            transcript.update(public_commitments);

            transcript.update(pseudo_commitment);
        }

        // real input
        {
            // L = (a * G) mod l;
            const auto L = alpha_scalar * G;

            // HP = [Hp(P)] mod l
            const auto HP = TurtleCoinCrypto::hash_to_point(public_keys[real_output_index]);

            // R = (alpha * HP) mod l
            const auto R = alpha_scalar * HP;

            auto sub_transcript = transcript;

            sub_transcript.update(L, R);

            h[(real_output_index + 1) % ring_size] = sub_transcript.challenge();
        }

        if (ring_size > 1)
        {
            for (size_t i = real_output_index + 1; i < real_output_index + ring_size; i++)
            {
                const auto idx = i % ring_size;

                // r = (h[idx] * mu_P) mod l
                const auto r = h[idx] * mu_P;

                // L = [(r * P) + (s[idx] * G)] mod l
                auto L = (r * public_keys[idx]) + (signature[idx] * G);

                // HP = [Hp(P)] mod l
                const auto HP = TurtleCoinCrypto::hash_to_point(public_keys[idx]);

                // R = [(s[idx] * HP) + (r * I)] mod l
                auto R = (signature[idx] * HP) + (r * key_image);

                if (use_commitments)
                {
                    // r2 = (h[idx] * mu_C) mod l
                    const auto r2 = h[idx] * mu_C;

                    /**
                     * Here we're calculating the offset commitments based upon the input
                     * commitments minus our pseudo commitment that we generated thus
                     * allowing us to prove our knowledge of z (the delta between the
                     * input blinding scalar and the pseudo blinding scalar) while committing
                     * to a "zero" amount difference between the two commitments
                     */
                    // C = (C[idx] - PS) mod l
                    const auto C = public_commitments[idx] - pseudo_commitment;

                    // L += (r2 * C[idx]) mod l
                    L += (r2 * C);

                    // R += (r2 * D) mod l
                    R += (r2 * commitment_image);
                }

                auto sub_transcript = transcript;

                sub_transcript.update(L, R);

                h[(idx + 1) % ring_size] = sub_transcript.challenge();
            }
        }

        signature[real_output_index] = alpha_scalar;

        if (use_commitments)
            signature[real_output_index] -= (h[real_output_index] * z * mu_C);

        return {true, crypto_clsag_signature_t(signature, h[0], commitment_image), h, mu_P};
    }
} // namespace TurtleCoinCrypto::RingSignature::CLSAG
