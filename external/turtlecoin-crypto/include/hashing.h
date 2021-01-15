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

#ifndef CRYPTO_HASHING_H
#define CRYPTO_HASHING_H

#include "serializer.h"
#include "string_tools.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sha3/include/sha3.h>
#include <stdexcept>
#include <string>

/**
 * A structure representing a 256-bit hash value
 */
typedef struct Hash
{
    Hash() {}

    Hash(std::initializer_list<uint8_t> input)
    {
        std::copy(input.begin(), input.end(), std::begin(bytes));
    }

    Hash(const uint8_t input[32])
    {
        std::copy(input, input + sizeof(bytes), std::begin(bytes));
    }

    Hash(const std::string &s)
    {
        from_string(s);
    }

    Hash(const JSONValue &j)
    {
        if (!j.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());
    }

    Hash(const JSONValue &j, const std::string &key)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());
    }

    unsigned char &operator[](int i)
    {
        return bytes[i];
    }

    unsigned char operator[](int i) const
    {
        return bytes[i];
    }

    bool operator==(const Hash &other) const
    {
        return std::equal(std::begin(bytes), std::end(bytes), std::begin(other.bytes));
    }

    bool operator!=(const Hash &other) const
    {
        return !(*this == other);
    }

    bool operator<(const Hash &other) const
    {
        for (size_t i = 31; i >= 0; --i)
        {
            if (bytes[i] < other.bytes[i])
                return true;

            if (bytes[i] > other.bytes[i])
                return false;
        }

        return false;
    }

    bool operator>(const Hash &other) const
    {
        for (size_t i = 31; i >= 0; --i)
        {
            if (bytes[i] > other.bytes[i])
                return true;

            if (bytes[i] < other.bytes[i])
                return false;
        }

        return false;
    }

    /**
     * Returns a pointer to the underlying structure data
     * @return
     */
    [[nodiscard]] const uint8_t *data() const
    {
        return bytes;
    }

    /**
     * Serializes the struct to a byte array
     * @return
     */
    std::vector<uint8_t> serialize() const
    {
        serializer_t writer;

        writer.bytes(&bytes, sizeof(bytes));

        return writer.vector();
    }

    /**
     * Use this method instead of sizeof(Hash) to get the resulting
     * size of the key in bytes
     * @return
     */
    [[nodiscard]] size_t size() const
    {
        return sizeof(bytes);
    }

    /**
     * Converts the structure to a JSON object
     * @param writer
     */
    void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
    {
        writer.String(to_string());
    }

    /**
     * Encodes a hash as a hexadecimal string
     * @return
     */
    [[nodiscard]] std::string to_string() const
    {
        return TurtleCoinCrypto::StringTools::to_hex(bytes, sizeof(bytes));
    }

    uint8_t bytes[32] = {0};

  private:
    void from_string(const std::string &s)
    {
        const auto input = TurtleCoinCrypto::StringTools::from_hex(s);

        if (input.size() < size())
            throw std::runtime_error("Could not load scalar");

        std::copy(input.begin(), input.end(), std::begin(bytes));
    }
} crypto_hash_t;

namespace std
{
    inline ostream &operator<<(ostream &os, const crypto_hash_t &value)
    {
        os << value.to_string();

        return os;
    }
} // namespace std

namespace TurtleCoinCrypto::Hashing
{
    namespace Merkle
    {
        /**
         * Generates the merkle tree branches for the given set of hashes
         * @param hashes
         */
        std::vector<crypto_hash_t> tree_branch(const std::vector<crypto_hash_t> &hashes);

        /**
         * Calculates the depth of the merkle tree based on the count of elements
         * @param count
         * @return
         */
        size_t tree_depth(size_t count);

        /**
         * Generates the merkle root hash for the given set of hashes
         * @param hashes
         * @param root_hash
         */
        crypto_hash_t root_hash(const std::vector<crypto_hash_t> &hashes);

        /**
         * Generates the merkle root hash from the given set of merkle branches and the supplied leaf
         * following the provided path (0 or 1)
         * @param branches
         * @param depth
         * @param leaf
         * @param root_hash
         * @param path
         */
        crypto_hash_t root_hash_from_branch(
            const std::vector<crypto_hash_t> &branches,
            size_t depth,
            const crypto_hash_t &leaf,
            const uint8_t &path = 0);
    } // namespace Merkle

    /**
     * Hashes the given input data using SHA-3 into a 256-bit hash
     * @param input
     * @param length
     * @return
     */
    crypto_hash_t sha3(const void *input, size_t length);

    /**
     * Hashes the given vector of data using SHA-3 into a 256-bit hash
     * @param input
     * @return
     */
    template<typename T> crypto_hash_t sha3(const std::vector<T> &input)
    {
        return sha3(input.data(), input.size() * sizeof(T));
    }

    /**
     * Hashes the given input data using SHA-3 into a 256-bit hash
     * @param input
     * @return
     */
    template<typename T> crypto_hash_t sha3(const T &input)
    {
        return sha3(input.data(), input.size());
    }

    /**
     * Hashes the given input using SHA-3 for the number of rounds indicated by iterations
     * this method also performs basic key stretching whereby the input data is appended
     * to the resulting hash each round to "salt" each round of hashing to prevent simply
     * iterating the hash over itself
     * @param input
     * @param length
     * @param iterations
     * @return
     */
    crypto_hash_t sha3_slow_hash(const void *input, size_t length, uint64_t iterations);

    /**
     * Hashes the given POD using SHA-3 for the number of rounds indicated by iterations
     * this method also performs basic key stretching whereby the input data is appended
     * to the resulting hash each round to "salt" each round of hashing to prevent simply
     * iterating the hash over itself
     * @param input
     * @param iterations
     * @return
     */
    template<typename T> crypto_hash_t sha3_slow_hash(const T &input, uint64_t iterations = 0)
    {
        return sha3_slow_hash(input.data(), input.size(), iterations);
    }
} // namespace TurtleCoinCrypto::Hashing

#endif // CRYPTO_HASHING_H
