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

#include <nan.h>
#include <v8.h>

/**
 * A whole bunch of of NAN helper functions to save a lot of typing
 */

template<typename T> static inline v8::Local<T> to_v8_string(const std::string &string)
{
    return Nan::New(string).ToLocalChecked();
}

#define STR_TO_NAN_VAL(string) to_v8_string<v8::Value>(string)
#define STR_TO_NAN_STR(string) to_v8_string<v8::String>(string)
#define NAN_TO_STR(value) \
    std::string(*Nan::Utf8String(value->ToString(Nan::GetCurrentContext()).FromMaybe(v8::Local<v8::String>())));
#define NAN_TO_UINT32(value) Nan::To<uint32_t>(value).FromJust()
#define NAN_TO_UINT64(value) (uint64_t) Nan::To<uint32_t>(value).FromJust()

template<typename T> static inline v8::Local<v8::Array> to_v8_array(const std::vector<T> &vector)
{
    auto array = Nan::New<v8::Array>(vector.size());

    for (size_t i = 0; i < vector.size(); ++i)
        Nan::Set(array, i, STR_TO_NAN_VAL(vector[i].to_string()));

    return array;
}

static inline v8::Local<v8::Array> prepare(const bool success, const v8::Local<v8::Value> &val)
{
    v8::Local<v8::Array> result = Nan::New<v8::Array>(2);

    Nan::Set(result, 0, Nan::New(!success));

    Nan::Set(result, 1, val);

    return result;
}

static inline v8::Local<v8::Object> get_object(const v8::Local<v8::Value> &nan_value)
{
    auto result = Nan::New<v8::Object>();

    if (nan_value->IsObject())
        result = v8::Local<v8::Object>::Cast(nan_value);

    return result;
}

static inline v8::Local<v8::Object> get_object(const Nan::FunctionCallbackInfo<v8::Value> &info, const uint8_t index)
{
    v8::Local<v8::Object> result = Nan::New<v8::Object>();

    if (info.Length() >= index)
        result = get_object(info[index]);

    return result;
}

static inline bool object_has(const v8::Local<v8::Object> &obj, const std::string &key)
{
    return Nan::Has(obj, STR_TO_NAN_STR(key)).FromJust();
}

template<typename T> static inline T get(const v8::Local<v8::Value> &nan_value)
{
    T result = T();

    return result;
}

template<> inline std::string get<std::string>(const v8::Local<v8::Value> &nan_value)
{
    auto result = std::string();

    if (nan_value->IsString())
        result = NAN_TO_STR(nan_value);

    return result;
}

template<> inline uint32_t get<uint32_t>(const v8::Local<v8::Value> &nan_value)
{
    uint32_t result = 0;

    if (nan_value->IsNumber())
        result = NAN_TO_UINT32(nan_value);

    return result;
}

template<> inline uint64_t get<uint64_t>(const v8::Local<v8::Value> &nan_value)
{
    uint64_t result = 0;

    if (nan_value->IsNumber())
        result = NAN_TO_UINT64(nan_value);

    return result;
}

template<typename T> static inline T get(const Nan::FunctionCallbackInfo<v8::Value> &info, const uint8_t index)
{
    T result;

    if (info.Length() >= index)
        result = get<T>(info[index]);

    return result;
}

static inline std::string get(const v8::Local<v8::Object> &obj, const std::string &key)
{
    std::string result = std::string();

    if (object_has(obj, key))
    {
        const auto value = Nan::Get(obj, STR_TO_NAN_VAL(key)).ToLocalChecked();

        result = get<std::string>(value);
    }

    return result;
}

template<typename T> static inline T get_crypto_t(const Nan::FunctionCallbackInfo<v8::Value> &info, const uint8_t index)
{
    T result;

    const auto value = get<std::string>(info, index);

    if (!value.empty())
        try
        {
            result = value;
        }
        catch (...)
        {
        }

    return result;
}

template<typename T> static inline T get_crypto_t(const v8::Local<v8::Object> &obj, const std::string &key)
{
    T result;

    const auto value = get(obj, key);

    if (!value.empty())
        result = value;

    return result;
}

template<typename T> static inline std::vector<T> get_vector(const v8::Local<v8::Array> &array)
{
    std::vector<T> results;

    const auto array_size = array->Length();

    for (size_t i = 0; i < array_size; ++i)
    {
        const auto nan_value = Nan::Get(array, i).ToLocalChecked();

        if (nan_value->IsString())
        {
            const auto value = std::string(*Nan::Utf8String(nan_value));

            try
            {
                results.push_back(value);
            }
            catch (...)
            {
            }
        }
    }

    /**
     * If our resulting array size is not what we expected, then something
     * in the array was not what we expected it to be which means that the
     * entire array is garbage
     */
    if (results.size() != array_size)
        results.clear();

    return results;
}

template<> inline std::vector<uint64_t> get_vector<uint64_t>(const v8::Local<v8::Array> &array)
{
    std::vector<uint64_t> results;

    const auto array_size = array->Length();

    for (size_t i = 0; i < array_size; ++i)
    {
        const auto nan_value = Nan::Get(array, i).ToLocalChecked();

        if (nan_value->IsNumber())
        {
            const auto value = (uint64_t)Nan::To<uint32_t>(nan_value).FromJust();

            results.push_back(value);
        }
    }

    /**
     * If our resulting array size is not what we expected, then something
     * in the array was not what we expected it to be which means that the
     * entire array is garbage
     */
    if (results.size() != array_size)
        results.clear();

    return results;
}

