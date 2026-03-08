// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TcpConnector.h"

#include <cassert>
#include <cstring>
#include <stdexcept>

// clang-format off
/* Order of includes is important here, because, you know, *windows* Â¯\_(ãƒ„)_/Â¯ */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
// clang-format on

#include "Dispatcher.h"
#include "ErrorMessage.h"
#include "TcpConnection.h"

#include <system/IpAddress.h>
#include <system/InterruptedException.h>
#include <system/Ipv4Address.h>

namespace System
{
    namespace
    {
        struct TcpConnectorContext : public OVERLAPPED
        {
            NativeContext *context;
            size_t connection;
            bool interrupted;
        };

        LPFN_CONNECTEX connectEx = nullptr;

    } // namespace

    TcpConnector::TcpConnector(): dispatcher(nullptr) {}

    TcpConnector::TcpConnector(Dispatcher &dispatcher): dispatcher(&dispatcher), context(nullptr) {}

    TcpConnector::TcpConnector(TcpConnector &&other): dispatcher(other.dispatcher)
    {
        if (dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            context = nullptr;
            other.dispatcher = nullptr;
        }
    }

    TcpConnector::~TcpConnector()
    {
        assert(dispatcher == nullptr || context == nullptr);
    }

    TcpConnector &TcpConnector::operator=(TcpConnector &&other)
    {
        assert(dispatcher == nullptr || context == nullptr);
        dispatcher = other.dispatcher;
        if (dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            context = nullptr;
            other.dispatcher = nullptr;
        }

        return *this;
    }

