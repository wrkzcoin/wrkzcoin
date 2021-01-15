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

#ifndef CRYPTO_MULTISIG_H
#define CRYPTO_MULTISIG_H

#include "crypto_common.h"

namespace TurtleCoinCrypto::Multisig
{
    /**
     * Generates a multisig secret key using our secret key and their public key
     * @param their_public_key
     * @param our_secret_key
     * @return
     */
    crypto_secret_key_t generate_multisig_secret_key(
        const crypto_public_key_t &their_public_key,
        const crypto_secret_key_t &our_secret_key);

    /**
     * Generate a vector of multisig secret keys from our secret key and a vector
     * of the other participants public keys
     * @param their_public_keys
     * @param our_secret_key
     * @return
     */
    std::vector<crypto_secret_key_t> generate_multisig_secret_keys(
        const std::vector<crypto_public_key_t> &their_public_keys,
        const crypto_secret_key_t &our_secret_key);

    /**
     * Generate a shared public key from a vector of participant public keys
     * @param public_keys
     * @return
     */
    crypto_public_key_t generate_shared_public_key(const std::vector<crypto_public_key_t> &public_keys);

    /**
     * Generate a shared secret key from a vector of participant secret keys
     * @param secret_keys
     * @return
     */
    crypto_secret_key_t generate_shared_secret_key(const std::vector<crypto_secret_key_t> &secret_keys);

    /**
     * Calculates the number of mutlisig secret key exchange key rounds that must be
     * completed before we will have the desired number of multisig keys for the M:N
     * multisig scheme. N:N is easy, N-1:N is pretty easy, and M:N gets insane
     * @param participants
     * @param threshold
     * @return
     */
    size_t rounds_required(size_t participants, size_t threshold);
} // namespace TurtleCoinCrypto::Multisig

#endif // CRYPTO_MULTISIG_H