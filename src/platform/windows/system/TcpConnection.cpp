// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TcpConnection.h"

#include <cassert>
#include <stdexcept>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
/* Order of includes is important here, because, you know, *windows* Â¯\_(ãƒ„)_/Â¯ */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
// clang-format on

#include "Dispatcher.h"
#include "ErrorMessage.h"

#include <system/IpAddress.h>
#include <system/InterruptedException.h>
#include <system/Ipv4Address.h>

namespace System
{
    namespace
    {
        struct TcpConnectionContext : public OVERLAPPED
        {
            NativeContext *context;
            bool interrupted;
        };

    } // namespace

    TcpConnection::TcpConnection(): dispatcher(nullptr) {}

    TcpConnection::TcpConnection(TcpConnection &&other): dispatcher(other.dispatcher)
    {
        if (dispatcher != nullptr)
        {
            assert(other.readContext == nullptr);
            assert(other.writeContext == nullptr);
            connection = other.connection;
            readContext = nullptr;
            writeContext = nullptr;
            other.dispatcher = nullptr;
        }
    }

    TcpConnection::~TcpConnection()
    {
        if (dispatcher != nullptr)
        {
            assert(readContext == nullptr);
            assert(writeContext == nullptr);
            int result = closesocket(connection);
            assert(result == 0);
        }
    }

    TcpConnection &TcpConnection::operator=(TcpConnection &&other)
    {
        if (dispatcher != nullptr)
        {
            assert(readContext == nullptr);
            assert(writeContext == nullptr);
            if (closesocket(connection) != 0)
            {
                throw std::runtime_error(
                    "TcpConnection::operator=, closesocket failed, " + errorMessage(WSAGetLastError()));
            }
        }

        dispatcher = other.dispatcher;
        if (dispatcher != nullptr)
        {
            assert(other.readContext == nullptr);
            assert(other.writeContext == nullptr);
            connection = other.connection;
            readContext = nullptr;
            writeContext = nullptr;
            other.dispatcher = nullptr;
        }

        return *this;
    }