    TcpConnection TcpConnector::connect(const Ipv4Address &address, uint16_t port)
    {
        assert(dispatcher != nullptr);
        assert(context == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        std::string message;
        SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connection == INVALID_SOCKET)
        {
            message = "socket failed, " + errorMessage(WSAGetLastError());
        }
        else
        {
            sockaddr_in bindAddress;
            bindAddress.sin_family = AF_INET;
            bindAddress.sin_port = 0;
            bindAddress.sin_addr.s_addr = INADDR_ANY;
            if (bind(connection, reinterpret_cast<sockaddr *>(&bindAddress), sizeof bindAddress) != 0)
            {
                message = "bind failed, " + errorMessage(WSAGetLastError());
            }
            else
            {
                GUID guidConnectEx = WSAID_CONNECTEX;
                DWORD read = sizeof connectEx;
                if (connectEx == nullptr
                    && WSAIoctl(
                           connection,
                           SIO_GET_EXTENSION_FUNCTION_POINTER,
                           &guidConnectEx,
                           sizeof guidConnectEx,
                           &connectEx,
                           sizeof connectEx,
                           &read,
                           NULL,
                           NULL)
                           != 0)
                {
                    message = "WSAIoctl failed, " + errorMessage(WSAGetLastError());
                }
                else
                {
                    assert(read == sizeof connectEx);
                    if (CreateIoCompletionPort(
                            reinterpret_cast<HANDLE>(connection), dispatcher->getCompletionPort(), 0, 0)
                        != dispatcher->getCompletionPort())
                    {
                        message = "CreateIoCompletionPort failed, " + lastErrorMessage();
                    }
                    else
                    {
                        sockaddr_in addressData;
                        addressData.sin_family = AF_INET;
                        addressData.sin_port = htons(port);
                        addressData.sin_addr.S_un.S_addr = htonl(address.getValue());
                        TcpConnectorContext context2;
                        context2.hEvent = NULL;
                        if (connectEx(
                                connection,
                                reinterpret_cast<sockaddr *>(&addressData),
                                sizeof addressData,
                                NULL,
                                0,
                                NULL,
                                &context2)
                            == TRUE)
                        {
                            message = "ConnectEx returned immediately, which is not supported.";
                        }
                        else
                        {
                            int lastError = WSAGetLastError();
                            if (lastError != WSA_IO_PENDING)
                            {
                                message = "ConnectEx failed, " + errorMessage(lastError);
                            }
                            else
                            {
                                context2.context = dispatcher->getCurrentContext();
                                context2.connection = connection;
                                context2.interrupted = false;
                                context = &context2;
                                dispatcher->getCurrentContext()->interruptProcedure = [&]() {
                                    assert(dispatcher != nullptr);
                                    assert(context != nullptr);
                                    TcpConnectorContext *context2 = static_cast<TcpConnectorContext *>(context);
                                    if (!context2->interrupted)
                                    {
                                        if (CancelIoEx(reinterpret_cast<HANDLE>(context2->connection), context2)
                                            != TRUE)
                                        {
                                            DWORD lastError = GetLastError();
                                            if (lastError != ERROR_NOT_FOUND)
                                            {
                                                throw std::runtime_error(
                                                    "TcpConnector::stop, CancelIoEx failed, " + lastErrorMessage());
                                            }

                                            context2->context->interrupted = true;
                                        }

                                        context2->interrupted = true;
                                    }
                                };

                                dispatcher->dispatch();
                                dispatcher->getCurrentContext()->interruptProcedure = nullptr;
                                assert(context2.context == dispatcher->getCurrentContext());
                                assert(context2.connection == connection);
                                assert(dispatcher != nullptr);
                                assert(context == &context2);
                                context = nullptr;
                                DWORD transferred;
                                DWORD flags;
                                if (WSAGetOverlappedResult(connection, &context2, &transferred, FALSE, &flags) != TRUE)
                                {
                                    lastError = WSAGetLastError();
                                    if (lastError != ERROR_OPERATION_ABORTED)
                                    {
                                        message = "ConnectEx failed, " + errorMessage(lastError);
                                    }
                                    else
                                    {
                                        assert(context2.interrupted);
                                        if (closesocket(connection) != 0)
                                        {
                                            throw std::runtime_error(
                                                "TcpConnector::connect, closesocket failed, "
                                                + errorMessage(WSAGetLastError()));
                                        }
                                        else
                                        {
                                            throw InterruptedException();
                                        }
                                    }
                                }
                                else
                                {
                                    if (context2.interrupted)
                                    {
                                        if (closesocket(connection) != 0)
                                        {
                                            throw std::runtime_error(
                                                "TcpConnector::connect, closesocket failed, "
                                                + errorMessage(WSAGetLastError()));
                                        }
                                        else
                                        {
                                            throw InterruptedException();
                                        }
                                    }

                                    assert(transferred == 0);
                                    assert(flags == 0);
                                    DWORD value = 1;
                                    if (setsockopt(
                                            connection,
                                            SOL_SOCKET,
                                            SO_UPDATE_CONNECT_CONTEXT,
                                            reinterpret_cast<char *>(&value),
                                            sizeof(value))
                                        != 0)
                                    {
                                        message = "setsockopt failed, " + errorMessage(WSAGetLastError());
                                    }
                                    else
                                    {
                                        return TcpConnection(*dispatcher, connection);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            int result = closesocket(connection);
            assert(result == 0);
        }

        throw std::runtime_error("TcpConnector::connect, " + message);
    }

    TcpConnection TcpConnector::connect(const IpAddress &address, uint16_t port)
    {
        assert(dispatcher != nullptr);
        assert(context == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        std::string message;
        int af = address.isV6() ? AF_INET6 : AF_INET;
        SOCKET connection = socket(af, SOCK_STREAM, IPPROTO_TCP);
        if (connection == INVALID_SOCKET)
        {
            message = "socket failed, " + errorMessage(WSAGetLastError());
        }
        else
        {
            // ConnectEx requires the socket to be bound first
            bool bindOk = false;
            if (address.isV6())
            {
                sockaddr_in6 bindAddr = {};
                bindAddr.sin6_family = AF_INET6;
                bindAddr.sin6_port   = 0;
                bindAddr.sin6_addr   = in6addr_any;
                bindOk = (bind(connection, reinterpret_cast<sockaddr *>(&bindAddr), sizeof bindAddr) == 0);
            }
            else
            {
                sockaddr_in bindAddr = {};
                bindAddr.sin_family      = AF_INET;
                bindAddr.sin_port        = 0;
                bindAddr.sin_addr.s_addr = INADDR_ANY;
                bindOk = (bind(connection, reinterpret_cast<sockaddr *>(&bindAddr), sizeof bindAddr) == 0);
            }

            if (!bindOk)
            {
                message = "bind failed, " + errorMessage(WSAGetLastError());
            }
            else
            {
                GUID guidConnectEx = WSAID_CONNECTEX;
                DWORD read = sizeof connectEx;
                if (connectEx == nullptr
                    && WSAIoctl(
                           connection,
                           SIO_GET_EXTENSION_FUNCTION_POINTER,
                           &guidConnectEx,
                           sizeof guidConnectEx,
                           &connectEx,
                           sizeof connectEx,
                           &read,
                           NULL,
                           NULL)
                           != 0)
                {
                    message = "WSAIoctl failed, " + errorMessage(WSAGetLastError());
                }
                else
                {
                    assert(read == sizeof connectEx);
                    if (CreateIoCompletionPort(
                            reinterpret_cast<HANDLE>(connection), dispatcher->getCompletionPort(), 0, 0)
                        != dispatcher->getCompletionPort())
                    {
                        message = "CreateIoCompletionPort failed, " + lastErrorMessage();
                    }
                    else
                    {
                        sockaddr_storage addrStorage = {};
                        int addrLen;
                        if (address.isV6())
                        {
                            sockaddr_in6 *a6 = reinterpret_cast<sockaddr_in6 *>(&addrStorage);
                            a6->sin6_family = AF_INET6;
                            a6->sin6_port   = htons(port);
                            std::memcpy(&a6->sin6_addr, address.getBytes(), 16);
                            addrLen = sizeof(sockaddr_in6);
                        }
                        else
                        {
                            sockaddr_in *a4 = reinterpret_cast<sockaddr_in *>(&addrStorage);
                            a4->sin_family      = AF_INET;
                            a4->sin_port        = htons(port);
                            a4->sin_addr.S_un.S_addr = htonl(address.toV4());
                            addrLen = sizeof(sockaddr_in);
                        }

                        TcpConnectorContext context2;
                        context2.hEvent = NULL;
                        if (connectEx(
                                connection,
                                reinterpret_cast<sockaddr *>(&addrStorage),
                                addrLen,
                                NULL,
                                0,
                                NULL,
                                &context2)
                            == TRUE)
                        {
                            message = "ConnectEx returned immediately, which is not supported.";
                        }
                        else
                        {
                            int lastError = WSAGetLastError();
                            if (lastError != WSA_IO_PENDING)
                            {
                                message = "ConnectEx failed, " + errorMessage(lastError);
                            }
                            else
                            {
                                context2.context     = dispatcher->getCurrentContext();
                                context2.connection  = connection;
                                context2.interrupted = false;
                                context = &context2;
                                dispatcher->getCurrentContext()->interruptProcedure = [&]() {
                                    assert(dispatcher != nullptr);
                                    assert(context != nullptr);
                                    TcpConnectorContext *ctx = static_cast<TcpConnectorContext *>(context);
                                    if (!ctx->interrupted)
                                    {
                                        if (CancelIoEx(reinterpret_cast<HANDLE>(ctx->connection), ctx) != TRUE)
                                        {
                                            DWORD err = GetLastError();
                                            if (err != ERROR_NOT_FOUND)
                                            {
                                                throw std::runtime_error(
                                                    "TcpConnector::connect(IpAddress), CancelIoEx failed, "
                                                    + lastErrorMessage());
                                            }
                                            ctx->context->interrupted = true;
                                        }
                                        ctx->interrupted = true;
                                    }
                                };

                                dispatcher->dispatch();
                                dispatcher->getCurrentContext()->interruptProcedure = nullptr;
                                assert(context2.context == dispatcher->getCurrentContext());
                                assert(context2.connection == connection);
                                assert(dispatcher != nullptr);
                                assert(context == &context2);
                                context = nullptr;
                                DWORD transferred;
                                DWORD flags;
                                if (WSAGetOverlappedResult(connection, &context2, &transferred, FALSE, &flags) != TRUE)
                                {
                                    lastError = WSAGetLastError();
                                    if (lastError != ERROR_OPERATION_ABORTED)
                                    {
                                        message = "ConnectEx failed, " + errorMessage(lastError);
                                    }
                                    else
                                    {
                                        assert(context2.interrupted);
                                        if (closesocket(connection) != 0)
                                        {
                                            throw std::runtime_error(
                                                "TcpConnector::connect(IpAddress), closesocket failed, "
                                                + errorMessage(WSAGetLastError()));
                                        }
                                        else
                                        {
                                            throw InterruptedException();
                                        }
                                    }
                                }
                                else
                                {
                                    if (context2.interrupted)
                                    {
                                        if (closesocket(connection) != 0)
                                        {
                                            throw std::runtime_error(
                                                "TcpConnector::connect(IpAddress), closesocket failed, "
                                                + errorMessage(WSAGetLastError()));
                                        }
                                        else
                                        {
                                            throw InterruptedException();
                                        }
                                    }

                                    assert(transferred == 0);
                                    assert(flags == 0);
                                    DWORD value = 1;
                                    if (setsockopt(
                                            connection,
                                            SOL_SOCKET,
                                            SO_UPDATE_CONNECT_CONTEXT,
                                            reinterpret_cast<char *>(&value),
                                            sizeof(value))
                                        != 0)
                                    {
                                        message = "setsockopt failed, " + errorMessage(WSAGetLastError());
                                    }
                                    else
                                    {
                                        return TcpConnection(*dispatcher, connection);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            int result = closesocket(connection);
            assert(result == 0);
        }

        throw std::runtime_error("TcpConnector::connect(IpAddress), " + message);
    }

} // namespace System
