// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <vector>

#include <system/IpAddress.h>

namespace System
{
    class Dispatcher;

    // Resolves hostnames to IpAddress (IPv4 and IPv6).
    class IpResolver
    {
      public:
        IpResolver();

        explicit IpResolver(Dispatcher &dispatcher);

        IpResolver(const IpResolver &) = delete;

        IpResolver(IpResolver &&other);

        ~IpResolver();

        IpResolver &operator=(const IpResolver &) = delete;

        IpResolver &operator=(IpResolver &&other);

        // Resolve hostname, preferring IPv4 over IPv6.
        IpAddress resolve(const std::string &host);

        // Resolve all A and AAAA records for hostname.
        std::vector<IpAddress> resolveAll(const std::string &host);

      private:
        Dispatcher *dispatcher;
    };

} // namespace System
