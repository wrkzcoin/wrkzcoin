// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TcpConnection.h"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <system/ErrorMessage.h>
#include <system/IpAddress.h>
#include <system/InterruptedException.h>
#include <system/Ipv4Address.h>
#include <unistd.h>

namespace System
{
    TcpConnection::TcpConnection(): dispatcher(nullptr) {}

    TcpConnection::TcpConnection(TcpConnection &&other): dispatcher(other.dispatcher)
    {
        if (other.dispatcher != nullptr)
        {
            assert(other.contextPair.writeContext == nullptr);
            assert(other.contextPair.readContext == nullptr);
            connection = other.connection;
            contextPair = other.contextPair;
            other.dispatcher = nullptr;
        }
    }

    TcpConnection::~TcpConnection()
    {
        if (dispatcher != nullptr)
        {
            assert(contextPair.readContext == nullptr);
            assert(contextPair.writeContext == nullptr);
            int result = close(connection);
            if (result)
            {
            }
            assert(result != -1);
        }
    }

    TcpConnection &TcpConnection::operator=(TcpConnection &&other)
    {
        if (dispatcher != nullptr)
        {
            assert(contextPair.readContext == nullptr);
            assert(contextPair.writeContext == nullptr);
            if (close(connection) == -1)
            {
                throw std::runtime_error("TcpConnection::operator=, close failed, " + lastErrorMessage());
            }
        }

        dispatcher = other.dispatcher;
        if (other.dispatcher != nullptr)
        {
            assert(other.contextPair.readContext == nullptr);
            assert(other.contextPair.writeContext == nullptr);
            connection = other.connection;
            contextPair = other.contextPair;
            other.dispatcher = nullptr;
        }

        return *this;
    }

    size_t TcpConnection::read(uint8_t *data, size_t size)
    {
        assert(dispatcher != nullptr);
        assert(contextPair.readContext == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        std::string message;
        ssize_t transferred = ::recv(connection, (void *)data, size, 0);
        if (transferred == -1)
        {
            bool knownError = false;

            if (errno == EAGAIN)
            {
                knownError = true;
            }

            if (errno == EWOULDBLOCK)
            {
                knownError = true;
            }

            if (!knownError)
            {
                message = "recv failed, " + lastErrorMessage();
            }
            else
            {
                epoll_event connectionEvent;
                OperationContext operationContext;
                operationContext.interrupted = false;
                operationContext.context = dispatcher->getCurrentContext();
                contextPair.readContext = &operationContext;
                connectionEvent.data.ptr = &contextPair;

                if (contextPair.writeContext != nullptr)
                {
                    connectionEvent.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
                }
                else
                {
                    connectionEvent.events = EPOLLIN | EPOLLONESHOT;
                }

                if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1)
                {
                    message = "epoll_ctl failed, " + lastErrorMessage();
                }
                else
                {
                    dispatcher->getCurrentContext()->interruptProcedure = [&]() {
                        assert(dispatcher != nullptr);
                        assert(contextPair.readContext != nullptr);
                        epoll_event connectionEvent;
                        connectionEvent.events = EPOLLONESHOT;
                        connectionEvent.data.ptr = nullptr;

                        if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1)
                        {
                            throw std::runtime_error(
                                "TcpConnection::read, interrupt procedure, epoll_ctl failed, " + lastErrorMessage());
                        }

                        contextPair.readContext->interrupted = true;
                        dispatcher->pushContext(contextPair.readContext->context);
                    };

                    dispatcher->dispatch();
                    dispatcher->getCurrentContext()->interruptProcedure = nullptr;
                    assert(dispatcher != nullptr);
                    assert(operationContext.context == dispatcher->getCurrentContext());
                    assert(contextPair.readContext == &operationContext);

                    if (operationContext.interrupted)
                    {
                        contextPair.readContext = nullptr;
                        throw InterruptedException();
                    }

                    contextPair.readContext = nullptr;
                    if (contextPair.writeContext != nullptr)
                    { // write is presented, rearm
                        epoll_event connectionEvent;
                        connectionEvent.events = EPOLLOUT | EPOLLONESHOT;
                        connectionEvent.data.ptr = &contextPair;

                        if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1)
                        {
                            message = "epoll_ctl failed, " + lastErrorMessage();
                            throw std::runtime_error("TcpConnection::read");
                        }
                    }

                    if ((operationContext.events & (EPOLLERR | EPOLLHUP)) != 0)
                    {
                        throw std::runtime_error("TcpConnection::read");
                    }

                    ssize_t transferred = ::recv(connection, (void *)data, size, 0);
                    if (transferred == -1)
                    {
                        message = "recv failed, " + lastErrorMessage();
                    }
                    else
                    {
                        assert(transferred <= static_cast<ssize_t>(size));
                        return transferred;
                    }
                }
            }

