// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>

namespace System
{
    // Dual-stack IP address holding either an IPv4 or IPv6 address.
    // IPv4 is stored as 4 big-endian bytes in m_bytes[0..3].
    // IPv6 is stored as 16 big-endian bytes in m_bytes[0..15].
    class IpAddress
    {
      public:
        // Construct IPv4 from host-order uint32
        explicit IpAddress(uint32_t ipv4);

        // Construct IPv6 from 16 network-order bytes
        explicit IpAddress(const uint8_t ip6[16]);

        // Parse from string: "1.2.3.4" (IPv4) or "[::1]" / "::1" (IPv6)
        explicit IpAddress(const std::string &s);

        bool isV4() const;

        bool isV6() const;

        // Returns host-order uint32; only valid when isV4()
        uint32_t toV4() const;

        // Returns pointer to 16 raw bytes in network order
        const uint8_t *getBytes() const;

        // Returns "1.2.3.4" for IPv4 or "[2001:db8::1]" for IPv6
        std::string toString() const;

        bool isLoopback() const;

        bool isPrivate() const;

        bool operator==(const IpAddress &other) const;

        bool operator!=(const IpAddress &other) const;

        bool operator<(const IpAddress &other) const;

      private:
        bool    m_isV6;
        uint8_t m_bytes[16]; // IPv4 uses first 4 bytes only
    };

} // namespace System
