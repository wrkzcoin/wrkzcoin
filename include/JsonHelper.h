// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "json.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

/* Type aliases so existing code that uses JSONObject / JSONValue continues to compile */
using JSONObject = nlohmann::json;
using JSONValue  = nlohmann::json;

template<typename T> bool hasMember(const T &j, const std::string &key)
{
    return j.contains(key);
}

template<typename T> const nlohmann::json &getJsonValue(const T &j, const std::string &key)
{
    if (!j.contains(key))
    {
        throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
    }
    return j.at(key);
}

template<typename T> uint64_t getUint64FromJSON(const T &j, const std::string &key)
{
    return getJsonValue(j, key).template get<uint64_t>();
}

template<typename T> int64_t getInt64FromJSON(const T &j, const std::string &key)
{
    return getJsonValue(j, key).template get<int64_t>();
}

template<typename T> std::string getStringFromJSON(const T &j, const std::string &key)
{
    return getJsonValue(j, key).template get<std::string>();
}

/* Gets a string from a JSON value directly (e.g. an element of a string array) */
template<typename T> std::string getStringFromJSONString(const T &j)
{
    return j.template get<std::string>();
}

template<typename T> const nlohmann::json &getArrayFromJSON(const T &j, const std::string &key)
{
    return getJsonValue(j, key);
}

template<typename T> const nlohmann::json &getObjectFromJSON(const T &j, const std::string &key)
{
    return getJsonValue(j, key);
}

template<typename T> bool getBoolFromJSON(const T &j, const std::string &key)
{
    return getJsonValue(j, key).template get<bool>();
}