            throw std::runtime_error("TcpConnection::read, " + message);
        }

        assert(transferred <= static_cast<ssize_t>(size));
        return transferred;
    }

    std::size_t TcpConnection::write(const uint8_t *data, size_t size)
    {
        assert(dispatcher != nullptr);
        assert(contextPair.writeContext == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        std::string message;
        if (size == 0)
        {
            if (shutdown(connection, SHUT_WR) == -1)
            {
                throw std::runtime_error("TcpConnection::write, shutdown failed, " + lastErrorMessage());
            }

            return 0;
        }

        ssize_t transferred = ::send(connection, (void *)data, size, MSG_NOSIGNAL);
        if (transferred == -1)
        {
            bool knownError = false;

            if (errno == EAGAIN)
            {
                knownError = true;
            }

            if (errno == EWOULDBLOCK)
            {
                knownError = true;
            }

            if (!knownError)
            {
                message = "send failed, " + lastErrorMessage();
            }
            else
            {
                epoll_event connectionEvent;
                OperationContext operationContext;
                operationContext.interrupted = false;
                operationContext.context = dispatcher->getCurrentContext();
                contextPair.writeContext = &operationContext;
                connectionEvent.data.ptr = &contextPair;

                if (contextPair.readContext != nullptr)
                {
                    connectionEvent.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
                }
                else
                {
                    connectionEvent.events = EPOLLOUT | EPOLLONESHOT;
                }

                if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1)
                {
                    message = "epoll_ctl failed, " + lastErrorMessage();
                }
                else
                {
                    dispatcher->getCurrentContext()->interruptProcedure = [&]() {
                        assert(dispatcher != nullptr);
                        assert(contextPair.writeContext != nullptr);
                        epoll_event connectionEvent;
                        connectionEvent.events = EPOLLONESHOT;
                        connectionEvent.data.ptr = nullptr;

                        if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1)
                        {
                            throw std::runtime_error(
                                "TcpConnection::write, interrupt procedure, epoll_ctl failed, " + lastErrorMessage());
                        }

                        contextPair.writeContext->interrupted = true;
                        dispatcher->pushContext(contextPair.writeContext->context);
                    };

                    dispatcher->dispatch();
                    dispatcher->getCurrentContext()->interruptProcedure = nullptr;
                    assert(dispatcher != nullptr);
                    assert(operationContext.context == dispatcher->getCurrentContext());
                    assert(contextPair.writeContext == &operationContext);

                    if (operationContext.interrupted)
                    {
                        contextPair.writeContext = nullptr;
                        throw InterruptedException();
                    }

                    contextPair.writeContext = nullptr;
                    if (contextPair.readContext != nullptr)
                    { // read is presented, rearm
                        epoll_event connectionEvent;
                        connectionEvent.events = EPOLLIN | EPOLLONESHOT;
                        connectionEvent.data.ptr = &contextPair;

                        if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, connection, &connectionEvent) == -1)
                        {
                            message = "epoll_ctl failed, " + lastErrorMessage();
                            throw std::runtime_error("TcpConnection::write, " + message);
                        }
                    }

                    if ((operationContext.events & (EPOLLERR | EPOLLHUP)) != 0)
                    {
                        throw std::runtime_error("TcpConnection::write, events & (EPOLLERR | EPOLLHUP) != 0");
                    }

                    ssize_t transferred = ::send(connection, (void *)data, size, 0);
                    if (transferred == -1)
                    {
                        message = "send failed, " + lastErrorMessage();
                    }
                    else
                    {
                        assert(transferred <= static_cast<ssize_t>(size));
                        return transferred;
                    }
                }
            }

            throw std::runtime_error("TcpConnection::write, " + message);
        }

        assert(transferred <= static_cast<ssize_t>(size));
        return transferred;
    }

    std::pair<Ipv4Address, uint16_t> TcpConnection::getPeerAddressAndPort() const
    {
        sockaddr_storage addr;
        socklen_t size = sizeof(addr);
        if (getpeername(connection, reinterpret_cast<sockaddr *>(&addr), &size) != 0)
        {
            throw std::runtime_error("TcpConnection::getPeerAddress, getpeername failed, " + lastErrorMessage());
        }

        if (addr.ss_family == AF_INET)
        {
            const auto &in4 = reinterpret_cast<const sockaddr_in &>(addr);
            return std::make_pair(Ipv4Address(ntohl(in4.sin_addr.s_addr)), ntohs(in4.sin_port));
        }
        else if (addr.ss_family == AF_INET6)
        {
            const auto &in6 = reinterpret_cast<const sockaddr_in6 &>(addr);
            const uint8_t *b = in6.sin6_addr.s6_addr;
            // Handle IPv4-mapped IPv6 (::ffff:x.x.x.x) transparently
            if (b[0]==0&&b[1]==0&&b[2]==0&&b[3]==0&&b[4]==0&&b[5]==0&&
                b[6]==0&&b[7]==0&&b[8]==0&&b[9]==0&&b[10]==0xff&&b[11]==0xff)
            {
                uint32_t v4 = (uint32_t(b[12])<<24)|(uint32_t(b[13])<<16)|(uint32_t(b[14])<<8)|b[15];
                return std::make_pair(Ipv4Address(v4), ntohs(in6.sin6_port));
            }
            // Pure IPv6: return port with zero IPv4 (caller should use getPeerIpAddress())
            return std::make_pair(Ipv4Address(0), ntohs(in6.sin6_port));
        }
        throw std::runtime_error("TcpConnection::getPeerAddressAndPort, unknown address family");
    }

    IpAddress TcpConnection::getPeerIpAddress() const
    {
        sockaddr_storage addr;
        socklen_t size = sizeof(addr);
        if (getpeername(connection, reinterpret_cast<sockaddr *>(&addr), &size) != 0)
        {
            throw std::runtime_error("TcpConnection::getPeerIpAddress, getpeername failed, " + lastErrorMessage());
        }

        if (addr.ss_family == AF_INET)
        {
            return IpAddress(ntohl(reinterpret_cast<const sockaddr_in &>(addr).sin_addr.s_addr));
        }
        else if (addr.ss_family == AF_INET6)
        {
            return IpAddress(reinterpret_cast<const sockaddr_in6 &>(addr).sin6_addr.s6_addr);
        }
        throw std::runtime_error("TcpConnection::getPeerIpAddress, unknown address family");
    }

    TcpConnection::TcpConnection(Dispatcher &dispatcher, int socket): dispatcher(&dispatcher), connection(socket)
    {
        contextPair.readContext = nullptr;
        contextPair.writeContext = nullptr;

        /* The Levin protocol is a strict request/response exchange of mostly
         * small messages. Nagle holds back the trailing partial segment of a
         * write until the previous one is acked, which combined with the peer's
         * delayed ack adds a fixed stall to every round trip. Not fatal, but on
         * a sync that is tens of thousands of round trips it adds up. Failure
         * here is not worth aborting a working connection over. */
        const int nodelay = 1;
        setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof nodelay);

        epoll_event connectionEvent;
        connectionEvent.events = EPOLLONESHOT;
        connectionEvent.data.ptr = nullptr;

        if (epoll_ctl(dispatcher.getEpoll(), EPOLL_CTL_ADD, socket, &connectionEvent) == -1)
        {
            throw std::runtime_error("TcpConnection::TcpConnection, epoll_ctl failed, " + lastErrorMessage());
        }
    }

} // namespace System