template<typename T>
static inline std::vector<T> get_vector(const Nan::FunctionCallbackInfo<v8::Value> &info, const uint8_t index)
{
    std::vector<T> results;

    if (info.Length() >= index)
        if (info[index]->IsArray())
        {
            const auto array = v8::Local<v8::Array>::Cast(info[index]);

            results = get_vector<T>(array);
        }

    return results;
}

template<typename T> static inline std::vector<T> get_vector(const v8::Local<v8::Object> &obj, const std::string &key)
{
    std::vector<T> results;

    if (object_has(obj, key))
    {
        const auto nan_value = Nan::Get(obj, STR_TO_NAN_VAL(key)).ToLocalChecked();

        if (nan_value->IsArray())
        {
            const auto array = v8::Local<v8::Array>::Cast(nan_value);

            results = get_vector<T>(array);
        }
    }

    return results;
}

template<> inline std::vector<crypto_bulletproof_t> get_vector<crypto_bulletproof_t>(const v8::Local<v8::Array> &array)
{
    std::vector<crypto_bulletproof_t> results;

    const auto array_size = array->Length();

    for (size_t i = 0; i < array_size; ++i)
    {
        const auto nan_value = Nan::Get(array, i).ToLocalChecked();

        if (nan_value->IsObject())
        {
            const auto proof_obj = get_object(nan_value);

            crypto_bulletproof_t proof(
                get_crypto_t<crypto_point_t>(proof_obj, "A"),
                get_crypto_t<crypto_point_t>(proof_obj, "S"),
                get_crypto_t<crypto_point_t>(proof_obj, "T1"),
                get_crypto_t<crypto_point_t>(proof_obj, "T2"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "taux"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "mu"),
                get_vector<crypto_point_t>(proof_obj, "L"),
                get_vector<crypto_point_t>(proof_obj, "R"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "g"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "h"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "t"));

            results.push_back(proof);
        }
    }

    /**
     * If our resulting array size is not what we expected, then something
     * in the array was not what we expected it to be which means that the
     * entire array is garbage
     */
    if (results.size() != array_size)
        results.clear();

    return results;
}


template<>
inline std::vector<crypto_bulletproof_plus_t> get_vector<crypto_bulletproof_plus_t>(const v8::Local<v8::Array> &array)
{
    std::vector<crypto_bulletproof_plus_t> results;

    const auto array_size = array->Length();

    for (size_t i = 0; i < array_size; ++i)
    {
        const auto nan_value = Nan::Get(array, i).ToLocalChecked();

        if (nan_value->IsObject())
        {
            const auto proof_obj = get_object(nan_value);

            crypto_bulletproof_plus_t proof(
                get_crypto_t<crypto_point_t>(proof_obj, "A"),
                get_crypto_t<crypto_point_t>(proof_obj, "A1"),
                get_crypto_t<crypto_point_t>(proof_obj, "B"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "r1"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "s1"),
                get_crypto_t<crypto_scalar_t>(proof_obj, "d1"),
                get_vector<crypto_point_t>(proof_obj, "L"),
                get_vector<crypto_point_t>(proof_obj, "R"));

            results.push_back(proof);
        }
    }

    /**
     * If our resulting array size is not what we expected, then something
     * in the array was not what we expected it to be which means that the
     * entire array is garbage
     */
    if (results.size() != array_size)
        results.clear();

    return results;
}

template<>
inline std::vector<std::vector<crypto_pedersen_commitment_t>>
    get_vector<std::vector<crypto_pedersen_commitment_t>>(const v8::Local<v8::Array> &array)
{
    std::vector<std::vector<crypto_pedersen_commitment_t>> results;

    const auto outer_array_size = array->Length();

    for (size_t i = 0; i < outer_array_size; ++i)
    {
        const auto outer_element = Nan::Get(array, i).ToLocalChecked();

        if (outer_element->IsArray())
        {
            const auto inner_array = v8::Local<v8::Array>::Cast(outer_element);

            const auto inner_elements = get_vector<crypto_pedersen_commitment_t>(inner_array);

            if (!inner_elements.empty())
                results.push_back(inner_elements);
        }
    }

    /**
     * If our resulting array size is not what we expected, then something
     * in the array was not what we expected it to be which means that the
     * entire array is garbage
     */
    if (results.size() != outer_array_size)
        results.clear();

    return results;
}

static inline v8::Local<v8::Object> to_v8_object(const crypto_clsag_signature_t &signature)
{
    v8::Local<v8::Object> jsonObject = Nan::New<v8::Object>();

    Nan::Set(jsonObject, STR_TO_NAN_VAL("scalars"), to_v8_array(signature.scalars));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("challenge"), STR_TO_NAN_VAL(signature.challenge.to_string()));

    if (signature.commitment_image != TurtleCoinCrypto::Z)
        Nan::Set(
            jsonObject, STR_TO_NAN_VAL("commitment_image"), STR_TO_NAN_VAL(signature.commitment_image.to_string()));

    return jsonObject;
}

