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

#ifndef CRYPTO_TYPES_H
#define CRYPTO_TYPES_H

#include "serializer.h"

#include <cstring>
#include <ed25519/include/ed25519.h>
#include <stdexcept>
#include <string_tools.h>

/**
 * l = 2^252 + 2774231777737235353585193779
 */
static const uint8_t l[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                              0xa2, 0xde, 0xf9, 0xde, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};

/**
 * q = 2^255 - 19
 * Value is provided here for reference purposes
 */
static const uint8_t q[32] = {0xeD, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                              0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};

typedef struct EllipticCurvePoint
{
    /**
     * Various constructor methods for creating a point. All of the methods
     * will load the various types, then automatically load the related
     * ge_p2 and ge_p2 points into cached memory to help speed up operations
     * that use them later without incurring the cost of loading them from bytes
     * again. While this uses a bit more memory to represent a point, it does
     * provide us with a more performant experience when conducting arithmetic
     * operations using the point
     */

    EllipticCurvePoint()
    {
        ge_frombytes_negate_vartime(&point3, bytes);
    }

    EllipticCurvePoint(std::initializer_list<uint8_t> input)
    {
        std::copy(input.begin(), input.end(), std::begin(bytes));

        if (ge_frombytes_negate_vartime(&point3, bytes) != 0)
            throw std::runtime_error("could not load point");

        ge_p3_to_cached(&cached_point, &point3);
    }

    EllipticCurvePoint(const uint8_t input[32])
    {
        std::copy(input, input + sizeof(bytes), std::begin(bytes));

        if (ge_frombytes_negate_vartime(&point3, bytes) != 0)
            throw std::runtime_error("could not load point");

        ge_p3_to_cached(&cached_point, &point3);
    }

    EllipticCurvePoint(const size_t &number)
    {
        std::memmove(bytes, &number, sizeof(number));

        if (ge_frombytes_negate_vartime(&point3, bytes) != 0)
            throw std::runtime_error("could not load point");

        ge_p3_to_cached(&cached_point, &point3);
    }

    EllipticCurvePoint(const std::vector<uint8_t> &input)
    {
        if (input.size() < size())
            throw std::runtime_error("could not load point");

        std::copy(input.begin(), input.end(), std::begin(bytes));

        if (ge_frombytes_negate_vartime(&point3, bytes) != 0)
            throw std::runtime_error("could not load point");

        ge_p3_to_cached(&cached_point, &point3);
    }

    EllipticCurvePoint(const ge_p3 &point): point3(point)
    {
        ge_p3_tobytes(bytes, &point);

        ge_p3_to_cached(&cached_point, &point3);
    }

    EllipticCurvePoint(const std::string &s)
    {
        from_string(s);
    }

    EllipticCurvePoint(const JSONValue &j)
    {
        if (!j.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());
    }

    EllipticCurvePoint(const JSONValue &j, const std::string &key)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());
    }

    /**
     * Allows us to check a random value to determine if it is a scalar or not
     * @param value
     * @return
     */
    template<typename T> static bool check(const T &value)
    {
        /**
         * Try loading the given value into a point type and then check to see if the bytes
         * that we have loaded are actually a point. If we fail at any point, then it
         * definitely is not a point that was provided.
         */
        try
        {
            EllipticCurvePoint check_value(value);

            return check_value.check();
        }
        catch (...)
        {
            return false;
        }
    }

    /**
     * Overloading a bunch of the standard operators to make operations using this
     * structure to use a lot cleaner syntactic sugar in downstream code.
     */

    unsigned char operator[](int i) const
    {
        return bytes[i];
    }

    bool operator==(const EllipticCurvePoint &other) const
    {
        return std::equal(std::begin(bytes), std::end(bytes), std::begin(other.bytes));
    }

    bool operator!=(const EllipticCurvePoint &other) const
    {
        return !(*this == other);
    }

    bool operator<(const EllipticCurvePoint &other) const
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

    bool operator<=(const EllipticCurvePoint &other) const
    {
        return (*this == other) || (*this < other);
    }

    bool operator>(const EllipticCurvePoint &other) const
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

    bool operator>=(const EllipticCurvePoint &other) const
    {
        return (*this == other) || (*this > other);
    }

    EllipticCurvePoint operator+(const EllipticCurvePoint &other) const
    {
        ge_p1p1 tmp2;

        // AB = (a + b) mod l
        ge_add(&tmp2, &point3, &other.cached_point);

        ge_p3 final;

        ge_p1p1_to_p3(&final, &tmp2);

        return EllipticCurvePoint(final);
    }

    void operator+=(const EllipticCurvePoint &other)
    {
        *this = *this + other;
    }

    EllipticCurvePoint operator-(const EllipticCurvePoint &other) const
    {
        ge_p1p1 tmp2;

        // AB = (a - b) mod l
        ge_sub(&tmp2, &point3, &other.cached_point);

        ge_p3 final;

        ge_p1p1_to_p3(&final, &tmp2);

        return EllipticCurvePoint(final);
    }

    EllipticCurvePoint operator-() const
    {
        EllipticCurvePoint other({1}); // Z = (0, 1)

        return other - *this;
    }

    void operator-=(const EllipticCurvePoint &other)
    {
        *this = *this - other;
    }

    /**
     * Member methods used in general operations using scalars
     */

    /**
     * Returns a pointer to a ge_cached representation of the point
     * @return
     */
    const ge_cached cached() const
    {
        return cached_point;
    }

    /**
     * Checks to confirm that the point is indeed a point
     * @return
     */
    bool check() const
    {
        ge_p3 tmp;

        return ge_frombytes_negate_vartime(&tmp, bytes) == 0;
    }

    /**
     * Checks to confirm that the point is in our subgroup
     * @return
     */
    bool check_subgroup() const
    {
        ge_dsmp tmp;

        ge_dsm_precomp(tmp, &point3);

        return ge_check_subgroup_precomp_negate_vartime(tmp) == 0;
    }

    /**
     * Returns a pointer to the underlying structure data
     * @return
     */
    const uint8_t *data() const
    {
        return bytes;
    }

    /**
     * Computes 8P
     * @return
     */
    EllipticCurvePoint mul8() const
    {
        ge_p1p1 tmp;

        ge_p2 point2;

        ge_p3_to_p2(&point2, &point3);

        ge_mul8(&tmp, &point2);

        ge_p3 tmp2;

        ge_p1p1_to_p3(&tmp2, &tmp);

        return EllipticCurvePoint(tmp2);
    }

    /**
     * Returns the negation of the point
     * @return
     */
    EllipticCurvePoint negate() const
    {
        EllipticCurvePoint Z({0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});

        return Z - *this;
    }

    /**
     * Returns a pointer to a ge_p3 representation of the point
     * @return
     */
    const ge_p3 p3() const
    {
        return point3;
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
     * Use this method instead of sizeof(EllipticCurvePoint) to get the resulting
     * size of the key in bytes
     * @return
     */
    size_t size() const
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
     * Encodes the point as a hexadecimal string
     * @return
     */
    std::string to_string() const
    {
        return TurtleCoinCrypto::StringTools::to_hex(bytes, sizeof(bytes));
    }

  private:
    /**
     * Loads the point from a hexademical string
     * @param s
     */
    void from_string(const std::string &s)
    {
        const auto input = TurtleCoinCrypto::StringTools::from_hex(s);

        if (input.size() < size())
            throw std::runtime_error("could not load point");

        std::copy(input.begin(), input.end(), std::begin(bytes));

        if (ge_frombytes_negate_vartime(&point3, bytes) != 0)
            throw std::runtime_error("could not load point");

        ge_p3_to_cached(&cached_point, &point3);
    }

    uint8_t bytes[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    ge_p3 point3;
    ge_cached cached_point;
} crypto_point_t;

namespace TurtleCoinCrypto
{
    // Primary Generator Point (x,-4/5)
    const crypto_point_t G = {0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
                              0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
                              0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66};

    // Secondary Generator Point = Hp(G)
    const crypto_point_t H = {0xdd, 0x2a, 0xf5, 0xc2, 0x8a, 0xcc, 0xdc, 0x50, 0xc8, 0xbc, 0x4e,
                              0x15, 0x99, 0x12, 0x82, 0x3a, 0x87, 0x87, 0xc1, 0x18, 0x52, 0x97,
                              0x74, 0x5f, 0xb2, 0x30, 0xe2, 0x64, 0x6c, 0xd7, 0x7e, 0xf6};

    // Zero Point (0,0)
    const crypto_point_t U = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // Neutral Point (0,1)
    const crypto_point_t Z = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
} // namespace TurtleCoinCrypto

typedef struct EllipticCurveScalar
{
    /**
     * Constructor methods
     */

    EllipticCurveScalar() {}

    EllipticCurveScalar(std::initializer_list<uint8_t> input, bool reduce = false)
    {
        std::copy(input.begin(), input.end(), std::begin(bytes));

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const uint8_t input[32], bool reduce = false)
    {
        std::copy(input, input + sizeof(bytes), std::begin(bytes));

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const std::string &s, bool reduce = false)
    {
        from_string(s);

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const uint64_t &number, bool reduce = false)
    {
        std::memmove(bytes, &number, sizeof(number));

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const std::vector<uint8_t> &input, bool reduce = false)
    {
        /**
         * We allow loading a full scalar (256-bits), a uint64_t (64-bits), or a uint32_t (32-bits)
         */
        if (input.size() < size() && input.size() != 8 && input.size() != 4)
            throw std::runtime_error("Could not load scalar");

        std::copy(input.begin(), input.end(), std::begin(bytes));

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const std::vector<EllipticCurveScalar> &bits, bool reduce = false)
    {
        from_bits(bits);

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const JSONValue &j, bool reduce = false)
    {
        if (!j.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());

        if (reduce)
            do_reduce();
    }

    EllipticCurveScalar(const JSONValue &j, const std::string &key, bool reduce = false)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());

        if (reduce)
            do_reduce();
    }

    /**
     * Operator overloads to make arithmetic a lot easier to handle in methods that use these structures
     */

    unsigned char &operator[](int i)
    {
        return bytes[i];
    }

    unsigned char operator[](int i) const
    {
        return bytes[i];
    }

    bool operator==(const EllipticCurveScalar &other) const
    {
        return std::equal(std::begin(bytes), std::end(bytes), std::begin(other.bytes));
    }

    bool operator!=(const EllipticCurveScalar &other) const
    {
        return !(*this == other);
    }

    bool operator<(const EllipticCurveScalar &other) const
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

    bool operator<=(const EllipticCurveScalar &other) const
    {
        return (*this < other) || (*this == other);
    }

    bool operator>(const EllipticCurveScalar &other) const
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

    bool operator>=(const EllipticCurveScalar &other) const
    {
        return (*this > other) || (*this == other);
    }

    EllipticCurveScalar operator+(const EllipticCurveScalar &other) const
    {
        EllipticCurveScalar result;

        sc_add(result.bytes, bytes, other.bytes);

        return result;
    }

    void operator+=(const EllipticCurveScalar &other)
    {
        sc_add(bytes, bytes, other.bytes);
    }

    EllipticCurveScalar operator-(const EllipticCurveScalar &other) const
    {
        EllipticCurveScalar result;

        sc_sub(result.bytes, bytes, other.bytes);

        return result;
    }

    void operator-=(const EllipticCurveScalar &other)
    {
        sc_sub(bytes, bytes, other.bytes);
    }

    EllipticCurveScalar operator*(const EllipticCurveScalar &other) const
    {
        EllipticCurveScalar result;

        sc_mul(result.bytes, bytes, other.bytes);

        return result;
    }

    /**
     * Overloads a Scalar * Point returning the resulting point
     * @param point
     * @return
     */
    EllipticCurvePoint operator*(const EllipticCurvePoint &point) const
    {
        ge_p3 temp_p3;

        ge_p1p1 temp_p1p1;

        if (point == TurtleCoinCrypto::G) // If we're multiplying by G, use the base method, it's faster
        {
            ge_scalarmult_base(&temp_p1p1, bytes);

            ge_p1p1_to_p3(&temp_p3, &temp_p1p1);

            return EllipticCurvePoint(temp_p3);
        }
        else
        {
            const auto p = point.p3();

            // aB = (a * B) mod l
            ge_scalarmult(&temp_p1p1, bytes, &p);

            ge_p1p1_to_p3(&temp_p3, &temp_p1p1);

            return EllipticCurvePoint(temp_p3);
        }
    }


    /**
     * Computes 8P
     * @return
     */
    EllipticCurvePoint mul8(const EllipticCurvePoint &other) const
    {
        ge_p3 temp_p3;

        ge_p1p1 temp_p1p1;

        if (other == TurtleCoinCrypto::G) // If we're multiplying by G, use the base method, it's faster
        {
            ge_scalarmult_base(&temp_p1p1, bytes);

            ge_p1p1_to_p3(&temp_p3, &temp_p1p1);
        }
        else
        {
            const auto p = other.p3();

            // aB = (a * B) mod l
            ge_scalarmult(&temp_p1p1, bytes, &p);

            ge_p1p1_to_p3(&temp_p3, &temp_p1p1);
        }

        ge_p2 temp_p2;

        ge_p1p1_to_p2(&temp_p2, &temp_p1p1);

        ge_mul8(&temp_p1p1, &temp_p2);

        ge_p1p1_to_p3(&temp_p3, &temp_p1p1);

        return EllipticCurvePoint(temp_p3);
    }

    void operator*=(const EllipticCurveScalar &other)
    {
        sc_mul(bytes, bytes, other.bytes);
    }

    /**
     * Allows us to check a random value to determine if it is a scalar or not
     * @param value
     * @return
     */
    template<typename T> static bool check(const T &value)
    {
        /**
         * Try loading the given value into a scalar type without performing a scalar reduction
         * (which would defeat the purpose of this check) and then check to see if the bytes
         * that we have loaded indicate that the value is actually a scalar. If we fail
         * at any point, then it definitely is not a scalar that was provided.
         */
        try
        {
            EllipticCurveScalar check_value(value, false);

            return check_value.check();
        }
        catch (...)
        {
            return false;
        }
    }

    /**
     * Member methods used in general operations using scalars
     */

    /**
     * Checks to validate that the scalar is indeed a scalar
     * @return
     */
    bool check() const
    {
        return sc_check(bytes) == 0;
    }

    /**
     * Returns a pointer to the underlying structure data
     * @return
     */
    const uint8_t *data() const
    {
        return bytes;
    }

    /**
     * Provides the inversion of the scalar (1/x)
     * @return
     */
    EllipticCurveScalar invert() const
    {
        EllipticCurveScalar two(2);

        const auto exponent = EllipticCurveScalar(l) - two;

        return pow(exponent);
    }

    /**
     * Checks to validate that the scalar is NOT zero (0)
     * @return
     */
    bool is_nonzero() const
    {
        return sc_isnonzero(bytes) == 0;
    }

    /**
     * Returns the negation of the scalar (-x)
     * @return
     */
    EllipticCurveScalar negate() const
    {
        EllipticCurveScalar zero({0});

        return zero - *this;
    }

    /**
     * Raises the scalar to the specified power
     * r = (s ^ e)
     * @param exponent
     * @return
     */
    EllipticCurveScalar pow(const EllipticCurveScalar &exponent) const
    {
        // convert our exponent to a vector of 256 individual bits
        const auto bits = exponent.to_bits(256);

        EllipticCurveScalar result(1), m(bytes);

        /**
         * Use the double-and-multiply method to calculate the value which results in us
         * performing at maximum, 512 scalar multiplication operations.
         */
        for (const auto &bit : bits)
        {
            if (bit == 1)
                result = result * m;

            m *= m;
        }

        return result;
    }

    /**
     * Generates a vector of powers of the scalar
     * @param count
     * @param descending
     * @return
     */
    std::vector<EllipticCurveScalar> pow_expand(size_t count, bool descending = false, bool include_zero = true) const
    {
        std::vector<EllipticCurveScalar> result(count);

        size_t start = 0, end = count;

        if (!include_zero)
        {
            start += 1;

            end += 1;
        }

        for (size_t i = start, j = 0; i < end; ++i, ++j)
            result[j] = pow(i);

        if (descending)
            std::reverse(result.begin(), result.end());

        return result;
    }

    /**
     * Sums the specified power of the scalar
     * @param count
     * @return
     */
    EllipticCurveScalar pow_sum(size_t count) const
    {
        const bool is_power_of_2 = (count & (count - 1)) == 0;

        if (!is_power_of_2)
            throw std::runtime_error("must be a power of 2");

        if (count == 0)
            return {0};

        if (count == 1)
            return 1;

        EllipticCurveScalar result(1), base(bytes);

        result += base;

        while (count > 2)
        {
            base *= base;

            result += result * base;

            count /= 2;
        }

        return result;
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
     * Use this method instead of sizeof(EllipticCurveScalar) to get the resulting
     * size of the key in bytes -- this is provided here just to help match up the
     * two structure types (point & scalar)
     * @return
     */
    size_t size() const
    {
        return sizeof(bytes);
    }

    /**
     * Squares the scalar
     * r = (s ^ 2)
     * @return
     */
    EllipticCurveScalar squared() const
    {
        EllipticCurveScalar result;

        sc_mul(result.bytes, bytes, bytes);

        return result;
    }

    /**
     * Converts the scalar to a vector of scalars that represent the individual
     * bits of the scalar (maximum of 256 bits as 32 * 8 = 256)
     * @param bits
     * @return
     */
    std::vector<EllipticCurveScalar> to_bits(size_t bits = 256) const
    {
        if (bits > 256)
            throw std::range_error("requested bit length exceeds maximum scalar bit length");

        std::vector<EllipticCurveScalar> result;

        result.reserve(bits);

        size_t offset = 0;

        // Loop until we have the number of requested bits
        while (result.size() != bits)
        {
            /**
             * Load the first 8-bytes (64 bits) into a uint64_t to make it easier
             * to manipulate using standard bit shifts
             */
            uint64_t temp;

            std::memmove(&temp, std::begin(bytes) + offset, sizeof(temp));

            // Loop through the 64-bits in the uint64_t
            for (size_t i = 0; i < 64; i++)
            {
                // Once we have the requested number of bits, break the loop
                if (result.size() == bits)
                    break;

                const EllipticCurveScalar bit((temp >> i) & 0x01);

                result.push_back(bit);
            }

            // Adjust the offset in the event we need more than 64-bits from the scalar
            offset += sizeof(temp);
        }

        return result;
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
     * Encodes the specified number of bytes of the scalar as a hexadecimal string
     * @param byte_length
     * @return
     */
    std::string to_string(size_t byte_length = 32) const
    {
        if (byte_length > 32)
            throw std::range_error("length cannot exceed the size of the scalar");

        return TurtleCoinCrypto::StringTools::to_hex(bytes, byte_length);
    }

    /**
     * Encodes the first 8 bytes of the scalar as a uint64_t
     * @return
     */
    uint64_t to_uint64_t() const
    {
        uint64_t result;

        std::memmove(&result, &bytes, sizeof(result));

        return result;
    }

  private:
    uint8_t bytes[32] = {0};

    /**
     * Reduces the bytes in the scalar such that it is, most definitely, a scalar
     */
    void do_reduce()
    {
        sc_reduce32(bytes);
    }

    /**
     * Loads the scalar from a vector of individual bits
     * @param bits
     */
    void from_bits(const std::vector<EllipticCurveScalar> &bits)
    {
        constexpr size_t bits_mod = 32;

        // set all bytes to zero
        std::fill(std::begin(bytes), std::end(bytes), 0);

        if (bits.empty())
            return;

        const EllipticCurveScalar ZERO = {0}, ONE = 1;

        size_t offset = 0;

        uint32_t tmp = 0;

        // loop through the individual bits
        for (size_t i = 0; i < bits.size(); ++i)
        {
            if (bits[i] != ZERO && bits[i] != ONE)
                throw std::range_error("individual bit scalar values must be zero (0) or one (1)");

            /**
             * If we are not at the start of the bits supplied and we have consumed
             * enough bits to complete a uint32_t, then move it on to the byte stack
             */
            if (i != 0 && i % bits_mod == 0)
            {
                // move the current uint32_t into the bytes
                std::memmove(bytes + offset, &tmp, sizeof(tmp));

                // reset the uint32_t
                tmp = 0;

                // increment the offset by the size of the uint32_t
                offset += sizeof(tmp);
            }

            // if the bit is one (1) then we need to shift it into place
            if (bits[i] == 1)
                tmp |= 1 << (i % bits_mod);
        }

        // move the current uint32_t into the bytes at the current offset
        std::memmove(bytes + offset, &tmp, sizeof(tmp));
    }

    /**
     * Loads the scalar from a hexadecimal encoded string
     * @param s
     */
    void from_string(const std::string &s)
    {
        const auto input = TurtleCoinCrypto::StringTools::from_hex(s);

        if (input.size() < size())
            throw std::runtime_error("Could not load scalar");

        std::copy(input.begin(), input.end(), std::begin(bytes));
    }
} crypto_scalar_t;

namespace TurtleCoinCrypto
{
    // Commonly used Scalar values (0, 1, 2, 8, 1/8)
    const crypto_scalar_t ZERO = {0}, ONE(1), TWO(2), EIGHT(8), INV_EIGHT = EIGHT.invert();

    const crypto_scalar_t L = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                               0xa2, 0xde, 0xf9, 0xde, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
} // namespace TurtleCoinCrypto

/**
 * Converts an EllipticCurvePoint to an EllipticCurveScalar
 * @param point
 * @return
 */
static inline crypto_scalar_t pointToScalar(const crypto_point_t &point)
{
    uint8_t bytes[32];

    std::memmove(&bytes, point.data(), sizeof(bytes));

    return crypto_scalar_t(bytes);
}

#define PointToScalar(a) pointToScalar(a)

/**
 * These are aliased here to make it clearer what context the point and scalars are
 * being used for during different method calls. This helps to avoid confusion
 * when working with the various method calls without losing track of the type
 * of the parameters being passed and their return values
 */

typedef crypto_point_t crypto_public_key_t;

typedef crypto_scalar_t crypto_secret_key_t;

typedef crypto_point_t crypto_derivation_t;

typedef crypto_point_t crypto_key_image_t;

typedef crypto_scalar_t crypto_blinding_factor_t;

typedef crypto_point_t crypto_pedersen_commitment_t;

typedef struct Signature
{
    /**
     * Constructor methods
     */

    Signature() {}

    Signature(std::initializer_list<uint8_t> LR)
    {
        std::copy(LR.begin(), LR.end(), std::begin(bytes));
    }

    Signature(const uint8_t LR[64])
    {
        std::copy(LR, LR + sizeof(bytes), std::begin(bytes));
    }

    Signature(const uint8_t L[32], const uint8_t R[32])
    {
        LR.L = L;

        LR.R = R;
    }

    Signature(const std::string &LR)
    {
        from_string(LR);
    }

    Signature(const JSONValue &j)
    {
        if (!j.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());
    }

    Signature(const JSONValue &j, const std::string &key)
    {
        const auto &val = get_json_value(j, key);

        if (!val.IsString())
            throw std::invalid_argument("JSON value is of the wrong type");

        from_string(j.GetString());
    }

    /**
     * Simple operator overloads for comparison
     */

    bool operator==(const Signature &other) const
    {
        return std::equal(std::begin(bytes), std::end(bytes), std::begin(other.bytes));
    }

    bool operator!=(const Signature &other) const
    {
        return !(*this == other);
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
     * Use this method instead of sizeof(Signature) to get the resulting size of the value in bytes
     * @return
     */
    size_t size() const
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
     * Encodes a signature as a hexadecimal string
     * @return
     */
    std::string to_string() const
    {
        return TurtleCoinCrypto::StringTools::to_hex(bytes, sizeof(bytes));
    }

  private:
    /**
     * Loads a signature from a hexademical string
     * @param s
     */
    void from_string(const std::string &s)
    {
        const auto input = TurtleCoinCrypto::StringTools::from_hex(s);

        if (input.size() < size())
            throw std::runtime_error("Could not load signature");

        std::copy(input.begin(), input.end(), std::begin(bytes));
    }

    /**
     * A signature is composes of two scalars concatenated together such that S = (L || R)
     */
    struct signature_scalars
    {
        crypto_scalar_t L;
        crypto_scalar_t R;
    };

  public:
    /**
     * Provides an easy to reference structure for the signature of either the concatenated
     * L and R values together as a single 64 bytes or via the individual L & R scalars
     */
    union
    {
        signature_scalars LR;
        uint8_t bytes[64] = {0};
    };
} crypto_signature_t;

/**
 * Providing overloads into the std namespace such that we can easily included
 * points, scalars, and signatures in output streams
 */
namespace std
{
    inline ostream &operator<<(ostream &os, const crypto_point_t &value)
    {
        os << value.to_string();

        return os;
    }

    inline ostream &operator<<(ostream &os, const crypto_scalar_t &value)
    {
        os << value.to_string();

        return os;
    }

    inline ostream &operator<<(ostream &os, const crypto_signature_t &value)
    {
        os << value.to_string();

        return os;
    }
} // namespace std

#endif // CRYPTO_TYPES_H
