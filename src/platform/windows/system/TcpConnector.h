// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>

namespace System
{
    class Dispatcher;

    class IpAddress;

    class Ipv4Address;

    class TcpConnection;

    class TcpConnector
    {
      public:
        TcpConnector();

        explicit TcpConnector(Dispatcher &dispatcher);

        TcpConnector(const TcpConnector &) = delete;

        TcpConnector(TcpConnector &&other);

        ~TcpConnector();

        TcpConnector &operator=(const TcpConnector &) = delete;

        TcpConnector &operator=(TcpConnector &&other);

        TcpConnection connect(const Ipv4Address &address, uint16_t port);

        TcpConnection connect(const IpAddress &address, uint16_t port);

        /* Connects to an AF_UNIX socket. socketPath is a filesystem path, or
           "@name" for the Linux abstract namespace. Throws where the platform
           has no usable local sockets. */
        TcpConnection connect(const std::string &socketPath);

      private:
        Dispatcher *dispatcher;

        void *context;
    };

} // namespace System
