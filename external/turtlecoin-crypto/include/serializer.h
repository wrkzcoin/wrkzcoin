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

#ifndef CRYPTO_SERIALIZER_H
#define CRYPTO_SERIALIZER_H

#include "string_tools.h"

#include <cmath>

namespace SerializerTools
{
    /**
     * Packs the provided value into a byte vector
     * @tparam T
     * @param value
     * @return
     */
    template<typename T> static inline std::vector<uint8_t> pack(const T &value)
    {
        uint8_t bytes[64] = {0};

        std::memmove(&bytes, &value, sizeof(T));

        return std::vector<uint8_t>(bytes, bytes + sizeof(T));
    }

    /**
     * Unpacks a value from the provided byte vector starting at the given offset
     * @tparam T
     * @param packed
     * @param offset
     * @return
     */
    template<typename T> static inline T unpack(const std::vector<uint8_t> &packed, size_t offset = 0)
    {
        const auto size = sizeof(T);

        if (offset + size > packed.size())
            throw std::range_error("not enough data to complete request");

        uint8_t bytes[64] = {0};

        for (size_t i = offset, j = 0; i < offset + size; ++i, ++j)
            bytes[j] = packed[i];

        T value = 0;

        std::memmove(&value, &bytes, sizeof(T));

        return value;
    }

    /**
     * Encodes a value into a varint byte vector
     * @tparam T
     * @param value
     * @return
     */
    template<typename T> static inline std::vector<uint8_t> encode_varint(const T &value)
    {
        const auto max_length = sizeof(T) + 2;

        std::vector<uint8_t> output;

        T val = value;

        while (val >= 0x80)
        {
            if (output.size() == (max_length - 1))
                throw std::range_error("value is out of range for type");

            output.push_back((static_cast<char>(val) & 0x7f) | 0x80);

            val >>= 7;
        }

        output.push_back(static_cast<char>(val));

        return output;
    }

    /**
     * Decodes a value from the provided varint byte vector starting at the given offset
     * @tparam T
     * @param packed
     * @param offset
     * @return
     */
    template<typename T>
    static inline std::tuple<T, unsigned int> decode_varint(const std::vector<uint8_t> &packed, const size_t offset = 0)
    {
        if (offset > packed.size())
            throw std::range_error("offset exceeds sizes of vector");

        auto counter = offset;

        auto shift = 0;

        uint64_t temp_result = 0;

        unsigned char b;

        do
        {
            if (counter >= packed.size())
                throw std::range_error("could not decode varint");

            b = packed[counter++];

            const auto value = (shift < 28) ? (b & 0x7f) << shift : (b & 0x7f) * (uint64_t(1) << shift);

            temp_result += value;

            shift += 7;
        } while (b >= 0x80);

        const auto result = T(temp_result);

        if (result != temp_result)
            throw std::range_error("value is out of range for type");

        return {result, counter - offset};
    }
} // namespace SerializerTools

