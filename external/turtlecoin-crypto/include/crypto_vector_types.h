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

#ifndef CRYPTO_VECTOR_TYPES_H
#define CRYPTO_VECTOR_TYPES_H

#include "crypto_types.h"

#include <set>

namespace Crypto
{
    /**
     * Removes duplicates from a vector of keys and sorts them by value
     * @tparam T
     * @param keys the keys to dedupe and sort
     * @return the resultant vector of keys
     */
    template<typename T> std::vector<T> dedupe_and_sort_keys(const std::vector<T> &keys)
    {
        auto cmp = [](const T a, const T b) { return memcmp(&a, &b, sizeof(a)) > 0; };

        std::set<T, decltype(cmp)> seen(keys.begin(), keys.end(), cmp);

        std::vector<T> result(seen.begin(), seen.end());

        return result;
    }
} // namespace Crypto

typedef struct EllipticCurvePointVector
{
    EllipticCurvePointVector() {};

    EllipticCurvePointVector(std::vector<EllipticCurvePoint> points): points(std::move(points)) {};

    /**
     * Initializes the structure of the size count with the given value
     * @param size
     * @param value
     */
    EllipticCurvePointVector(size_t size, const EllipticCurvePoint &value = Crypto::Z)
    {
        points = std::vector<EllipticCurvePoint>(size, value);
    }

    EllipticCurvePoint &operator[](size_t i)
    {
        return points[i];
    }

    EllipticCurvePoint operator[](size_t i) const
    {
        return points[i];
    }

    bool operator==(const EllipticCurvePointVector &other) const
    {
        return std::equal(points.begin(), points.end(), other.points.begin());
    }

    bool operator!=(const EllipticCurvePointVector &other) const
    {
        return !(*this == other);
    }

    /**
     * Adds the two vectors together and returns the resulting vector
     * @param other
     * @return
     */
    EllipticCurvePointVector operator+(const EllipticCurvePointVector &other) const
    {
        if (points.size() != other.points.size())
            throw std::range_error("vectors must be of the same size");

        std::vector<EllipticCurvePoint> result(points);

        for (size_t i = 0; i < result.size(); ++i)
            result[i] += other.points[i];

        return result;
    }

    /**
     * Subtracts the second vector from the first vector and returns the results
     * @param other
     * @return
     */
    EllipticCurvePointVector operator-(const EllipticCurvePointVector &other) const
    {
        if (points.size() != other.points.size())
            throw std::range_error("vectors must be of the same size");

        std::vector<EllipticCurvePoint> result(points);

        for (size_t i = 0; i < result.size(); ++i)
            result[i] -= other.points[i];

        return result;
    }

    /**
     * Multiplies the vectors together and returns the results
     * @param other
     * @return
     */
    EllipticCurvePointVector operator*(const EllipticCurveScalar &other) const
    {
        std::vector<EllipticCurvePoint> result(points);

        for (auto &point : result)
            point = other * point;

        return result;
    }

    /**
     * Alias of struct::points::push_back
     * @param value
     */
    void append(const EllipticCurvePoint &value)
    {
        points.push_back(value);
    }

    /**
     * Returns the last value in the underlying container
     * @return
     */
    [[nodiscard]] EllipticCurvePoint back() const
    {
        return points.back();
    }

    /**
     * Removes duplicates of the keys and sorts them by value
     * @return
     */
    [[nodiscard]] EllipticCurvePointVector dedupe_sort() const
    {
        return Crypto::dedupe_and_sort_keys(points);
    }

    /**
     * Appends the provided vector to the end of the underlying container
     * @param values
     */
    void extend(const std::vector<EllipticCurvePoint> &values)
    {
        for (const auto &value : values)
            points.push_back(value);
    }

    /**
     * Appends the provided vector to the end of the underlying container
     * @param value
     */
    void extend(const EllipticCurvePointVector &value)
    {
        extend(value.points);
    }

    /**
     * Negates all of the values in the underlying container (0 - self)
     * @return
     */
    [[nodiscard]] EllipticCurvePointVector negate() const
    {
        std::vector<EllipticCurvePoint> result(points);

        for (auto &point : result)
            point = point.negate();

        return result;
    }

    /**
     * Returns the size of the underlying container
     * @return
     */
    [[nodiscard]] size_t size() const
    {
        return points.size();
    }

    /**
     * Returns a slice of the underlying vector using the provided offsets
     * @param start
     * @param end
     * @return
     */
    [[nodiscard]] EllipticCurvePointVector slice(size_t start, size_t end) const
    {
        if (end < start)
            throw std::range_error("ending offset must be greater than or equal to starting offset");

        return std::vector<EllipticCurvePoint>(points.begin() + start, points.begin() + end);
    }

    /**
     * Adds all of the values in the underlying container together and returns the result
     * @return
     */
    [[nodiscard]] EllipticCurvePoint sum() const
    {
        auto result = Crypto::Z;

        for (const auto &point : points)
            result += point;

        return result;
    }

    std::vector<EllipticCurvePoint> points;
} crypto_point_vector_t;

