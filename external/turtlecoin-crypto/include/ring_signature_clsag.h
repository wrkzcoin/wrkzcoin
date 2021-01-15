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

#ifndef CRYPTO_RING_SIGNATURE_CLSAG_H
#define CRYPTO_RING_SIGNATURE_CLSAG_H

#include "crypto_common.h"
#include "ringct.h"
#include "scalar_transcript.h"

typedef struct CLSAGSignature
{
    CLSAGSignature() {}

    CLSAGSignature(
        std::vector<crypto_scalar_t> scalars,
        const crypto_scalar_t challenge,
        const crypto_key_image_t &commitment_image = TurtleCoinCrypto::Z):
        scalars(std::move(scalars)), challenge(challenge), commitment_image(commitment_image)
    {
    }

    CLSAGSignature(const JSONValue &j)
    {
        from_json(j);
    }

    CLSAGSignature(const JSONValue &j, const std::string &key)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsObject())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_json(val);
    }

    CLSAGSignature(const std::string &input)
    {
        const auto string = TurtleCoinCrypto::StringTools::from_hex(input);

        deserialize(string);
    }

    CLSAGSignature(const std::vector<uint8_t> &input)
    {
        deserialize(input);
    }

    std::vector<uint8_t> serialize() const
    {
        serializer_t writer;

        writer.varint<uint64_t>(scalars.size());

        for (const auto &val : scalars)
            writer.key(val);

        writer.key(challenge);

        if (commitment_image != TurtleCoinCrypto::Z)
        {
            writer.boolean(true);

            writer.key(commitment_image);
        }
        else
        {
            writer.boolean(false);
        }

        return writer.vector();
    }

    size_t size() const
    {
        return serialize().size();
    }

    void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
    {
        writer.StartObject();
        {
            writer.Key("scalars");
            writer.StartArray();
            {
                for (const auto &val : scalars)
                    val.toJSON(writer);
            }
            writer.EndArray();

            writer.Key("challenge");
            challenge.toJSON(writer);

            if (commitment_image != TurtleCoinCrypto::Z)
            {
                writer.Key("commitment_image");
                commitment_image.toJSON(writer);
            }
        }
        writer.EndObject();
    }

    std::string to_string() const
    {
        const auto bytes = serialize();

        return TurtleCoinCrypto::StringTools::to_hex(bytes.data(), bytes.size());
    }

    std::vector<crypto_scalar_t> scalars;
    crypto_key_image_t commitment_image;
    crypto_scalar_t challenge;

  private:
    void from_json(const JSONValue &j)
    {
        if (!j.IsObject())
            throw std::invalid_argument("JSON value is of the wrong type");

        if (!has_member(j, "scalars"))
            throw std::invalid_argument("scalars not found in JSON object");

        if (!has_member(j, "challenge"))
            throw std::invalid_argument("challenge not found in JSON object");

        if (has_member(j, "commitment_image"))
            commitment_image = get_json_string(j, "commitment_image");

        challenge = get_json_string(j, "challenge");

        scalars.clear();

        for (const auto &elem : get_json_array(j, "scalars"))
            scalars.emplace_back(get_json_string(elem));
    }

    void deserialize(const std::vector<uint8_t> &input)
    {
        deserializer_t reader(input);

        const auto scalar_count = reader.varint<uint64_t>();

        scalars.clear();

        for (size_t i = 0; i < scalar_count; ++i)
            scalars.push_back(reader.key<crypto_scalar_t>());

        challenge = reader.key<crypto_scalar_t>();

        if (reader.boolean())
            commitment_image = reader.key<crypto_key_image_t>();
    }
} crypto_clsag_signature_t;

