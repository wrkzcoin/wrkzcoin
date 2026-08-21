// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <common/StringTools.h>
#include <cstdint>
#include <string.h>
#include <string>
#include <tuple>

struct NetworkAddress
{
    uint32_t ip;
    uint32_t port;
};

// IPv6 network address: 16-byte address + port (used in P2P peer exchange)
struct NetworkAddress6
{
    uint8_t  ip[16]; // network-order IPv6 bytes
    uint32_t port;
};

// Fields ordered to avoid padding: id+last_seen first (uint64), then ip[16]+port = 36 bytes total
struct PeerlistEntry6
{
    uint64_t        id;
    uint64_t        last_seen;
    NetworkAddress6 adr;
};

struct PeerlistEntry
{
    NetworkAddress adr;
    uint64_t id;
    uint64_t last_seen;
};

struct connection_entry
{
    NetworkAddress adr;
    uint64_t id;
    bool is_income;
};

inline bool operator<(const NetworkAddress &a, const NetworkAddress &b)
{
    return std::tie(a.ip, a.port) < std::tie(b.ip, b.port);
}

inline bool operator<(const NetworkAddress6 &a, const NetworkAddress6 &b)
{
    int c = memcmp(a.ip, b.ip, 16);
    if (c != 0) return c < 0;
    return a.port < b.port;
}

inline bool operator==(const NetworkAddress6 &a, const NetworkAddress6 &b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}

inline bool operator==(const NetworkAddress &a, const NetworkAddress &b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}

inline std::ostream &operator<<(std::ostream &s, const NetworkAddress &na)
{
    return s << Common::ipAddressToString(na.ip) << ":" << std::to_string(na.port);
}

inline uint32_t hostToNetwork(uint32_t n)
{
    return (n << 24) | (n & 0xff00) << 8 | (n & 0xff0000) >> 8 | (n >> 24);
}

inline uint32_t networkToHost(uint32_t n)
{
    return hostToNetwork(n); // the same
}
