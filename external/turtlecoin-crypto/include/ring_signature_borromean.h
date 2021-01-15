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

#ifndef CRYPTO_RING_SIGNATURE_BORROMEAN_H
#define CRYPTO_RING_SIGNATURE_BORROMEAN_H

#include "crypto_common.h"
#include "scalar_transcript.h"

namespace Crypto::RingSignature::Borromean
{
    /**
     * Checks the Borromean ring signature presented
     * @param message_digest
     * @param key_image
     * @param public_keys
     * @param signature
     * @return
     */
    bool check_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_key_image_t &key_image,
        const std::vector<crypto_public_key_t> &public_keys,
        const std::vector<crypto_signature_t> &signature);

    /**
     * Completes the prepared Borromean ring signature
     * @param signing_scalar
     * @param real_output_index
     * @param alpha_scalar
     * @param signature
     * @param partial_signing_scalars
     * @return
     */
    std::tuple<bool, std::vector<crypto_signature_t>> complete_ring_signature(
        const crypto_scalar_t &signing_scalar,
        size_t real_output_index,
        const std::vector<crypto_signature_t> &signature,
        const std::vector<crypto_scalar_t> &partial_signing_scalars = {});

    /**
     * Generates a partial signing scalar that is a factor of a full signing scalar and typically
     * used by multisig wallets -- input data is supplied from prepare_ring_signature
     * @param real_output_index
     * @param signature
     * @param spend_secret_key
     * @return
     */
    crypto_scalar_t generate_partial_signing_scalar(
        size_t real_output_index,
        const std::vector<crypto_signature_t> &signature,
        const crypto_secret_key_t &spend_secret_key);

    /**
     * Generates Borromean ring signature using the secret key provided
     * @param message_digest
     * @param secret_ephemeral
     * @param public_keys
     * @return
     */
    std::tuple<bool, std::vector<crypto_signature_t>> generate_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_secret_key_t &secret_ephemeral,
        const std::vector<crypto_public_key_t> &public_keys);

    /**
     * Prepares Borromean ring signature using the primitive values provided
     * Must be completed via complete_ring_signature before they will validate
     * @param message_digest
     * @param key_image
     * @param public_keys
     * @param real_output_index
     * @param alpha_scalar
     * @return
     */
    std::tuple<bool, std::vector<crypto_signature_t>> prepare_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_key_image_t &key_image,
        const std::vector<crypto_public_key_t> &public_keys,
        size_t real_output_index);
} // namespace Crypto::RingSignature::Borromean

#endif // CRYPTO_RING_SIGNATURE_BORROMEAN_H
