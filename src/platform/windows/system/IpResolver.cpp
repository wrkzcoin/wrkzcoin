// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "IpResolver.h"

#include <cassert>
#include <stdexcept>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <system/Dispatcher.h>
#include <system/ErrorMessage.h>
#include <system/IpAddress.h>
#include <system/InterruptedException.h>

namespace System
{
    IpResolver::IpResolver() : dispatcher(nullptr) {}

    IpResolver::IpResolver(Dispatcher &dispatcher) : dispatcher(&dispatcher) {}

    IpResolver::IpResolver(IpResolver &&other) : dispatcher(other.dispatcher)
    {
        if (dispatcher != nullptr)
        {
            other.dispatcher = nullptr;
        }
    }

    IpResolver::~IpResolver() {}

    IpResolver &IpResolver::operator=(IpResolver &&other)
    {
        dispatcher = other.dispatcher;
        if (dispatcher != nullptr)
        {
            other.dispatcher = nullptr;
        }
        return *this;
    }

    IpAddress IpResolver::resolve(const std::string &host)
    {
        assert(dispatcher != nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        ADDRINFOA hints = {};
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        ADDRINFOA *infos;
        int result = getaddrinfo(host.c_str(), nullptr, &hints, &infos);
        if (result != 0)
        {
            throw std::runtime_error("IpResolver::resolve, getaddrinfo failed, " + errorMessage(result));
        }

        // Prefer IPv4
        for (ADDRINFOA *ai = infos; ai != nullptr; ai = ai->ai_next)
        {
            if (ai->ai_family == AF_INET)
            {
                uint32_t v4 = ntohl(reinterpret_cast<sockaddr_in *>(ai->ai_addr)->sin_addr.S_un.S_addr);
                freeaddrinfo(infos);
                return IpAddress(v4);
            }
        }

        // Fall back to IPv6
        for (ADDRINFOA *ai = infos; ai != nullptr; ai = ai->ai_next)
        {
            if (ai->ai_family == AF_INET6)
            {
                IpAddress addr(reinterpret_cast<sockaddr_in6 *>(ai->ai_addr)->sin6_addr.s6_addr);
                freeaddrinfo(infos);
                return addr;
            }
        }

        freeaddrinfo(infos);
        throw std::runtime_error("IpResolver::resolve, no addresses found for: " + host);
    }

    std::vector<IpAddress> IpResolver::resolveAll(const std::string &host)
    {
        assert(dispatcher != nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        ADDRINFOA hints = {};
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        ADDRINFOA *infos;
        int result = getaddrinfo(host.c_str(), nullptr, &hints, &infos);
        if (result != 0)
        {
            throw std::runtime_error("IpResolver::resolveAll, getaddrinfo failed, " + errorMessage(result));
        }

        std::vector<IpAddress> addresses;
        for (ADDRINFOA *ai = infos; ai != nullptr; ai = ai->ai_next)
        {
            if (ai->ai_family == AF_INET)
            {
                uint32_t v4 = ntohl(reinterpret_cast<sockaddr_in *>(ai->ai_addr)->sin_addr.S_un.S_addr);
                addresses.emplace_back(v4);
            }
            else if (ai->ai_family == AF_INET6)
            {
                addresses.emplace_back(reinterpret_cast<sockaddr_in6 *>(ai->ai_addr)->sin6_addr.s6_addr);
            }
        }

        freeaddrinfo(infos);
        return addresses;
    }

} // namespace System
