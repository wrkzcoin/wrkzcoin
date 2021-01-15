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
// https://github.com/SarangNoether/skunkworks/tree/pybullet-plus

#ifndef CRYPTO_RANGEPROOFS_BULLETPROOFS_PLUS_H
#define CRYPTO_RANGEPROOFS_BULLETPROOFS_PLUS_H

#include "crypto_common.h"
#include "ringct.h"
#include "scalar_transcript.h"

/**
 * A Bulletproof+ Range Proof
 */
typedef struct BulletproofPlus
{
    BulletproofPlus() {}

    BulletproofPlus(
        const crypto_point_t &A,
        const crypto_point_t &A1,
        const crypto_point_t &B,
        const crypto_scalar_t &r1,
        const crypto_scalar_t &s1,
        const crypto_scalar_t &d1,
        std::vector<crypto_point_t> L,
        std::vector<crypto_point_t> R):
        A(A), A1(A1), B(B), r1(r1), s1(s1), d1(d1), L(std::move(L)), R(std::move(R))
    {
    }

    BulletproofPlus(const JSONValue &j)
    {
        from_json(j);
    }

    BulletproofPlus(const JSONValue &j, const std::string &key)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsObject())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_json(val);
    }

    BulletproofPlus(const std::string &input)
    {
        const auto string = Crypto::StringTools::from_hex(input);

        deserialize(string);
    }

    BulletproofPlus(const std::vector<uint8_t> &input)
    {
        deserialize(input);
    }

    /**
     * Serializes the struct to a byte array
     * @return
     */
    std::vector<uint8_t> serialize() const
    {
        serializer_t writer;

        writer.key(A);

        writer.key(A1);

        writer.key(B);

        writer.key(r1);

        writer.key(s1);

        writer.key(d1);

        writer.varint<uint64_t>(L.size());

        for (const auto &val : L)
            writer.key(val);

        writer.varint<uint64_t>(R.size());

        for (const auto &val : R)
            writer.key(val);

        return writer.vector();
    }

    /**
     * Returns the serialized byte size
     * @return
     */
    size_t size() const
    {
        return serialize().size();
    }

    void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
    {
        writer.StartObject();
        {
            writer.Key("A");
            A.toJSON(writer);

            writer.Key("A1");
            A1.toJSON(writer);

            writer.Key("B");
            B.toJSON(writer);

            writer.Key("r1");
            r1.toJSON(writer);

            writer.Key("s1");
            s1.toJSON(writer);

            writer.Key("d1");
            d1.toJSON(writer);

            writer.Key("L");
            writer.StartArray();
            {
                for (const auto &val : L)
                    val.toJSON(writer);
            }
            writer.EndArray();

            writer.Key("R");
            writer.StartArray();
            {
                for (const auto &val : R)
                    val.toJSON(writer);
            }
            writer.EndArray();
        }
        writer.EndObject();
    }

    /**
     * Returns the hex encoded serialized byte array
     * @return
     */
    std::string to_string() const
    {
        const auto bytes = serialize();

        return Crypto::StringTools::to_hex(bytes.data(), bytes.size());
    }

    crypto_point_t A, A1, B;
    crypto_scalar_t r1, s1, d1;
    std::vector<crypto_point_t> L, R;

  private:
    void deserialize(const std::vector<uint8_t> &input)
    {
        deserializer_t reader(input);

        A = reader.key<crypto_point_t>();

        A1 = reader.key<crypto_point_t>();

        B = reader.key<crypto_point_t>();

        r1 = reader.key<crypto_scalar_t>();

        s1 = reader.key<crypto_scalar_t>();

        d1 = reader.key<crypto_scalar_t>();

        {
            const auto count = reader.varint<uint64_t>();

            L.clear();

            for (size_t i = 0; i < count; ++i)
                L.push_back(reader.key<crypto_point_t>());
        }

        {
            const auto count = reader.varint<uint64_t>();

            R.clear();

            for (size_t i = 0; i < count; ++i)
                R.push_back(reader.key<crypto_point_t>());
        }
    }

    void from_json(const JSONValue &j)
    {
        if (!j.IsObject())
            throw std::invalid_argument("JSON value is of the wrong type");

        if (!has_member(j, "A"))
            throw std::invalid_argument("A not found in JSON object");

        A = get_json_string(j, "A");

        if (!has_member(j, "A1"))
            throw std::invalid_argument("A1 not found in JSON object");

        A1 = get_json_string(j, "A1");

        if (!has_member(j, "B"))
            throw std::invalid_argument("B not found in JSON object");

        B = get_json_string(j, "B");

        if (!has_member(j, "r1"))
            throw std::invalid_argument("r1 not found in JSON object");

        r1 = get_json_string(j, "r1");

        if (!has_member(j, "s1"))
            throw std::invalid_argument("s1 not found in JSON object");

        s1 = get_json_string(j, "s1");

        if (!has_member(j, "d1"))
            throw std::invalid_argument("d1 not found in JSON object");

        d1 = get_json_string(j, "d1");

        if (!has_member(j, "L"))
            throw std::invalid_argument("L not found in JSON object");

        L.clear();

        for (const auto &elem : get_json_array(j, "L"))
            L.emplace_back(get_json_string(elem));

        if (!has_member(j, "R"))
            throw std::invalid_argument("R not found in JSON object");

        R.clear();

        for (const auto &elem : get_json_array(j, "R"))
            R.emplace_back(get_json_string(elem));
    }
} crypto_bulletproof_plus_t;

namespace Crypto::RangeProofs::BulletproofsPlus
{
    /**
     * Generates a Bulletproof+ range proof and the related pedersen commitments
     * for the given amounts and blinding factors
     * @param amounts
     * @param blinding_factors
     * @param N
     * @return
     */
    std::tuple<crypto_bulletproof_plus_t, std::vector<crypto_pedersen_commitment_t>> prove(
        const std::vector<uint64_t> &amounts,
        const std::vector<crypto_blinding_factor_t> &blinding_factors,
        size_t N = 64);

    /**
     * Performs batch verification of the range proofs provided for the provided
     * pedersen commitments to the given values
     * @param proofs
     * @param commitments
     * @param N
     * @return
     */
    bool verify(
        const std::vector<crypto_bulletproof_plus_t> &proofs,
        const std::vector<std::vector<crypto_pedersen_commitment_t>> &commitments,
        size_t N = 64);

    /**
     * Performs verification of the range proof provided for the provided
     * pedersen commitments to the given values
     * @param proof
     * @param commitments
     * @param N
     * @return
     */
    bool verify(
        const crypto_bulletproof_plus_t &proof,
        const std::vector<crypto_pedersen_commitment_t> &commitments,
        size_t N = 64);
} // namespace Crypto::RangeProofs::BulletproofsPlus

namespace std
{
    inline ostream &operator<<(ostream &os, const crypto_bulletproof_plus_t &value)
    {
        os << "Bulletproof+:" << std::endl
           << "\tA: " << value.A << std::endl
           << "\tA1: " << value.A1 << std::endl
           << "\tB: " << value.B << std::endl
           << "\tr1: " << value.r1 << std::endl
           << "\ts1: " << value.s1 << std::endl
           << "\td1: " << value.d1 << std::endl
           << "\tL: " << std::endl;

        for (const auto &val : value.L)
            os << "\t\t" << val << std::endl;

        os << "\tR:" << std::endl;

        for (const auto &val : value.R)
            os << "\t\t" << val << std::endl;

        return os;
    }
} // namespace std

#endif // CRYPTO_RANGEPROOFS_BULLETPROOFS_PLUS_H
