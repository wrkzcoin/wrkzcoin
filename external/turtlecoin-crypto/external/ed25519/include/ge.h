/**
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#ifndef ED25519_GE_H
#define ED25519_GE_H

#include "fe.h"

#include <cstring>
#include <iostream>
#include <string>

typedef struct GeP2
{
    bool operator==(const GeP2 &other) const
    {
        return memcmp(X, other.X, sizeof(X)) == 0 && memcmp(Y, other.Y, sizeof(Y)) == 0
               && memcmp(Z, other.Z, sizeof(Z)) == 0;
    }

    bool operator!=(const GeP2 &other) const
    {
        return !(*this == other);
    }

    fe X = {0};
    fe Y = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe Z = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
} ge_p2;

typedef struct GeP3
{
    bool operator==(const GeP3 &other) const
    {
        return memcmp(X, other.X, sizeof(X)) == 0 && memcmp(Y, other.Y, sizeof(Y)) == 0
               && memcmp(Z, other.Z, sizeof(Z)) == 0 && memcmp(T, other.T, sizeof(T)) == 0;
    }

    bool operator!=(const GeP3 &other) const
    {
        return !(*this == other);
    }

    fe X = {0};
    fe Y = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe Z = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe T = {0};
} ge_p3;

typedef struct GeP1P1
{
    bool operator==(const GeP1P1 &other) const
    {
        return memcmp(X, other.X, sizeof(X)) == 0 && memcmp(Y, other.Y, sizeof(Y)) == 0
               && memcmp(Z, other.Z, sizeof(Z)) == 0 && memcmp(T, other.T, sizeof(T)) == 0;
    }

    bool operator!=(const GeP1P1 &other) const
    {
        return !(*this == other);
    }

    fe X = {0};
    fe Y = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe Z = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe T = {0};
} ge_p1p1;

typedef struct GePrecomp
{
    bool operator==(const GePrecomp &other) const
    {
        return memcmp(yplusx, other.yplusx, sizeof(yplusx)) == 0 && memcmp(yminusx, other.yminusx, sizeof(yminusx)) == 0
               && memcmp(xy2d, other.xy2d, sizeof(xy2d)) == 0;
    }

    bool operator!=(const GePrecomp &other) const
    {
        return !(*this == other);
    }

    fe yplusx = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe yminusx = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe xy2d = {0};
} ge_precomp;

typedef struct GeCached
{
    bool operator==(const GeCached &other) const
    {
        return memcmp(YplusX, other.YplusX, sizeof(YplusX)) == 0 && memcmp(YminusX, other.YminusX, sizeof(YminusX)) == 0
               && memcmp(Z, other.Z, sizeof(Z)) == 0 && memcmp(T2d, other.T2d, sizeof(T2d)) == 0;
    }

    bool operator!=(const GeCached &other) const
    {
        return !(*this == other);
    }

    fe YplusX = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe YminusX = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe Z = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    fe T2d = {0};
} ge_cached;

typedef ge_cached ge_dsmp[8];

namespace std
{
    inline ostream &operator<<(ostream &os, const ge_p2 &value)
    {
        os << "ge_p2: " << std::endl;

        os << "\tX: ";
        for (const auto &val : value.X)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tY: ";
        for (const auto &val : value.Y)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tZ: ";
        for (const auto &val : value.Z)
            os << std::to_string(val) << ",";
        os << std::endl;

        return os;
    }

    inline ostream &operator<<(ostream &os, const ge_p3 &value)
    {
        os << "ge_p3: " << std::endl;

        os << "\tX: ";
        for (const auto &val : value.X)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tY: ";
        for (const auto &val : value.Y)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tZ: ";
        for (const auto &val : value.Z)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tT: ";
        for (const auto &val : value.T)
            os << std::to_string(val) << ",";
        os << std::endl;

        return os;
    }

    inline ostream &operator<<(ostream &os, const ge_p1p1 &value)
    {
        os << "ge_p1p1: " << std::endl;

        os << "\tX: ";
        for (const auto &val : value.X)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tY: ";
        for (const auto &val : value.Y)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tZ: ";
        for (const auto &val : value.Z)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tT: ";
        for (const auto &val : value.T)
            os << std::to_string(val) << ",";
        os << std::endl;

        return os;
    }

    inline ostream &operator<<(ostream &os, const ge_precomp &value)
    {
        os << "ge_precomp: " << std::endl;

        os << "\typlusx: ";
        for (const auto &val : value.yplusx)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tyminusx: ";
        for (const auto &val : value.yminusx)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\txy2d: ";
        for (const auto &val : value.xy2d)
            os << std::to_string(val) << ",";
        os << std::endl;

        return os;
    }

    inline ostream &operator<<(ostream &os, const ge_cached &value)
    {
        os << "ge_cached: " << std::endl;

        os << "\tYplusX: ";
        for (const auto &val : value.YplusX)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tYminusX: ";
        for (const auto &val : value.YminusX)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tZ: ";
        for (const auto &val : value.Z)
            os << std::to_string(val) << ",";
        os << std::endl;

        os << "\tT2d: ";
        for (const auto &val : value.T2d)
            os << std::to_string(val) << ",";
        os << std::endl;

        return os;
    }
} // namespace std

#endif // ED25519_GE_H
