// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include <system/IpAddress.h>

#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#endif

namespace System
{
    IpAddress::IpAddress(uint32_t ipv4) : m_isV6(false)
    {
        std::memset(m_bytes, 0, sizeof m_bytes);
        // Store host-order uint32 as big-endian bytes
        m_bytes[0] = static_cast<uint8_t>(ipv4 >> 24);
        m_bytes[1] = static_cast<uint8_t>(ipv4 >> 16);
        m_bytes[2] = static_cast<uint8_t>(ipv4 >> 8);
        m_bytes[3] = static_cast<uint8_t>(ipv4);
    }

    IpAddress::IpAddress(const uint8_t ip6[16]) : m_isV6(true)
    {
        std::memcpy(m_bytes, ip6, 16);
    }

    IpAddress::IpAddress(const std::string &s) : m_isV6(false)
    {
        std::memset(m_bytes, 0, sizeof m_bytes);
        if (s.empty())
        {
            throw std::runtime_error("IpAddress: empty string");
        }

        // IPv6 detection: contains ':'
        if (s.find(':') != std::string::npos)
        {
            m_isV6 = true;
            std::string addr = s;
            // Strip optional brackets [...]
            if (addr.size() >= 2 && addr.front() == '[' && addr.back() == ']')
            {
                addr = addr.substr(1, addr.size() - 2);
            }
            if (inet_pton(AF_INET6, addr.c_str(), m_bytes) != 1)
            {
                throw std::runtime_error("IpAddress: invalid IPv6 address: " + s);
            }
        }
        else
        {
            // IPv4 dotted-decimal
            uint8_t tmp[4];
            if (inet_pton(AF_INET, s.c_str(), tmp) != 1)
            {
                throw std::runtime_error("IpAddress: invalid IPv4 address: " + s);
            }
            m_bytes[0] = tmp[0];
            m_bytes[1] = tmp[1];
            m_bytes[2] = tmp[2];
            m_bytes[3] = tmp[3];
        }
    }

    bool IpAddress::isV4() const
    {
        return !m_isV6;
    }

    bool IpAddress::isV6() const
    {
        return m_isV6;
    }

    uint32_t IpAddress::toV4() const
    {
        return (uint32_t(m_bytes[0]) << 24)
             | (uint32_t(m_bytes[1]) << 16)
             | (uint32_t(m_bytes[2]) << 8)
             |  uint32_t(m_bytes[3]);
    }

    const uint8_t *IpAddress::getBytes() const
    {
        return m_bytes;
    }

    std::string IpAddress::toString() const
    {
        if (!m_isV6)
        {
            char buf[INET_ADDRSTRLEN];
            // inet_ntop expects network-order bytes; m_bytes is already big-endian
            if (inet_ntop(AF_INET, m_bytes, buf, sizeof buf) == nullptr)
            {
                throw std::runtime_error("IpAddress::toString: inet_ntop failed");
            }
            return buf;
        }
        else
        {
            char inner[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, m_bytes, inner, sizeof inner) == nullptr)
            {
                throw std::runtime_error("IpAddress::toString: inet_ntop failed");
            }
            return std::string("[") + inner + "]";
        }
    }

    bool IpAddress::isLoopback() const
    {
        if (!m_isV6)
        {
            // 127.0.0.0/8
            return m_bytes[0] == 127;
        }
        else
        {
            // ::1
            for (int i = 0; i < 15; ++i)
            {
                if (m_bytes[i] != 0) return false;
            }
            return m_bytes[15] == 1;
        }
    }

    bool IpAddress::isPrivate() const
    {
        if (!m_isV6)
        {
            uint32_t v = toV4();
            return
                // 10.0.0.0/8
                (v & 0xff000000u) == 0x0a000000u ||
                // 172.16.0.0/12
                (v & 0xfff00000u) == 0xac100000u ||
                // 192.168.0.0/16
                (v & 0xffff0000u) == 0xc0a80000u;
        }
        else
        {
            // fc00::/7  (Unique Local Addresses)
            return (m_bytes[0] & 0xfe) == 0xfc;
        }
    }

    bool IpAddress::operator==(const IpAddress &other) const
    {
        if (m_isV6 != other.m_isV6) return false;
        return std::memcmp(m_bytes, other.m_bytes, m_isV6 ? 16 : 4) == 0;
    }

    bool IpAddress::operator!=(const IpAddress &other) const
    {
        return !(*this == other);
    }

    bool IpAddress::operator<(const IpAddress &other) const
    {
        // IPv4 sorts before IPv6
        if (m_isV6 != other.m_isV6) return !m_isV6;
        return std::memcmp(m_bytes, other.m_bytes, m_isV6 ? 16 : 4) < 0;
    }

} // namespace System