namespace TurtleCoinCrypto::RingSignature::CLSAG
{
    /**
     * Checks the CLSAG ring signature presented
     * @param message_digest
     * @param key_image
     * @param public_keys
     * @param signature
     * @param commitments
     * @param pseudo_commitment
     * @return
     */
    bool check_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_key_image_t &key_image,
        const std::vector<crypto_public_key_t> &public_keys,
        const crypto_clsag_signature_t &signature,
        const std::vector<crypto_pedersen_commitment_t> &commitments = {},
        const crypto_pedersen_commitment_t &pseudo_commitment = TurtleCoinCrypto::Z);

    /**
     * Completes the prepared CLSAG ring signature
     * @param signing_scalar
     * @param real_output_index
     * @param signature
     * @param h
     * @param mu_P
     * @param partial_signing_scalars
     * @return
     */
    std::tuple<bool, crypto_clsag_signature_t> complete_ring_signature(
        const crypto_scalar_t &signing_scalar,
        size_t real_output_index,
        const crypto_clsag_signature_t &signature,
        const std::vector<crypto_scalar_t> &h,
        const crypto_scalar_t &mu_P,
        const std::vector<crypto_scalar_t> &partial_signing_scalars = {});

    /**
     * Generates a partial signing scalar that is a factor of a full signing scalar and typically
     * used by multisig wallets -- input data is supplied from prepare_ring_signature
     * @param mu_P
     * @param spend_secret_key
     * @return
     */
    crypto_scalar_t
        generate_partial_signing_scalar(const crypto_scalar_t &mu_P, const crypto_secret_key_t &spend_secret_key);

    /**
     * Generates CLSAG ring signature using the secrets provided
     * @param message_digest
     * @param secret_ephemeral
     * @param public_keys
     * @param input_blinding_factor
     * @param public_commitments
     * @param pseudo_blinding_factor
     * @param pseudo_commitment
     * @return
     */
    std::tuple<bool, crypto_clsag_signature_t> generate_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_secret_key_t &secret_ephemeral,
        const std::vector<crypto_public_key_t> &public_keys,
        const crypto_blinding_factor_t &input_blinding_factor = TurtleCoinCrypto::ZERO,
        const std::vector<crypto_pedersen_commitment_t> &public_commitments = {},
        const crypto_blinding_factor_t &pseudo_blinding_factor = TurtleCoinCrypto::ZERO,
        const crypto_pedersen_commitment_t &pseudo_commitment = TurtleCoinCrypto::Z);

    /**
     * Prepares CLSAG ring signature using the primitive values provided
     * Must be completed via complete_ring_signature before they will validate
     * @param message_digest
     * @param key_image
     * @param public_keys
     * @param real_output_index
     * @param input_blinding_factor
     * @param public_commitments
     * @param pseudo_blinding_factor
     * @param pseudo_commitment
     * @return
     */
    std::tuple<bool, crypto_clsag_signature_t, std::vector<crypto_scalar_t>, crypto_scalar_t> prepare_ring_signature(
        const crypto_hash_t &message_digest,
        const crypto_key_image_t &key_image,
        const std::vector<crypto_public_key_t> &public_keys,
        size_t real_output_index = 0,
        const crypto_blinding_factor_t &input_blinding_factor = TurtleCoinCrypto::ZERO,
        const std::vector<crypto_pedersen_commitment_t> &public_commitments = {},
        const crypto_blinding_factor_t &pseudo_blinding_factor = TurtleCoinCrypto::ZERO,
        const crypto_pedersen_commitment_t &pseudo_commitment = TurtleCoinCrypto::Z);
} // namespace TurtleCoinCrypto::RingSignature::CLSAG

namespace std
{
    inline ostream &operator<<(ostream &os, const crypto_clsag_signature_t &value)
    {
        os << "CLSAG:" << std::endl << "\tscalars:" << std::endl;

        for (const auto &val : value.scalars)
            os << "\t\t" << val << std::endl;

        os << "\tchallenge: " << value.challenge << std::endl;

        if (value.commitment_image != TurtleCoinCrypto::Z)
            os << "\tcommitment_image: " << value.commitment_image << std::endl;

        return os;
    }
} // namespace std

#endif // CRYPTO_RING_SIGNATURE_CLSAG_H
