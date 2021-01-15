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

#ifndef CRYPTO_RINGCT_H
#define CRYPTO_RINGCT_H

#include "crypto_common.h"
#include "scalar_transcript.h"

namespace Crypto::RingCT
{
    /**
     * Verifies that the sum of the pseudo commitments is equal to the sum
     * of the output commitments plus the commitment to the transaction fee
     * @param pseudo_commitments
     * @param output_commitments
     * @param transaction_fee
     * @return
     */
    bool check_commitments_parity(
        const std::vector<crypto_pedersen_commitment_t> &pseudo_commitments,
        const std::vector<crypto_pedersen_commitment_t> &output_commitments,
        uint64_t transaction_fee);

    /**
     * Generates the amount mask for the given derivation scalar
     * @param derivation_scalar
     * @return
     */
    crypto_scalar_t generate_amount_mask(const crypto_scalar_t &derivation_scalar);

    /**
     * Generates the commitment blinding factor for the given derivation scalar
     * @param derivation_scalar
     * @return
     */
    crypto_blinding_factor_t generate_commitment_blinding_factor(const crypto_scalar_t &derivation_scalar);

    /**
     * Generate a pedersen commitment using the supplied blinding factor and unmasked amount
     * C = (y * G) + (a * H) mod l
     * @param blinding_factor
     * @param amount
     * @return
     */
    crypto_pedersen_commitment_t
        generate_pedersen_commitment(const crypto_blinding_factor_t &blinding_factor, const uint64_t &amount);

    /**
     * Generate the random pseudo commitment blinding factors and pseudo output commitments
     * from the list of input amounts and the output commitments while proving them to a zero sum
     * @param input_amounts
     * @param output_masks
     * @return
     */
    std::tuple<std::vector<crypto_blinding_factor_t>, std::vector<crypto_pedersen_commitment_t>>
        generate_pseudo_commitments(
            const std::vector<uint64_t> &input_amounts,
            const std::vector<crypto_blinding_factor_t> &output_blinding_factors);

    /**
     * Toggles an amount from unmasked/masked to the inverse state of masked/unmasked using the
     * generated amount mask
     * @param amount_mask
     * @param amount
     * @return
     */
    crypto_scalar_t toggle_masked_amount(const crypto_scalar_t &amount_mask, const crypto_scalar_t &amount);
} // namespace Crypto::RingCT

#endif // CRYPTO_RINGCT_H