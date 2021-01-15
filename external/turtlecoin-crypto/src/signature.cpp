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

#include "signature.h"

static const crypto_scalar_t SIGNATURE_DOMAIN_0 = {0x20, 0x20, 0x49, 0x20, 0x41, 0x54, 0x54, 0x45, 0x53, 0x54, 0x20,
                                                   0x54, 0x48, 0x41, 0x54, 0x20, 0x49, 0x20, 0x48, 0x41, 0x56, 0x45,
                                                   0x20, 0x54, 0x48, 0x45, 0x20, 0x4b, 0x45, 0x59, 0x20, 0x20};

namespace TurtleCoinCrypto::Signature
{
    bool check_signature(
        const crypto_hash_t &message_digest,
        const crypto_public_key_t &public_key,
        const crypto_signature_t &signature)
    {
        // P = [(l * P) + (r * G)] mod l
        const auto point = (signature.LR.L * public_key) + (signature.LR.R * G);

        crypto_scalar_transcript_t transcript(SIGNATURE_DOMAIN_0, message_digest, public_key, point);

        // [(c - sL) mod l] != 0
        return (transcript.challenge() - signature.LR.L).is_nonzero();
    }

    crypto_signature_t complete_signature(
        const crypto_scalar_t &signing_scalar,
        const crypto_signature_t &signature,
        const std::vector<crypto_scalar_t> &partial_signing_scalars)
    {
        auto finalized_signature = signature;

        if (partial_signing_scalars.empty() && signing_scalar != TurtleCoinCrypto::ZERO)
        {
            finalized_signature.LR.R -= (signature.LR.L * signing_scalar);
        }
        else if (!partial_signing_scalars.empty())
        {
            // create a copy of our partial signing scalars for computation and handling
            crypto_scalar_vector_t keys(partial_signing_scalars);

            /**
             * Remove any duplicate keys from the scalars that are being added together as we only use
             * unique scalars when computing the resultant scalar
             * p = [pk1 + pk2 + pk3 + ...] mod l
             */
            const auto derived_scalar = keys.dedupe_sort().sum();

            /**
             * Subtract the result of the aggregated signing scalars from the alpha_scalar value that was
             * supplied by the prepared ring signature to arrive at the final value to complete the
             * given ring signature
             */
            // s[i].R = [alpha_scalar - p]
            finalized_signature.LR.R -= derived_scalar;
        }
        else
        {
            throw std::invalid_argument("must supply a signing scalar or partial signing keys");
        }

        return finalized_signature;
    }

    crypto_scalar_t generate_partial_signing_scalar(
        const crypto_signature_t &signature,
        const crypto_secret_key_t &spend_secret_key)
    {
        // asL = (s.L * a) mod l
        return signature.LR.L * spend_secret_key;
    }

    crypto_signature_t generate_signature(const crypto_hash_t &message_digest, const crypto_secret_key_t &secret_key)
    {
        // A = (a * G) mod l
        const auto public_key = secret_key * G;

        const auto signature = prepare_signature(message_digest, public_key);

        return complete_signature(secret_key, signature);
    }

    crypto_signature_t prepare_signature(const crypto_hash_t &message_digest, const crypto_public_key_t &public_key)
    {
        // help to provide stronger RNG for the alpha scalar
        crypto_scalar_transcript_t alpha_transcript(message_digest, public_key, TurtleCoinCrypto::random_scalar());

        const auto alpha_scalar = alpha_transcript.challenge();

        // P = (a * G) mod l
        const auto point = alpha_scalar * G;

        crypto_scalar_transcript_t transcript(SIGNATURE_DOMAIN_0, message_digest, public_key, point);

        crypto_signature_t signature;

        signature.LR.L = transcript.challenge();

        signature.LR.R = alpha_scalar;

        return signature;
    }
} // namespace TurtleCoinCrypto::Signature