static inline v8::Local<v8::Object> to_v8_object(const crypto_bulletproof_t &proof)
{
    v8::Local<v8::Object> jsonObject = Nan::New<v8::Object>();

    Nan::Set(jsonObject, STR_TO_NAN_VAL("A"), STR_TO_NAN_VAL(proof.A.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("S"), STR_TO_NAN_VAL(proof.S.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("T1"), STR_TO_NAN_VAL(proof.T1.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("T2"), STR_TO_NAN_VAL(proof.T2.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("taux"), STR_TO_NAN_VAL(proof.taux.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("mu"), STR_TO_NAN_VAL(proof.mu.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("L"), to_v8_array(proof.L));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("R"), to_v8_array(proof.R));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("g"), STR_TO_NAN_VAL(proof.g.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("h"), STR_TO_NAN_VAL(proof.h.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("t"), STR_TO_NAN_VAL(proof.t.to_string()));

    return jsonObject;
}

static inline v8::Local<v8::Object> to_v8_object(const crypto_bulletproof_plus_t &proof)
{
    v8::Local<v8::Object> jsonObject = Nan::New<v8::Object>();

    Nan::Set(jsonObject, STR_TO_NAN_VAL("A"), STR_TO_NAN_VAL(proof.A.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("A1"), STR_TO_NAN_VAL(proof.A1.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("B"), STR_TO_NAN_VAL(proof.B.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("r1"), STR_TO_NAN_VAL(proof.r1.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("s1"), STR_TO_NAN_VAL(proof.s1.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("d1"), STR_TO_NAN_VAL(proof.d1.to_string()));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("L"), to_v8_array(proof.L));

    Nan::Set(jsonObject, STR_TO_NAN_VAL("R"), to_v8_array(proof.R));

    return jsonObject;
}

/**
 * Mapped methods from bulletproofs.cpp
 */

NAN_METHOD(bulletproofs_prove)
{
    auto result = Nan::New<v8::Array>(3);

    Nan::Set(result, 0, Nan::New(true));

    bool success = false;

    const auto amounts = get_vector<uint64_t>(info, 0);

    const auto blinding_factors = get_vector<crypto_blinding_factor_t>(info, 1);

    auto N = get<uint32_t>(info, 2);

    if (N == 0)
        N = 64;

    if (!amounts.empty() && !blinding_factors.empty())
        try
        {
            const auto [proof, commitments] = TurtleCoinCrypto::RangeProofs::Bulletproofs::prove(amounts, blinding_factors, N);

            Nan::Set(result, 0, Nan::New(false));

            Nan::Set(result, 1, to_v8_object(proof));

            Nan::Set(result, 2, to_v8_array(commitments));
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

NAN_METHOD(bulletproofs_verify)
{
    auto result = Nan::New(false);

    const auto proofs = get_vector<crypto_bulletproof_t>(info, 0);

    const auto commitments = get_vector<std::vector<crypto_pedersen_commitment_t>>(info, 1);

    if (!proofs.empty() && !commitments.empty())
        try
        {
            const auto success = TurtleCoinCrypto::RangeProofs::Bulletproofs::verify(proofs, commitments);

            result = Nan::New(success);
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

/**
 * Mapped methods from bulletproofsplus.cpp
 */

NAN_METHOD(bulletproofsplus_prove)
{
    auto result = Nan::New<v8::Array>(3);

    Nan::Set(result, 0, Nan::New(true));

    bool success = false;

    const auto amounts = get_vector<uint64_t>(info, 0);

    const auto blinding_factors = get_vector<crypto_blinding_factor_t>(info, 1);

    auto N = get<uint32_t>(info, 2);

    if (N == 0)
        N = 64;

    if (!amounts.empty() && !blinding_factors.empty())
        try
        {
            const auto [proof, commitments] =
                TurtleCoinCrypto::RangeProofs::BulletproofsPlus::prove(amounts, blinding_factors, N);

            Nan::Set(result, 0, Nan::New(false));

            Nan::Set(result, 1, to_v8_object(proof));

            Nan::Set(result, 2, to_v8_array(commitments));
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

NAN_METHOD(bulletproofsplus_verify)
{
    auto result = Nan::New(false);

    const auto proofs = get_vector<crypto_bulletproof_plus_t>(info, 0);

    const auto commitments = get_vector<std::vector<crypto_pedersen_commitment_t>>(info, 1);

    if (!proofs.empty() && !commitments.empty())
        try
        {
            const auto success = TurtleCoinCrypto::RangeProofs::BulletproofsPlus::verify(proofs, commitments);

            result = Nan::New(success);
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

/**
 * Mapped methods from crypto_common.cpp
 */

NAN_METHOD(check_point)
{
    const auto point = get<std::string>(info, 0);

    const auto success = TurtleCoinCrypto::check_point(point);

    info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(check_scalar)
{
    const auto scalar = get<std::string>(info, 0);

    const auto success = TurtleCoinCrypto::check_scalar(scalar);

    info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(derivation_to_scalar)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto derivation = get<std::string>(info, 0);

    const auto output_index = get<uint64_t>(info, 1);

    if (!derivation.empty())
        try
        {
            const auto scalar = TurtleCoinCrypto::derivation_to_scalar(derivation, output_index);

            result = STR_TO_NAN_VAL(scalar.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(derive_public_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto derivation_scalar = get<std::string>(info, 0);

    const auto public_key = get<std::string>(info, 1);

    if (!derivation_scalar.empty() && !public_key.empty())
        try
        {
            const auto key = TurtleCoinCrypto::derive_public_key(derivation_scalar, public_key);

            result = STR_TO_NAN_VAL(key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(derive_secret_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto derivation_scalar = get<std::string>(info, 0);

    const auto secret_key = get<std::string>(info, 1);

    if (!derivation_scalar.empty() && !secret_key.empty())
        try
        {
            const auto key = TurtleCoinCrypto::derive_secret_key(derivation_scalar, secret_key);

            result = STR_TO_NAN_VAL(key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_key_derivation)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto public_key = get<std::string>(info, 0);

    const auto secret_key = get<std::string>(info, 1);

    if (!public_key.empty() && !secret_key.empty())
        try
        {
            const auto key = TurtleCoinCrypto::generate_key_derivation(public_key, secret_key);

            result = STR_TO_NAN_VAL(key.to_string());

            success = true;
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_key_image)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto public_key = get<std::string>(info, 0);

    const auto secret_key = get<std::string>(info, 1);

    const auto partial_key_images = get_vector<crypto_key_image_t>(info, 2);

    if (!public_key.empty() && !secret_key.empty())
        try
        {
            const auto key = TurtleCoinCrypto::generate_key_image(public_key, secret_key, partial_key_images);

            result = STR_TO_NAN_VAL(key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_keys)
{
    auto result = Nan::New<v8::Array>(3);

    Nan::Set(result, 0, Nan::New(true));

    try
    {
        const auto [public_key, secret_key] = TurtleCoinCrypto::generate_keys();

        Nan::Set(result, 0, Nan::New(false));

        Nan::Set(result, 1, STR_TO_NAN_VAL(public_key.to_string()));

        Nan::Set(result, 2, STR_TO_NAN_VAL(secret_key.to_string()));
    }
    catch (...)
    {
    }

    info.GetReturnValue().Set(result);
}

NAN_METHOD(generate_subwallet_keys)
{
    auto result = Nan::New<v8::Array>(3);

    Nan::Set(result, 0, Nan::New(true));

    const auto spend_secret_key = get<std::string>(info, 0);

    const auto subwallet_index = get<uint64_t>(info, 1);

    if (!spend_secret_key.empty())
        try
        {
            const auto [public_key, secret_key] = TurtleCoinCrypto::generate_subwallet_keys(spend_secret_key, subwallet_index);

            Nan::Set(result, 0, Nan::New(false));

            Nan::Set(result, 1, STR_TO_NAN_VAL(public_key.to_string()));

            Nan::Set(result, 2, STR_TO_NAN_VAL(secret_key.to_string()));
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

NAN_METHOD(generate_view_from_spend)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto spend_secret_key = get<std::string>(info, 0);

    if (!spend_secret_key.empty())
        try
        {
            const auto view_secret_key = TurtleCoinCrypto::generate_view_from_spend(spend_secret_key);

            result = STR_TO_NAN_VAL(view_secret_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(hash_to_point)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto data = get<std::string>(info, 0);

    if (!data.empty())
        try
        {
            const auto input = TurtleCoinCrypto::StringTools::from_hex(data);

            const auto point = TurtleCoinCrypto::hash_to_point(input.data(), sizeof(input.data()));

            result = STR_TO_NAN_VAL(point.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(hash_to_scalar)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto data = get<std::string>(info, 0);

    if (!data.empty())
        try
        {
            const auto input = TurtleCoinCrypto::StringTools::from_hex(data);

            const auto scalar = TurtleCoinCrypto::hash_to_scalar(input.data(), sizeof(input.data()));

            result = STR_TO_NAN_VAL(scalar.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(pow2_round)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto input = get<uint32_t>(info, 0);

    try
    {
        const auto value = uint32_t(TurtleCoinCrypto::pow2_round(input));

        result = Nan::New(value);

        success = true;
    }
    catch (...)
    {
    }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(random_point)
{
    v8::Local<v8::Value> result = STR_TO_NAN_VAL(TurtleCoinCrypto::random_point().to_string());

    info.GetReturnValue().Set(prepare(true, result));
}

NAN_METHOD(random_points)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto count = get<uint32_t>(info, 0);

    try
    {
        const auto points = TurtleCoinCrypto::random_points(count);

        result = to_v8_array(points);

        success = true;
    }
    catch (...)
    {
    }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(random_scalar)
{
    v8::Local<v8::Value> result = STR_TO_NAN_VAL(TurtleCoinCrypto::random_scalar().to_string());

    info.GetReturnValue().Set(prepare(true, result));
}

NAN_METHOD(random_scalars)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto count = get<uint32_t>(info, 0);

    try
    {
        const auto scalars = TurtleCoinCrypto::random_scalars(count);

        result = to_v8_array(scalars);

        success = true;
    }
    catch (...)
    {
    }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(secret_key_to_public_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto secret_key = get<std::string>(info, 0);

    if (!secret_key.empty())
        try
        {
            const auto public_key = TurtleCoinCrypto::secret_key_to_public_key(secret_key);

            result = STR_TO_NAN_VAL(public_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(underive_public_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto derivation = get<std::string>(info, 0);

    const auto output_index = get<uint64_t>(info, 1);

    const auto public_ephemeral = get<std::string>(info, 2);

    if (!derivation.empty() && !public_ephemeral.empty())
        try
        {
            const auto public_key = TurtleCoinCrypto::underive_public_key(derivation, output_index, public_ephemeral);

            result = STR_TO_NAN_VAL(public_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

/**
 * Mapped methods from hashing.cpp
 */

NAN_METHOD(sha3)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto data = get<std::string>(info, 0);

    if (!data.empty())
        try
        {
            const auto input = TurtleCoinCrypto::StringTools::from_hex(data);

            const auto hash = TurtleCoinCrypto::Hashing::sha3(input.data(), input.size());

            result = STR_TO_NAN_VAL(hash.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(sha3_slow_hash)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto data = get<std::string>(info, 0);

    const auto iterations = get<uint32_t>(info, 1);

    if (!data.empty())
        try
        {
            const auto input = TurtleCoinCrypto::StringTools::from_hex(data);

            const auto hash = TurtleCoinCrypto::Hashing::sha3_slow_hash(input.data(), input.size(), iterations);

            result = STR_TO_NAN_VAL(hash.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(tree_branch)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto hashes = get_vector<crypto_hash_t>(info, 0);

    if (!hashes.empty())
        try
        {
            const auto tree_branches = TurtleCoinCrypto::Hashing::Merkle::tree_branch(hashes);

            result = to_v8_array(tree_branches);

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(tree_depth)
{
    const auto count = get<uint32_t>(info, 0);

    const auto depth = uint32_t(TurtleCoinCrypto::Hashing::Merkle::tree_depth(count));

    info.GetReturnValue().Set(prepare(true, Nan::New(depth)));
}

NAN_METHOD(root_hash)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto hashes = get_vector<crypto_hash_t>(info, 0);

    if (!hashes.empty())
        try
        {
            const auto root_hash = TurtleCoinCrypto::Hashing::Merkle::root_hash(hashes);

            result = STR_TO_NAN_VAL(root_hash.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(root_hash_from_branch)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto hashes = get_vector<crypto_hash_t>(info, 0);

    const auto depth = get<uint32_t>(info, 1);

    const auto leaf = get<std::string>(info, 2);

    const auto path = get<uint8_t>(info, 3);

    if (!hashes.empty() && !leaf.empty() && path <= 1)
        try
        {
            const auto root_hash = TurtleCoinCrypto::Hashing::Merkle::root_hash_from_branch(hashes, depth, leaf, path);

            result = STR_TO_NAN_VAL(root_hash.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

/**
 * Mapped methods from multisig.cpp
 */

NAN_METHOD(generate_multisig_secret_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto their_public_key = get<std::string>(info, 0);

    const auto our_secret_key = get<std::string>(info, 1);

    if (!their_public_key.empty() && !our_secret_key.empty())
        try
        {
            const auto secret_key = TurtleCoinCrypto::Multisig::generate_multisig_secret_key(their_public_key, our_secret_key);

            result = STR_TO_NAN_VAL(secret_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_multisig_secret_keys)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto their_public_keys = get_vector<crypto_public_key_t>(info, 0);

    const auto our_secret_key = get<std::string>(info, 1);

    if (!their_public_keys.empty() && !our_secret_key.empty())
        try
        {
            const auto secret_keys = TurtleCoinCrypto::Multisig::generate_multisig_secret_keys(their_public_keys, our_secret_key);

            result = to_v8_array(secret_keys);

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_shared_public_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto public_keys = get_vector<crypto_public_key_t>(info, 0);

    if (!public_keys.empty())
        try
        {
            const auto public_key = TurtleCoinCrypto::Multisig::generate_shared_public_key(public_keys);

            result = STR_TO_NAN_VAL(public_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_shared_secret_key)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto secret_keys = get_vector<crypto_secret_key_t>(info, 0);

    if (!secret_keys.empty())
        try
        {
            const auto secret_key = TurtleCoinCrypto::Multisig::generate_shared_secret_key(secret_keys);

            result = STR_TO_NAN_VAL(secret_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(rounds_required)
{
    auto result = STR_TO_NAN_VAL("");

    const auto participants = get<uint32_t>(info, 0);

    const auto threshold = get<uint32_t>(info, 1);

    const auto rounds = uint32_t(TurtleCoinCrypto::Multisig::rounds_required(participants, threshold));

    result = Nan::New(rounds);

    info.GetReturnValue().Set(prepare(true, result));
}

/**
 * Mapped methods from ringct.cpp
 */

NAN_METHOD(check_commitments_parity)
{
    bool success = false;

    const auto pseudo_commitments = get_vector<crypto_pedersen_commitment_t>(info, 0);

    const auto output_commitments = get_vector<crypto_pedersen_commitment_t>(info, 1);

    const auto transaction_fee = get<uint64_t>(info, 2);

    try
    {
        success = TurtleCoinCrypto::RingCT::check_commitments_parity(pseudo_commitments, output_commitments, transaction_fee);
    }
    catch (...)
    {
    }

    info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(generate_amount_mask)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto derivation_scalar = get<std::string>(info, 0);

    if (!derivation_scalar.empty())
        try
        {
            const auto amount_mask = TurtleCoinCrypto::RingCT::generate_amount_mask(derivation_scalar);

            result = STR_TO_NAN_VAL(amount_mask.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_commitment_blinding_factor)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto derivation_scalar = get<std::string>(info, 0);

    if (!derivation_scalar.empty())
        try
        {
            const auto blinding_factor = TurtleCoinCrypto::RingCT::generate_commitment_blinding_factor(derivation_scalar);

            result = STR_TO_NAN_VAL(blinding_factor.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_pedersen_commitment)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto blinding_factor = get<std::string>(info, 0);

    const uint64_t amount = get<uint64_t>(info, 1);

    if (!blinding_factor.empty())
        try
        {
            const auto commitment = TurtleCoinCrypto::RingCT::generate_pedersen_commitment(blinding_factor, amount);

            result = STR_TO_NAN_VAL(commitment.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_pseudo_commitments)
{
    v8::Local<v8::Array> result = Nan::New<v8::Array>(3);

    Nan::Set(result, 0, Nan::New(true));

    const auto input_amounts = get_vector<uint64_t>(info, 0);

    const auto output_blinding_factors = get_vector<crypto_blinding_factor_t>(info, 1);

    if (!input_amounts.empty() && !output_blinding_factors.empty())
        try
        {
            const auto [blinding_factors, commitments] =
                TurtleCoinCrypto::RingCT::generate_pseudo_commitments(input_amounts, output_blinding_factors);

            Nan::Set(result, 0, Nan::New(false));

            Nan::Set(result, 1, to_v8_array(blinding_factors));

            Nan::Set(result, 2, to_v8_array(commitments));
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

NAN_METHOD(toggle_masked_amount)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto amount_mask = get<std::string>(info, 0);

    const auto amount_hex = get<std::string>(info, 1);

    const auto amount = get<uint64_t>(info, 1);

    if (!amount_mask.empty())
        try
        {
            if (!amount_hex.empty())
            {
                const auto amount_bytes = TurtleCoinCrypto::StringTools::from_hex(amount_hex);

                const auto masked_amount =
                    TurtleCoinCrypto::RingCT::toggle_masked_amount(amount_mask, amount_bytes).to_uint64_t();

                result = STR_TO_NAN_VAL(TurtleCoinCrypto::StringTools::to_hex(&masked_amount, sizeof(uint64_t)));

                success = true;
            }
            else
            {
                const auto masked_amount = TurtleCoinCrypto::RingCT::toggle_masked_amount(amount_mask, amount).to_uint64_t();

                result = STR_TO_NAN_VAL(TurtleCoinCrypto::StringTools::to_hex(&masked_amount, sizeof(uint64_t)));

                success = true;
            }
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

/**
 * Mapped methods from ring_signature_borromean.cpp
 */

NAN_METHOD(borromean_check_ring_signature)
{
    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto key_image = get<std::string>(info, 1);

    const auto public_keys = get_vector<crypto_public_key_t>(info, 2);

    const auto signature = get_vector<crypto_signature_t>(info, 3);

    if (!message_digest.empty() && !key_image.empty() && !public_keys.empty() && !signature.empty())
        try
        {
            success = TurtleCoinCrypto::RingSignature::Borromean::check_ring_signature(
                message_digest, key_image, public_keys, signature);
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(borromean_complete_ring_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto signing_scalar = get<std::string>(info, 0);

    const auto real_output_index = get<uint32_t>(info, 1);

    const auto signature = get_vector<crypto_signature_t>(info, 2);

    const auto partial_signing_scalars = get_vector<crypto_scalar_t>(info, 3);

    if (!signing_scalar.empty() && !signature.empty())
        try
        {
            const auto [method_success, sigs] = TurtleCoinCrypto::RingSignature::Borromean::complete_ring_signature(
                signing_scalar, real_output_index, signature, partial_signing_scalars);

            if (method_success)
                result = to_v8_array(sigs);

            success = method_success;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(borromean_generate_partial_signing_scalar)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto real_output_index = get<uint32_t>(info, 0);

    const auto signature = get_vector<crypto_signature_t>(info, 1);

    const auto spend_secret_key = get<std::string>(info, 2);

    if (!signature.empty() && !spend_secret_key.empty())
        try
        {
            const auto partial_signing_scalar = TurtleCoinCrypto::RingSignature::Borromean::generate_partial_signing_scalar(
                real_output_index, signature, spend_secret_key);

            result = STR_TO_NAN_VAL(partial_signing_scalar.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(borromean_generate_ring_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto secret_ephemeral = get<std::string>(info, 1);

    const auto public_keys = get_vector<crypto_public_key_t>(info, 2);

    if (!message_digest.empty() && !secret_ephemeral.empty() && !public_keys.empty())
        try
        {
            const auto [method_success, signature] = TurtleCoinCrypto::RingSignature::Borromean::generate_ring_signature(
                message_digest, secret_ephemeral, public_keys);

            if (method_success)
                result = to_v8_array(signature);

            success = method_success;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(borromean_prepare_ring_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto key_image = get<std::string>(info, 1);

    const auto public_keys = get_vector<crypto_public_key_t>(info, 2);

    const auto real_output_index = get<uint32_t>(info, 3);

    if (!message_digest.empty() && !key_image.empty() && !public_keys.empty())
        try
        {
            const auto [method_success, signature] = TurtleCoinCrypto::RingSignature::Borromean::prepare_ring_signature(
                message_digest, key_image, public_keys, real_output_index);

            if (method_success)
                result = to_v8_array(signature);

            success = method_success;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

/**
 * Mapped methods from ring_signature_clsag.cpp
 */

NAN_METHOD(clsag_check_ring_signature)
{
    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto key_image = get<std::string>(info, 1);

    const auto public_keys = get_vector<crypto_public_key_t>(info, 2);

    const auto signature_obj = get_object(info, 3);

    const auto commitments = get_vector<crypto_pedersen_commitment_t>(info, 4);

    const auto pseudo_commitment = get_crypto_t<crypto_pedersen_commitment_t>(info, 5);

    if (!message_digest.empty() && !key_image.empty() && !public_keys.empty())
        try
        {
            crypto_clsag_signature_t signature(
                get_vector<crypto_scalar_t>(signature_obj, "scalars"),
                get_crypto_t<crypto_scalar_t>(signature_obj, "challenge"),
                get_crypto_t<crypto_key_image_t>(signature_obj, "commitment_image"));

            success = TurtleCoinCrypto::RingSignature::CLSAG::check_ring_signature(
                message_digest, key_image, public_keys, signature, commitments, pseudo_commitment);
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(Nan::New(success));
}

NAN_METHOD(clsag_complete_ring_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto signing_scalar = get<std::string>(info, 0);

    const auto real_output_index = get<uint32_t>(info, 1);

    const auto signature_obj = get_object(info, 2);

    const auto h = get_vector<crypto_scalar_t>(info, 3);

    const auto mu_P = get<std::string>(info, 4);

    const auto partial_signing_scalars = get_vector<crypto_scalar_t>(info, 5);

    if (!signing_scalar.empty() && !h.empty() && !mu_P.empty())
        try
        {
            crypto_clsag_signature_t signature(
                get_vector<crypto_scalar_t>(signature_obj, "scalars"),
                get_crypto_t<crypto_scalar_t>(signature_obj, "challenge"),
                get_crypto_t<crypto_key_image_t>(signature_obj, "commitment_image"));

            const auto [method_success, sig] = TurtleCoinCrypto::RingSignature::CLSAG::complete_ring_signature(
                signing_scalar, real_output_index, signature, h, mu_P, partial_signing_scalars);

            if (method_success)
                result = to_v8_object(sig);

            success = method_success;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(clsag_generate_partial_signing_scalar)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto mu_P = get<std::string>(info, 0);

    const auto spend_secret_key = get<std::string>(info, 1);

    if (!mu_P.empty() && !spend_secret_key.empty())
        try
        {
            const auto partial_signing_key =
                TurtleCoinCrypto::RingSignature::CLSAG::generate_partial_signing_scalar(mu_P, spend_secret_key);

            result = STR_TO_NAN_VAL(partial_signing_key.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(clsag_generate_ring_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto secret_ephemeral = get<std::string>(info, 1);

    const auto public_keys = get_vector<crypto_public_key_t>(info, 2);

    const auto input_blinding_factor = get_crypto_t<crypto_blinding_factor_t>(info, 3);

    const auto public_commitments = get_vector<crypto_pedersen_commitment_t>(info, 4);

    const auto pseudo_blinding_factor = get_crypto_t<crypto_blinding_factor_t>(info, 5);

    const auto pseudo_commitment = get_crypto_t<crypto_pedersen_commitment_t>(info, 6);

    if (!message_digest.empty() && !secret_ephemeral.empty() && !public_keys.empty())
        try
        {
            const auto [method_success, signature] = TurtleCoinCrypto::RingSignature::CLSAG::generate_ring_signature(
                message_digest,
                secret_ephemeral,
                public_keys,
                input_blinding_factor,
                public_commitments,
                pseudo_blinding_factor,
                pseudo_commitment);

            if (method_success)
                result = to_v8_object(signature);

            success = method_success;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(clsag_prepare_ring_signature)
{
    v8::Local<v8::Array> result = Nan::New<v8::Array>(4);

    Nan::Set(result, 0, Nan::New(true));

    const auto message_digest = get<std::string>(info, 0);

    const auto key_image = get<std::string>(info, 1);

    const auto public_keys = get_vector<crypto_public_key_t>(info, 2);

    const auto real_output_index = get<uint32_t>(info, 3);

    const auto input_blinding_factor = get_crypto_t<crypto_blinding_factor_t>(info, 4);

    const auto public_commitments = get_vector<crypto_pedersen_commitment_t>(info, 5);

    const auto pseudo_blinding_factor = get_crypto_t<crypto_blinding_factor_t>(info, 6);

    const auto pseudo_commitment = get_crypto_t<crypto_pedersen_commitment_t>(info, 7);

    if (!message_digest.empty() && !key_image.empty() && !public_keys.empty())
        try
        {
            const auto [method_success, signature, h, mu_P] = TurtleCoinCrypto::RingSignature::CLSAG::prepare_ring_signature(
                message_digest,
                key_image,
                public_keys,
                real_output_index,
                input_blinding_factor,
                public_commitments,
                pseudo_blinding_factor,
                pseudo_commitment);

            if (method_success)
            {
                Nan::Set(result, 0, Nan::New(false));

                Nan::Set(result, 1, to_v8_object(signature));

                Nan::Set(result, 2, to_v8_array(h));

                Nan::Set(result, 3, STR_TO_NAN_VAL(mu_P.to_string()));
            }
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

/**
 * Mapped methods from signature.cpp
 */

NAN_METHOD(check_signature)
{
    auto result = Nan::New(false);

    const auto message_digest = get<std::string>(info, 0);

    const auto public_key = get<std::string>(info, 1);

    const auto signature = get<std::string>(info, 2);

    if (!message_digest.empty() && !public_key.empty() && !signature.empty())
        try
        {
            const auto success = TurtleCoinCrypto::Signature::check_signature(message_digest, public_key, signature);

            result = Nan::New(success);
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(result);
}

NAN_METHOD(complete_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto signing_scalar = get<std::string>(info, 0);

    const auto signature = get<std::string>(info, 1);

    const auto partial_signing_scalars = get_vector<crypto_scalar_t>(info, 2);

    if (!signing_scalar.empty() && !signature.empty())
        try
        {
            const auto sig = TurtleCoinCrypto::Signature::complete_signature(signing_scalar, signature, partial_signing_scalars);

            result = STR_TO_NAN_VAL(sig.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_partial_signing_scalar)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto signature = get<std::string>(info, 0);

    const auto spend_secret_key = get<std::string>(info, 1);

    if (!signature.empty() && !spend_secret_key.empty())
        try
        {
            const auto partial_signing_scalar =
                TurtleCoinCrypto::Signature::generate_partial_signing_scalar(signature, spend_secret_key);

            result = STR_TO_NAN_VAL(partial_signing_scalar.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(generate_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto secret_key = get<std::string>(info, 1);

    if (!message_digest.empty() && !secret_key.empty())
        try
        {
            const auto signature = TurtleCoinCrypto::Signature::generate_signature(message_digest, secret_key);

            result = STR_TO_NAN_VAL(signature.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_METHOD(prepare_signature)
{
    auto result = STR_TO_NAN_VAL("");

    bool success = false;

    const auto message_digest = get<std::string>(info, 0);

    const auto public_key = get<std::string>(info, 1);

    if (!message_digest.empty() && !public_key.empty())
        try
        {
            const auto signature = TurtleCoinCrypto::Signature::prepare_signature(message_digest, public_key);

            result = STR_TO_NAN_VAL(signature.to_string());

            success = true;
        }
        catch (...)
        {
        }

    info.GetReturnValue().Set(prepare(success, result));
}

NAN_MODULE_INIT(InitModule)
{
    // Mapped methods from bulletproofs.cpp
    {
        NAN_EXPORT(target, bulletproofs_prove);

        NAN_EXPORT(target, bulletproofs_verify);
    }

    // Mapped methods from bulletproofsplus.cpp
    {
        NAN_EXPORT(target, bulletproofsplus_prove);

        NAN_EXPORT(target, bulletproofsplus_verify);
    }

    // Mapped methods from crypto_common.cpp
    {
        NAN_EXPORT(target, check_point);

        NAN_EXPORT(target, check_scalar);

        NAN_EXPORT(target, derivation_to_scalar);

        NAN_EXPORT(target, derive_public_key);

        NAN_EXPORT(target, derive_secret_key);

        NAN_EXPORT(target, generate_key_derivation);

        NAN_EXPORT(target, generate_key_image);

        NAN_EXPORT(target, generate_keys);

        NAN_EXPORT(target, generate_subwallet_keys);

        NAN_EXPORT(target, generate_view_from_spend);

        NAN_EXPORT(target, hash_to_point);

        NAN_EXPORT(target, hash_to_scalar);

        NAN_EXPORT(target, pow2_round);

        NAN_EXPORT(target, random_point);

        NAN_EXPORT(target, random_points);

        NAN_EXPORT(target, random_scalar);

        NAN_EXPORT(target, random_scalars);

        NAN_EXPORT(target, secret_key_to_public_key);

        NAN_EXPORT(target, underive_public_key);
    }

    // Mapped methods from hashing.cpp
    {
        NAN_EXPORT(target, sha3);

        NAN_EXPORT(target, sha3_slow_hash);

        NAN_EXPORT(target, tree_branch);

        NAN_EXPORT(target, tree_depth);

        NAN_EXPORT(target, root_hash);

        NAN_EXPORT(target, root_hash_from_branch);
    }

    // Mapped methods from multisig.cpp
    {
        NAN_EXPORT(target, generate_multisig_secret_key);

        NAN_EXPORT(target, generate_multisig_secret_keys);

        NAN_EXPORT(target, generate_shared_public_key);

        NAN_EXPORT(target, generate_shared_secret_key);

        NAN_EXPORT(target, rounds_required);
    }

    // Mapped methods from ringct.cpp
    {
        NAN_EXPORT(target, check_commitments_parity);

        NAN_EXPORT(target, generate_amount_mask);

        NAN_EXPORT(target, generate_commitment_blinding_factor);

        NAN_EXPORT(target, generate_pedersen_commitment);

        NAN_EXPORT(target, generate_pseudo_commitments);

        NAN_EXPORT(target, toggle_masked_amount);
    }

    // Mapped methods from ring_signature_borromean.cpp
    {
        NAN_EXPORT(target, borromean_check_ring_signature);

        NAN_EXPORT(target, borromean_complete_ring_signature);

        NAN_EXPORT(target, borromean_generate_partial_signing_scalar);

        NAN_EXPORT(target, borromean_generate_ring_signature);

        NAN_EXPORT(target, borromean_prepare_ring_signature);
    }

    // Mapped methods from ring_signature_clsag.cpp
    {
        NAN_EXPORT(target, clsag_check_ring_signature);

        NAN_EXPORT(target, clsag_complete_ring_signature);

        NAN_EXPORT(target, clsag_generate_partial_signing_scalar);

        NAN_EXPORT(target, clsag_generate_ring_signature);

        NAN_EXPORT(target, clsag_prepare_ring_signature);
    }

    // Mapped methods from signature.cpp
    {
        NAN_EXPORT(target, check_signature);

        NAN_EXPORT(target, complete_signature);

        NAN_EXPORT(target, generate_partial_signing_scalar);

        NAN_EXPORT(target, generate_signature);

        NAN_EXPORT(target, prepare_signature);
    }
}

NODE_MODULE(cryptography, InitModule);
