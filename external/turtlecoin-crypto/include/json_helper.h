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
// Adapted from code by Zpalmtree found at
// https://github.com/turtlecoin/turtlecoin/blob/cab5c65d243878dc0c9e1ac964614e8e431b6c77/include/JsonHelper.h

#ifndef CRYPTO_JSON_HELPER_H
#define CRYPTO_JSON_HELPER_H

#include <rapidjson/document.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stdexcept>

typedef rapidjson::GenericObject<
    true,
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>>
    JSONObject;

typedef rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>
    JSONValue;

static const std::string kTypeNamesJH[] = {"Null", "False", "True", "Object", "Array", "String", "Number", "Double"};

/**
 * Checks if the specified property is in the value/document provided
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> bool has_member(const T &j, const std::string &key)
{
    const auto val = j.FindMember(key);

    return val != j.MemberEnd();
}

/**
 * Retrieves the value at the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> const rapidjson::Value &get_json_value(const T &j, const std::string &key)
{
    const auto val = j.FindMember(key);

    if (val == j.MemberEnd())
        throw std::invalid_argument("Missing JSON parameter: '" + key + "'");

    return val->value;
}

/**
 * Retrieves a boolean from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> bool get_json_bool(const T &j)
{
    if (!j.IsBool())
        throw std::invalid_argument("JSON parameter is wrong type. Expected bool, got " + kTypeNamesJH[j.GetType()]);

    return j.GetBool();
}

/**
 * Retrieves a boolean from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> bool get_json_bool(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_bool(val);
}

/**
 * Retrieves an int64_t from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> int64_t get_json_int64_t(const T &j)
{
    if (!j.IsInt64())
        throw std::invalid_argument("JSON parameter is wrong type. Expected int64_t, got " + kTypeNamesJH[j.GetType()]);

    return j.GetInt64();
}

/**
 * Retrieves an int64_t from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> int64_t get_json_int64_t(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_int64_t(val);
}

/**
 * Retrieves an uint64_t from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> uint64_t get_json_uint64_t(const T &j)
{
    if (!j.IsUint64())
        throw std::invalid_argument("JSON parameter is wrong type. Expected uint64_t, got " + kTypeNamesJH[j.GetType()]);

    return j.GetUint64();
}

/**
 * Retrieves an uint64_t from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> uint64_t get_json_uint64_t(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_uint64_t(val);
}

/**
 * Retrieves an uint32_t from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> uint32_t get_json_uint32_t(const T &j)
{
    if (!j.IsUint())
        throw std::invalid_argument("JSON parameter is wrong type. Expected uint32_t, got " + kTypeNamesJH[j.GetType()]);

    return j.GetUint();
}

/**
 * Retrieves an uint32_t from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> uint32_t get_json_uint32_t(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_uint32_t(val);
}

/**
 * Retrieves a double from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> double get_json_double(const T &j)
{
    if (!j.IsDouble())
        throw std::invalid_argument("JSON parameter is wrong type. Expected double, got " + kTypeNamesJH[j.GetType()]);

    return j.GetDouble();
}

/**
 * Retrieves a double from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> double get_json_double(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_double(val);
}

/**
 * Retrieves a std::string from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> std::string get_json_string(const T &j)
{
    if (!j.IsString())
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected std::string, got " + kTypeNamesJH[j.GetType()]);

    return j.GetString();
}

/**
 * Retrieves a std::string from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> std::string get_json_string(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_string(val);
}

/**
 * Retrieves an array from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> auto get_json_array(const T &j)
{
    if (!j.IsArray())
        throw std::invalid_argument("JSON parameter is wrong type. Expected Array, got " + kTypeNamesJH[j.GetType()]);

    return j.GetArray();
}

/**
 * Retrieves an array from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> auto get_json_array(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_array(val);
}

/**
 * Retrieves an object from the given value
 * @tparam T
 * @param j
 * @return
 */
template<typename T> JSONObject get_json_object(const T &j)
{
    if (!j.IsObject())
        throw std::invalid_argument("JSON parameter is wrong type. Expected Object, got " + kTypeNamesJH[j.GetType()]);

    return j.Get_Object();
}

/**
 * Retrieves an object from the value in the given property
 * @tparam T
 * @param j
 * @param key
 * @return
 */
template<typename T> JSONObject get_json_object(const T &j, const std::string &key)
{
    const auto &val = get_json_value(j, key);

    return get_json_object(val);
}

#endif // CRYPTO_JSON_HELPER_H