typedef struct Serializer
{
    Serializer() {}

    Serializer(const std::vector<uint8_t> &input)
    {
        buffer = input;
    }

    unsigned char &operator[](int i)
    {
        return buffer[i];
    }

    unsigned char operator[](int i) const
    {
        return buffer[i];
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void boolean(const bool value)
    {
        if (value)
            buffer.push_back(1);
        else
            buffer.push_back(0);
    }

    /**
     * Encodes the value into the vector
     * @param data
     * @param length
     */
    void bytes(const void *data, size_t length)
    {
        auto const *raw = static_cast<uint8_t const *>(data);

        for (size_t i = 0; i < length; ++i)
            buffer.push_back(raw[i]);
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void bytes(const std::vector<uint8_t> &value)
    {
        extend(value);
    }

    /**
     * Returns a pointer to the underlying structure data
     * @return
     */
    const uint8_t *data() const
    {
        return buffer.data();
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void hex(const std::string &value)
    {
        const auto bytes = TurtleCoinCrypto::StringTools::from_hex(value);

        extend(bytes);
    }

    /**
     * Encodes the value into the vector
     * @tparam T
     * @param value
     */
    template<typename T> void key(const T &value)
    {
        for (size_t i = 0; i < value.size(); ++i)
            buffer.push_back(value[i]);
    }

    /**
     * Clears the underlying byte vector
     */
    void reset()
    {
        buffer.clear();
    }

    /**
     * Use this method instead of sizeof() to get the resulting
     * size of the structure in bytes
     * @return
     */
    size_t const size() const
    {
        return buffer.size();
    }

    /**
     * Returns the hex encoding of the underlying byte vector
     * @return
     */
    std::string to_string() const
    {
        return TurtleCoinCrypto::StringTools::to_hex(buffer.data(), buffer.size());
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void uint8(const uint8_t &value)
    {
        buffer.push_back(value);
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void uint16(const uint16_t &value)
    {
        const auto packed = SerializerTools::pack(value);

        extend(packed);
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void uint32(const uint32_t &value)
    {
        const auto packed = SerializerTools::pack(value);

        extend(packed);
    }

    /**
     * Encodes the value into the vector
     * @param value
     */
    void uint64(const uint64_t &value)
    {
        const auto packed = SerializerTools::pack(value);

        extend(packed);
    }

    /**
     * Encodes the value into the vector as a varint
     * @tparam T
     * @param value
     */
    template<typename T> void varint(const T &value)
    {
        const auto bytes = SerializerTools::encode_varint(value);

        extend(bytes);
    }

    /**
     * Returns a copy of the underlying vector
     * @return
     */
    std::vector<uint8_t> vector() const
    {
        return buffer;
    }

  private:
    std::vector<uint8_t> buffer;

    void extend(const std::vector<uint8_t> &vector)
    {
        for (const auto &element : vector)
            buffer.push_back(element);
    }
} serializer_t;

typedef struct DeSerializer
{
    DeSerializer(std::initializer_list<uint8_t> input)
    {
        buffer = std::vector<uint8_t>(input.begin(), input.end());
    }

    DeSerializer(const std::vector<uint8_t> &input)
    {
        buffer = input;
    }

    DeSerializer(const std::string &input)
    {
        buffer = TurtleCoinCrypto::StringTools::from_hex(input);
    }

    /**
     * Decodes a value from the byte vector
     * @param peek
     * @return
     */
    bool boolean(bool peek = false)
    {
        return uint8(peek) == 1;
    }

    /**
     * Returns a byte vector of the given length from the byte vector
     * @param count
     * @param peek
     * @return
     */
    std::vector<uint8_t> bytes(size_t count = 1, bool peek = false)
    {
        const auto start = offset;

        if (!peek)
            offset += count;

        return std::vector<uint8_t>(buffer.begin() + start, buffer.begin() + start + count);
    }

    /**
     * Trims read dead from the byte vector thus reducing its memory footprint
     */
    void compact()
    {
        buffer = std::vector<uint8_t>(buffer.begin() + offset, buffer.end());
    }

    /**
     * Returns a pointer to the underlying structure data
     * @return
     */
    const uint8_t *data() const
    {
        return buffer.data();
    }

    /**
     * Decodes a hex encoded string of the given length from the byte vector
     * @param length
     * @param peek
     * @return
     */
    std::string hex(size_t length = 1, bool peek = false)
    {
        const auto temp = bytes(length, peek);

        return TurtleCoinCrypto::StringTools::to_hex(temp.data(), temp.size());
    }

    /**
     * Decodes a value from the byte vector
     * @tparam T
     * @param peek
     * @return
     */
    template<typename T> T key(bool peek = false)
    {
        T result;

        result = bytes(result.size(), peek);

        return result;
    }

    /**
     * Resets the reader to the given position (default 0)
     * @param position
     */
    void reset(size_t position = 0)
    {
        offset = position;
    }

    /**
     * Use this method instead of sizeof() to get the resulting
     * size of the structure in bytes
     * @return
     */
    size_t const size() const
    {
        return buffer.size();
    }

    /**
     * Skips the next specified bytes while reading
     * @param count
     */
    void skip(size_t count = 1)
    {
        offset += count;
    }

    /**
     * Returns the hex encoding of the underlying byte vector
     * @return
     */
    std::string to_string() const
    {
        return TurtleCoinCrypto::StringTools::to_hex(buffer.data(), buffer.size());
    }

    /**
     * Decodes a value from the byte vector
     * @param peek
     * @return
     */
    uint8_t uint8(bool peek = false)
    {
        const auto start = offset;

        if (!peek)
            offset += sizeof(uint8_t);

        return SerializerTools::unpack<uint8_t>(buffer, start);
    }

    /**
     * Decodes a value from the byte vector
     * @param peek
     * @return
     */
    uint16_t uint16(bool peek = false)
    {
        const auto start = offset;

        if (!peek)
            offset += sizeof(uint16_t);

        return SerializerTools::unpack<uint16_t>(buffer, start);
    }

    /**
     * Decodes a value from the byte vector
     * @param peek
     * @return
     */
    uint32_t uint32(bool peek = false)
    {
        const auto start = offset;

        if (!peek)
            offset += sizeof(uint32_t);

        return SerializerTools::unpack<uint32_t>(buffer, start);
    }

    /**
     * Decodes a value from the byte vector
     * @param peek
     * @return
     */
    uint64_t uint64(bool peek = false)
    {
        const auto start = offset;

        if (!peek)
            offset += sizeof(uint64_t);

        return SerializerTools::unpack<uint64_t>(buffer, start);
    }

    /**
     * Decodes a value from the byte vector
     * @tparam T
     * @param peek
     * @return
     */
    template<typename T> T varint(bool peek = false)
    {
        const auto start = offset;

        const auto [result, length] = SerializerTools::decode_varint<T>(buffer, start);

        if (!peek)
            offset += length;

        return result;
    }

    /**
     * Returns the remaining number of bytes that have not been read from the byte vector
     * @return
     */
    size_t unread_bytes() const
    {
        const auto unread = buffer.size() - offset;

        return unread >= 0 ? unread : 0;
    }

    /**
     * Returns a byte vector copy of the remaining number of bytes that have not been read from the byte vector
     * @return
     */
    std::vector<uint8_t> unread_data() const
    {
        return std::vector<uint8_t>(buffer.begin() + offset, buffer.end());
    }

  private:
    std::vector<uint8_t> buffer;

    size_t offset = 0;
} deserializer_t;

#endif // CRYPTO_SERIALIZER_H