    size_t TcpConnection::read(uint8_t *data, size_t size)
    {
        assert(dispatcher != nullptr);
        assert(readContext == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        WSABUF buf {static_cast<ULONG>(size), reinterpret_cast<char *>(data)};
        DWORD flags = 0;
        TcpConnectionContext context;
        context.hEvent = NULL;
        if (WSARecv(connection, &buf, 1, NULL, &flags, &context, NULL) != 0)
        {
            int lastError = WSAGetLastError();
            if (lastError != WSA_IO_PENDING)
            {
                throw std::runtime_error("TcpConnection::read, WSARecv failed, " + errorMessage(lastError));
            }
        }

        assert(flags == 0);
        context.context = dispatcher->getCurrentContext();
        context.interrupted = false;
        readContext = &context;
        dispatcher->getCurrentContext()->interruptProcedure = [&]() {
            assert(dispatcher != nullptr);
            assert(readContext != nullptr);
            TcpConnectionContext *context = static_cast<TcpConnectionContext *>(readContext);
            if (!context->interrupted)
            {
                if (CancelIoEx(reinterpret_cast<HANDLE>(connection), context) != TRUE)
                {
                    DWORD lastError = GetLastError();
                    if (lastError != ERROR_NOT_FOUND)
                    {
                        throw std::runtime_error("TcpConnection::stop, CancelIoEx failed, " + lastErrorMessage());
                    }

                    context->context->interrupted = true;
                }

                context->interrupted = true;
            }
        };

        dispatcher->dispatch();
        dispatcher->getCurrentContext()->interruptProcedure = nullptr;
        assert(context.context == dispatcher->getCurrentContext());
        assert(dispatcher != nullptr);
        assert(readContext == &context);
        readContext = nullptr;
        DWORD transferred;
        if (WSAGetOverlappedResult(connection, &context, &transferred, FALSE, &flags) != TRUE)
        {
            int lastError = WSAGetLastError();
            if (lastError != ERROR_OPERATION_ABORTED)
            {
                throw std::runtime_error(
                    "TcpConnection::read, WSAGetOverlappedResult failed, " + errorMessage(lastError));
            }

            assert(context.interrupted);
            throw InterruptedException();
        }

        if (context.interrupted)
        {
            throw InterruptedException();
        }

        assert(transferred <= size);
        assert(flags == 0);
        return transferred;
    }

    size_t TcpConnection::write(const uint8_t *data, size_t size)
    {
        assert(dispatcher != nullptr);
        assert(writeContext == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        if (size == 0)
        {
            if (shutdown(connection, SD_SEND) != 0)
            {
                throw std::runtime_error("TcpConnection::write, shutdown failed, " + errorMessage(WSAGetLastError()));
            }

            return 0;
        }

        WSABUF buf {static_cast<ULONG>(size), reinterpret_cast<char *>(const_cast<uint8_t *>(data))};
        TcpConnectionContext context;
        context.hEvent = NULL;
        if (WSASend(connection, &buf, 1, NULL, 0, &context, NULL) != 0)
        {
            int lastError = WSAGetLastError();
            if (lastError != WSA_IO_PENDING)
            {
                throw std::runtime_error("TcpConnection::write, WSASend failed, " + errorMessage(lastError));
            }
        }

        context.context = dispatcher->getCurrentContext();
        context.interrupted = false;
        writeContext = &context;
        dispatcher->getCurrentContext()->interruptProcedure = [&]() {
            assert(dispatcher != nullptr);
            assert(writeContext != nullptr);
            TcpConnectionContext *context = static_cast<TcpConnectionContext *>(writeContext);
            if (!context->interrupted)
            {
                if (CancelIoEx(reinterpret_cast<HANDLE>(connection), context) != TRUE)
                {
                    DWORD lastError = GetLastError();
                    if (lastError != ERROR_NOT_FOUND)
                    {
                        throw std::runtime_error("TcpConnection::stop, CancelIoEx failed, " + lastErrorMessage());
                    }

                    context->context->interrupted = true;
                }

                context->interrupted = true;
            }
        };

        dispatcher->dispatch();
        dispatcher->getCurrentContext()->interruptProcedure = nullptr;
        assert(context.context == dispatcher->getCurrentContext());
        assert(dispatcher != nullptr);
        assert(writeContext == &context);
        writeContext = nullptr;
        DWORD transferred;
        DWORD flags;
        if (WSAGetOverlappedResult(connection, &context, &transferred, FALSE, &flags) != TRUE)
        {
            int lastError = WSAGetLastError();
            if (lastError != ERROR_OPERATION_ABORTED)
            {
                throw std::runtime_error(
                    "TcpConnection::write, WSAGetOverlappedResult failed, " + errorMessage(lastError));
            }

            assert(context.interrupted);
            throw InterruptedException();
        }

        if (context.interrupted)
        {
            throw InterruptedException();
        }

        assert(transferred == size);
        assert(flags == 0);
        return transferred;
    }

    std::pair<Ipv4Address, uint16_t> TcpConnection::getPeerAddressAndPort() const
    {
        sockaddr_storage addr;
        int size = sizeof(addr);
        if (getpeername(static_cast<SOCKET>(connection), reinterpret_cast<sockaddr *>(&addr), &size) != 0)
        {
            throw std::runtime_error(
                "TcpConnection::getPeerAddress, getpeername failed, " + errorMessage(WSAGetLastError()));
        }

        if (addr.ss_family == AF_INET)
        {
            const auto &in4 = reinterpret_cast<const sockaddr_in &>(addr);
            return std::make_pair(Ipv4Address(ntohl(in4.sin_addr.S_un.S_addr)), ntohs(in4.sin_port));
        }
        else if (addr.ss_family == AF_INET6)
        {
            const auto &in6 = reinterpret_cast<const sockaddr_in6 &>(addr);
            const uint8_t *b = in6.sin6_addr.s6_addr;
            if (b[0]==0&&b[1]==0&&b[2]==0&&b[3]==0&&b[4]==0&&b[5]==0&&
                b[6]==0&&b[7]==0&&b[8]==0&&b[9]==0&&b[10]==0xff&&b[11]==0xff)
            {
                uint32_t v4 = (uint32_t(b[12])<<24)|(uint32_t(b[13])<<16)|(uint32_t(b[14])<<8)|b[15];
                return std::make_pair(Ipv4Address(v4), ntohs(in6.sin6_port));
            }
            return std::make_pair(Ipv4Address(0), ntohs(in6.sin6_port));
        }
        throw std::runtime_error("TcpConnection::getPeerAddressAndPort, unknown address family");
    }

    IpAddress TcpConnection::getPeerIpAddress() const
    {
        sockaddr_storage addr;
        int size = sizeof(addr);
        if (getpeername(static_cast<SOCKET>(connection), reinterpret_cast<sockaddr *>(&addr), &size) != 0)
        {
            throw std::runtime_error(
                "TcpConnection::getPeerIpAddress, getpeername failed, " + errorMessage(WSAGetLastError()));
        }

        if (addr.ss_family == AF_INET)
        {
            return IpAddress(ntohl(reinterpret_cast<const sockaddr_in &>(addr).sin_addr.S_un.S_addr));
        }
        else if (addr.ss_family == AF_INET6)
        {
            return IpAddress(reinterpret_cast<const sockaddr_in6 &>(addr).sin6_addr.s6_addr);
        }
        throw std::runtime_error("TcpConnection::getPeerIpAddress, unknown address family");
    }

    TcpConnection::TcpConnection(Dispatcher &dispatcher, size_t connection):
        dispatcher(&dispatcher),
        connection(connection),
        readContext(nullptr),
        writeContext(nullptr)
    {
        /* See the linux implementation - Nagle adds a fixed stall to every
         * Levin round trip, and sync is made of round trips. */
        const BOOL nodelay = TRUE;
        setsockopt(
            static_cast<SOCKET>(connection),
            IPPROTO_TCP,
            TCP_NODELAY,
            reinterpret_cast<const char *>(&nodelay),
            sizeof nodelay);
    }

} // namespace System
