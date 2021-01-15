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
// https://github.com/SarangNoether/skunkworks/tree/pybullet

#ifndef CRYPTO_RANGEPROOFS_BULLETPROOFS_H
#define CRYPTO_RANGEPROOFS_BULLETPROOFS_H

#include "crypto_common.h"
#include "ringct.h"
#include "scalar_transcript.h"

/**
 * A Bulletproof Range Proof
 */
typedef struct Bulletproof
{
    Bulletproof() {}

    Bulletproof(
        const crypto_point_t &A,
        const crypto_point_t &S,
        const crypto_point_t &T1,
        const crypto_point_t &T2,
        const crypto_scalar_t &taux,
        const crypto_scalar_t &mu,
        std::vector<crypto_point_t> L,
        std::vector<crypto_point_t> R,
        const crypto_scalar_t &g,
        const crypto_scalar_t &h,
        const crypto_scalar_t &t):
        A(A), S(S), T1(T1), T2(T2), taux(taux), mu(mu), L(std::move(L)), R(std::move(R)), g(g), h(h), t(t)
    {
    }

    Bulletproof(const JSONValue &j)
    {
        from_json(j);
    }

    Bulletproof(const JSONValue &j, const std::string &key)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsObject())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_json(val);
    }

    Bulletproof(const std::string &input)
    {
        const auto string = TurtleCoinCrypto::StringTools::from_hex(input);

        deserialize(string);
    }

    Bulletproof(const std::vector<uint8_t> &input)
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

        writer.key(S);

        writer.key(T1);

        writer.key(T2);

        writer.key(taux);

        writer.key(mu);

        writer.varint<uint64_t>(L.size());

        for (const auto &val : L)
            writer.key(val);

        writer.varint<uint64_t>(R.size());

        for (const auto &val : R)
            writer.key(val);

        writer.key(g);

        writer.key(h);

        writer.key(t);

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

    /**
     * Writes the structure as JSON to the provided writer
     * @param writer
     */
    void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
    {
        writer.StartObject();
        {
            writer.Key("A");
            A.toJSON(writer);

            writer.Key("S");
            S.toJSON(writer);

            writer.Key("T1");
            T1.toJSON(writer);

            writer.Key("T2");
            T2.toJSON(writer);

            writer.Key("taux");
            taux.toJSON(writer);

            writer.Key("mu");
            mu.toJSON(writer);

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

            writer.Key("g");
            g.toJSON(writer);

            writer.Key("h");
            h.toJSON(writer);

            writer.Key("t");
            t.toJSON(writer);
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

        return TurtleCoinCrypto::StringTools::to_hex(bytes.data(), bytes.size());
    }

    crypto_point_t A, S, T1, T2;
    crypto_scalar_t taux, mu;
    std::vector<crypto_point_t> L, R;
    crypto_scalar_t g, h, t;

  private:
    void deserialize(const std::vector<uint8_t> &input)
    {
        deserializer_t reader(input);

        A = reader.key<crypto_point_t>();

        S = reader.key<crypto_point_t>();

        T1 = reader.key<crypto_point_t>();

        T2 = reader.key<crypto_point_t>();

        taux = reader.key<crypto_scalar_t>();

        mu = reader.key<crypto_scalar_t>();

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

        g = reader.key<crypto_scalar_t>();

        h = reader.key<crypto_scalar_t>();

        t = reader.key<crypto_scalar_t>();
    }

    void from_json(const JSONValue &j)
    {
        if (!j.IsObject())
            throw std::invalid_argument("JSON value is of the wrong type");

        if (!has_member(j, "A"))
            throw std::invalid_argument("A not found in JSON object");

        A = get_json_string(j, "A");

        if (!has_member(j, "S"))
            throw std::invalid_argument("S not found in JSON object");

        S = get_json_string(j, "S");

        if (!has_member(j, "T1"))
            throw std::invalid_argument("T1 not found in JSON object");

        T1 = get_json_string(j, "T1");

        if (!has_member(j, "T2"))
            throw std::invalid_argument("T2 not found in JSON object");

        T2 = get_json_string(j, "T2");

        if (!has_member(j, "taux"))
            throw std::invalid_argument("taux not found in JSON object");

        taux = get_json_string(j, "taux");

        if (!has_member(j, "mu"))
            throw std::invalid_argument("mu not found in JSON object");

        mu = get_json_string(j, "mu");

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

        if (!has_member(j, "g"))
            throw std::invalid_argument("a not found in JSON object");

        g = get_json_string(j, "g");

        if (!has_member(j, "h"))
            throw std::invalid_argument("b not found in JSON object");

        h = get_json_string(j, "h");

        if (!has_member(j, "t"))
            throw std::invalid_argument("t not found in JSON object");

        t = get_json_string(j, "t");
    }
} crypto_bulletproof_t;

namespace TurtleCoinCrypto::RangeProofs::Bulletproofs
{
    /**
     * Generates a Bulletproof range proof and the related pedersen commitments
     * for the given amounts and blinding factors
     * @param amounts
     * @param blinding_factors
     * @param N the number of bits (2^n) to prove
     * @return {proof, commitments}
     */
    std::tuple<crypto_bulletproof_t, std::vector<crypto_pedersen_commitment_t>> prove(
        const std::vector<uint64_t> &amounts,
        const std::vector<crypto_blinding_factor_t> &blinding_factors,
        size_t N = 64);

    /**
     * Performs batch verification of the range proofs provided for the provided
     * pedersen commitments to the given values
     * @param proofs
     * @param commitments[]
     * @param N the number of bits (2^n) to prove
     * @return
     */
    bool verify(
        const std::vector<crypto_bulletproof_t> &proofs,
        const std::vector<std::vector<crypto_pedersen_commitment_t>> &commitments,
        size_t N = 64);

    /**
     * Performs verification of the range proof provided for the provided
     * pedersen commitments to the given values
     * @param proof
     * @param commitments
     * @param N the number of bits (2^n) to prove
     * @return
     */
    bool verify(
        const crypto_bulletproof_t &proof,
        const std::vector<crypto_pedersen_commitment_t> &commitments,
        size_t N = 64);
} // namespace TurtleCoinCrypto::RangeProofs::Bulletproofs

namespace std
{
    inline ostream &operator<<(ostream &os, const crypto_bulletproof_t &value)
    {
        os << "Bulletproof:" << std::endl
           << "\tA: " << value.A << std::endl
           << "\tS: " << value.S << std::endl
           << "\tT1: " << value.T1 << std::endl
           << "\tT2: " << value.T2 << std::endl
           << "\ttaux: " << value.taux << std::endl
           << "\tmu: " << value.mu << std::endl
           << "\tL:" << std::endl;

        for (const auto &val : value.L)
            os << "\t\t" << val << std::endl;

        os << "\tR:" << std::endl;

        for (const auto &val : value.R)
            os << "\t\t" << val << std::endl;

        os << "\tg: " << value.g << std::endl << "\th: " << value.h << std::endl << "\tt: " << value.t << std::endl;

        return os;
    }
} // namespace std

#endif // CRYPTO_RANGEPROOFS_BULLETPROOFS_H
