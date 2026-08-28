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

    class TcpListener
    {
      public:
        TcpListener();

        TcpListener(Dispatcher &dispatcher, const Ipv4Address &address, uint16_t port);

        TcpListener(Dispatcher &dispatcher, const IpAddress &address, uint16_t port);

        /* Listens on an AF_UNIX socket. socketPath is a filesystem path, or
           "@name" for the Linux abstract namespace, and socketMode is the
           octal permission triple the socket file is created with - it is
           applied through the process umask so the socket is never reachable
           more widely than asked for, not even between bind and chmod.
           Throws where the platform has no usable local sockets. */
        TcpListener(Dispatcher &dispatcher, const std::string &socketPath, uint32_t socketMode);

        TcpListener(const TcpListener &) = delete;

        TcpListener(TcpListener &&other);

        ~TcpListener();

        TcpListener &operator=(const TcpListener &) = delete;

        TcpListener &operator=(TcpListener &&other);

        TcpConnection accept();

      private:
        Dispatcher *dispatcher;

        int listener;

        void *context;
    };

} // namespace System