typedef struct EllipticCurveScalarVector
{
    EllipticCurveScalarVector() {};

    EllipticCurveScalarVector(std::vector<EllipticCurveScalar> scalars): scalars(std::move(scalars)) {};

    /**
     * Initializes the structure of the size count with the given value
     * @param size
     * @param value
     */
    EllipticCurveScalarVector(size_t size, const EllipticCurveScalar &value = Crypto::ZERO)
    {
        scalars = std::vector<EllipticCurveScalar>(size, value);
    }

    EllipticCurveScalar &operator[](size_t i)
    {
        return scalars[i];
    }

    EllipticCurveScalar operator[](size_t i) const
    {
        return scalars[i];
    }

    bool operator==(const EllipticCurveScalarVector &other) const
    {
        return std::equal(scalars.begin(), scalars.end(), other.scalars.begin());
    }

    bool operator!=(const EllipticCurveScalarVector &other) const
    {
        return !(*this == other);
    }

    /**
     * Adds the scalar to every value in the underlying container
     * @param other
     * @return
     */
    EllipticCurveScalarVector operator+(const EllipticCurveScalar &other) const
    {
        std::vector<EllipticCurveScalar> result(scalars);

        for (auto &val : result)
            val += other;

        return result;
    }

    /**
     * Adds the two vectors together and returns the resulting vector
     * @param other
     * @return
     */
    EllipticCurveScalarVector operator+(const EllipticCurveScalarVector &other) const
    {
        if (scalars.size() != other.scalars.size())
            throw std::range_error("vectors must be of the same size");

        std::vector<EllipticCurveScalar> result(scalars);

        for (size_t i = 0; i < result.size(); ++i)
            result[i] += other.scalars[i];

        return result;
    }

    /**
     * Subtracts the scalar to every value in the underlying container
     * @param other
     * @return
     */
    EllipticCurveScalarVector operator-(const EllipticCurveScalar &other) const
    {
        std::vector<EllipticCurveScalar> result(scalars);

        for (auto &val : result)
            val -= other;

        return result;
    }

    /**
     * Subtracts the second vector from the first vector and returns the results
     * @param other
     * @return
     */
    EllipticCurveScalarVector operator-(const EllipticCurveScalarVector &other) const
    {
        if (scalars.size() != other.scalars.size())
            throw std::range_error("vectors must be of the same size");

        std::vector<EllipticCurveScalar> result(scalars);

        for (size_t i = 0; i < result.size(); ++i)
            result[i] -= other.scalars[i];

        return result;
    }

    /**
     * Multiplies every value in the underlying container by the provided scalar and
     * returns the results
     * @param other
     * @return
     */
    EllipticCurveScalarVector operator*(const EllipticCurveScalar &other) const
    {
        std::vector<EllipticCurveScalar> result(scalars);

        for (auto &val : result)
            val *= other;

        return result;
    }

    /**
     * Multiplies the vectors together and returns the results
     * Some call this a hadamard calculation
     * @param other
     * @return
     */
    EllipticCurveScalarVector operator*(const EllipticCurveScalarVector &other) const
    {
        if (scalars.size() != other.scalars.size())
            throw std::range_error("vectors must be of the same size");

        std::vector<EllipticCurveScalar> result(scalars);

        for (size_t i = 0; i < result.size(); ++i)
            result[i] *= other.scalars[i];

        return result;
    }

    /**
     * Multiplies the underlying vector of scalars by the vector of provided points
     * and returns the resulting points
     * @param other
     * @return
     */
    EllipticCurvePointVector operator*(const EllipticCurvePointVector &other) const
    {
        if (scalars.size() != other.points.size())
            throw std::range_error("vectors must be of the same size");

        std::vector<EllipticCurvePoint> result(scalars.size());

        for (size_t i = 0; i < result.size(); ++i)
            result[i] = scalars[i] * other.points[i];

        return result;
    }

    /**
     * Alias of struct::scalars::push_back
     * @param value
     */
    void append(const EllipticCurveScalar &value)
    {
        scalars.push_back(value);
    }

    /**
     * Returns the last value in the underlying container
     * @return
     */
    [[nodiscard]] EllipticCurveScalar back() const
    {
        return scalars.back();
    }

    /**
     * Removes duplicates of the keys and sorts them by value
     * @return
     */
    [[nodiscard]] EllipticCurveScalarVector dedupe_sort() const
    {
        return Crypto::dedupe_and_sort_keys(scalars);
    }

    /**
     * Appends the provided vector to the end of the underlying container
     * @param values
     */
    void extend(const std::vector<EllipticCurveScalar> &values)
    {
        for (const auto &value : values)
            scalars.push_back(value);
    }

    /**
     * Appends the provided vector to the end of the underlying container
     * @param value
     */
    void extend(const EllipticCurveScalarVector &value)
    {
        extend(value.scalars);
    }

    /**
     * Calculates the inner product of the two vectors
     * @param other
     * @return
     */
    [[nodiscard]] EllipticCurvePoint inner_product(const EllipticCurvePointVector &other) const
    {
        if (scalars.size() != other.points.size())
            throw std::range_error("vectors must be of equal size");

        /**
         * If there is only a single value in each vector then it is faster
         * to just compute the result of the multiplication
         */
        if (scalars.size() == 1)
            return scalars[0] * other[0];

        /**
         * The method below reduces the number of individual scalar multiplications and additions
         * performed in individual calls by using ge_double_scalarmult_negate_vartime instead
         * of regular ge_scalarmult (regardless of the implementation) it does not incur the
         * extra overhead of expanding and contracting multiple times. An alternative to this
         * is a method which, while reliable, is quite a bit slower and left below as a reference.
         *
         * return (*this * other).sum();
         */

        // Divide our vectors in half so that we can get a (L)eft and a (R)ight
        const auto n = scalars.size() / 2;

        crypto_point_vector_t points(n);

        // slice the scalars and the points up into the (L)eft and (R)ight
        const auto aL = slice(0, n), aR = slice(n, n * 2);

        const auto AL = other.slice(0, n), AR = other.slice(n, n * 2);

        for (size_t i = 0; i < aL.size(); i++)
        {
            ge_p1p1 point_p1p1;

            ge_p3 point_p3 = AR[i].p3();

            ge_dsmp pb_precomp;

            ge_dsm_precomp(pb_precomp, &point_p3);

            point_p3 = AL[i].p3();

            /**
             * Compute both (L)eft and (R)ight at the same time and add them together
             * r = (a * A) + (b * B)
             */
            ge_double_scalarmult_negate_vartime(&point_p1p1, aL[i].data(), &point_p3, aR[i].data(), pb_precomp);

            ge_p1p1_to_p3(&point_p3, &point_p1p1);

            const crypto_point_t point(point_p3);

            /**
             * When we get back the point (0,0) we need to convert it to a neutral point (0,1)
             */
            points[i] = point != Crypto::U ? point : Crypto::Z;
        }

        /**
         * If there was a (singular) value in the vectors that was not included in the
         * (L)eft and (R)ight pairings then toss that on to the end of the vector
         */
        if (n * 2 != scalars.size())
        {
            points.append(scalars.back() * other.back());
        }

        // Tally up the results and send them back
        return points.sum();
    }

    /**
     * Calculates the inner product of the two vectors
     * @param other
     * @return
     */
    [[nodiscard]] EllipticCurveScalar inner_product(const EllipticCurveScalarVector &other) const
    {
        if (scalars.size() != other.scalars.size())
            throw std::range_error("vectors must be of equal size");

        return (*this * other).sum();
    }

    /**
     * Inverts each of the values in the underlying container such that (1/x)
     * @param allow_zero
     * @return
     */
    [[nodiscard]] EllipticCurveScalarVector invert(bool allow_zero = false) const
    {
        if (allow_zero)
        {
            std::vector<EllipticCurveScalar> result(scalars);

            for (auto &scalar : result)
                scalar = scalar.invert();

            return result;
        }
        else
        {
            auto inputs = scalars;

            const auto n = inputs.size();

            std::vector<EllipticCurveScalar> scratch(n, Crypto::ONE);

            auto acc = Crypto::ONE;

            for (size_t i = 0; i < n; ++i)
            {
                if (inputs[i] == Crypto::ZERO)
                    throw std::range_error("cannot divide by 0");

                scratch[i] = acc;

                acc *= inputs[i];
            }

            acc = acc.invert();

            for (size_t i = n; i-- > 0;)
            {
                auto temp = acc * inputs[i];

                inputs[i] = acc * scratch[i];

                acc = temp;
            }

            return inputs;
        }
    }

    /**
     * Negates all of the values in the underlying container (0 - self)
     * @return
     */
    [[nodiscard]] EllipticCurveScalarVector negate() const
    {
        std::vector<EllipticCurveScalar> result(scalars);

        for (auto &point : result)
            point = point.negate();

        return result;
    }

    /**
     * Returns the size of the underlying container
     * @return
     */
    [[nodiscard]] size_t size() const
    {
        return scalars.size();
    }

    /**
     * Returns a slice of the underlying vector using the provided offsets
     * @param start
     * @param end
     * @return
     */
    [[nodiscard]] EllipticCurveScalarVector slice(size_t start, size_t end) const
    {
        if (end < start)
            throw std::range_error("ending offset must be greater than or equal to starting offset");

        return std::vector<EllipticCurveScalar>(scalars.begin() + start, scalars.begin() + end);
    }

    /**
     * Adds all of the values in the underlying container together and returns the result
     * @return
     */
    [[nodiscard]] EllipticCurveScalar sum() const
    {
        auto result = Crypto::ZERO;

        for (const auto &scalar : scalars)
            result += scalar;

        return result;
    }

    std::vector<EllipticCurveScalar> scalars;
} crypto_scalar_vector_t;

#endif // CRYPTO_VECTOR_TYPES_H
