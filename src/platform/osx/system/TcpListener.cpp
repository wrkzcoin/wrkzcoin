// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TcpListener.h"

#include "Dispatcher.h"
#include "TcpConnection.h"

#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system/ErrorMessage.h>
#include <system/InterruptedException.h>
#include <system/IpAddress.h>
#include <system/Ipv4Address.h>
#include <unistd.h>

namespace System
{
    TcpListener::TcpListener(): dispatcher(nullptr) {}

    TcpListener::TcpListener(Dispatcher &dispatcher, const Ipv4Address &addr, uint16_t port): dispatcher(&dispatcher)
    {
        std::string message;
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == -1)
        {
            message = "socket failed, " + lastErrorMessage();
        }
        else
        {
            int flags = fcntl(listener, F_GETFL, 0);
            if (flags == -1 || (fcntl(listener, F_SETFL, flags | O_NONBLOCK) == -1))
            {
                message = "fcntl failed, " + lastErrorMessage();
            }
            else
            {
                int on = 1;
                if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) == -1)
                {
                    message = "setsockopt failed, " + lastErrorMessage();
                }
                else
                {
                    sockaddr_in address;
                    address.sin_family = AF_INET;
                    address.sin_port = htons(port);
                    address.sin_addr.s_addr = htonl(addr.getValue());
                    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0)
                    {
                        message = "bind failed, " + lastErrorMessage();
                    }
                    else if (listen(listener, SOMAXCONN) != 0)
                    {
                        message = "listen failed, " + lastErrorMessage();
                    }
                    else
                    {
                        struct kevent event;
                        EV_SET(&event, listener, EVFILT_READ, EV_ADD | EV_DISABLE | EV_CLEAR, 0, SOMAXCONN, NULL);

                        if (kevent(dispatcher.getKqueue(), &event, 1, NULL, 0, NULL) == -1)
                        {
                            message = "kevent failed, " + lastErrorMessage();
                        }
                        else
                        {
                            context = nullptr;
                            return;
                        }
                    }
                }
            }

            if (close(listener) == -1)
            {
                message = "close failed, " + lastErrorMessage();
            }
        }

        throw std::runtime_error("TcpListener::TcpListener, " + message);
    }

    TcpListener::TcpListener(Dispatcher &dispatcher, const IpAddress &addr, uint16_t port): dispatcher(&dispatcher)
    {
        std::string message;
        int af = addr.isV6() ? AF_INET6 : AF_INET;
        listener = socket(af, SOCK_STREAM, IPPROTO_TCP);
        if (listener == -1)
        {
            message = "socket failed, " + lastErrorMessage();
        }
        else
        {
            int flags = fcntl(listener, F_GETFL, 0);
            if (flags == -1 || (fcntl(listener, F_SETFL, flags | O_NONBLOCK) == -1))
            {
                message = "fcntl failed, " + lastErrorMessage();
            }
            else
            {
                int on = 1;
                if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) == -1)
                {
                    message = "setsockopt failed, " + lastErrorMessage();
                }
                else
                {
                    bool bound = false;
                    if (addr.isV6())
                    {
                        int off = 0;
                        setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off);
                        sockaddr_in6 address6 = {};
                        address6.sin6_family = AF_INET6;
                        address6.sin6_port   = htons(port);
                        std::memcpy(&address6.sin6_addr, addr.getBytes(), 16);
                        bound = (bind(listener, reinterpret_cast<sockaddr *>(&address6), sizeof address6) == 0);
                    }
                    else
                    {
                        sockaddr_in address4 = {};
                        address4.sin_family      = AF_INET;
                        address4.sin_port        = htons(port);
                        address4.sin_addr.s_addr = htonl(addr.toV4());
                        bound = (bind(listener, reinterpret_cast<sockaddr *>(&address4), sizeof address4) == 0);
                    }

                    if (!bound)
                    {
                        message = "bind failed, " + lastErrorMessage();
                    }
                    else if (listen(listener, SOMAXCONN) != 0)
                    {
                        message = "listen failed, " + lastErrorMessage();
                    }
                    else
                    {
                        struct kevent event;
                        EV_SET(&event, listener, EVFILT_READ, EV_ADD | EV_DISABLE | EV_CLEAR, 0, SOMAXCONN, NULL);

                        if (kevent(dispatcher.getKqueue(), &event, 1, NULL, 0, NULL) == -1)
                        {
                            message = "kevent failed, " + lastErrorMessage();
                        }
                        else
                        {
                            context = nullptr;
                            return;
                        }
                    }
                }
            }

            if (close(listener) == -1)
            {
                message = "close failed, " + lastErrorMessage();
            }
        }

        throw std::runtime_error("TcpListener::TcpListener, " + message);
    }

    TcpListener::TcpListener(TcpListener &&other): dispatcher(other.dispatcher)
    {
        if (other.dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            listener = other.listener;
            context = nullptr;
            other.dispatcher = nullptr;
        }
    }

    TcpListener::~TcpListener()
    {
        if (dispatcher != nullptr)
        {
            assert(context == nullptr);
            int result = close(listener);
            if (result)
            {
            }
            assert(result != -1);
        }
    }

    TcpListener &TcpListener::operator=(TcpListener &&other)
    {
        if (dispatcher != nullptr)
        {
            assert(context == nullptr);
            if (close(listener) == -1)
            {
                throw std::runtime_error("TcpListener::operator=, close failed, " + lastErrorMessage());
            }
        }

        dispatcher = other.dispatcher;
        if (other.dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            listener = other.listener;
            context = nullptr;
            other.dispatcher = nullptr;
        }

        return *this;
    }

    TcpConnection TcpListener::accept()
    {
        assert(dispatcher != nullptr);
        assert(context == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        std::string message;
        OperationContext listenerContext;
        listenerContext.context = dispatcher->getCurrentContext();
        listenerContext.interrupted = false;
        struct kevent event;
        EV_SET(&event, listener, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR, 0, SOMAXCONN, &listenerContext);
        if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1)
        {
            message = "kevent failed, " + lastErrorMessage();
        }
        else
        {
            context = &listenerContext;
            dispatcher->getCurrentContext()->interruptProcedure = [&] {
                assert(dispatcher != nullptr);
                assert(context != nullptr);
                OperationContext *listenerContext = static_cast<OperationContext *>(context);
                if (!listenerContext->interrupted)
                {
                    struct kevent event;
                    EV_SET(&event, listener, EVFILT_READ, EV_DELETE | EV_DISABLE, 0, 0, NULL);

                    if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1)
                    {
                        throw std::runtime_error("TcpListener::stop, kevent failed, " + lastErrorMessage());
                    }

                    listenerContext->interrupted = true;
                    dispatcher->pushContext(listenerContext->context);
                }
            };

            dispatcher->dispatch();
            dispatcher->getCurrentContext()->interruptProcedure = nullptr;
            assert(dispatcher != nullptr);
            assert(listenerContext.context == dispatcher->getCurrentContext());
            assert(context == &listenerContext);
            context = nullptr;
            listenerContext.context = nullptr;
            if (listenerContext.interrupted)
            {
                throw InterruptedException();
            }

            sockaddr inAddr;
            socklen_t inLen = sizeof(inAddr);
            int connection = ::accept(listener, &inAddr, &inLen);
            if (connection == -1)
            {
                message = "accept failed, " + lastErrorMessage();
            }
            else
            {
                int flags = fcntl(connection, F_GETFL, 0);
                if (flags == -1 || fcntl(connection, F_SETFL, flags | O_NONBLOCK) == -1)
                {
                    message = "fcntl failed, " + lastErrorMessage();
                }
                else
                {
                    return TcpConnection(*dispatcher, connection);
                }
            }
        }

        throw std::runtime_error("TcpListener::accept, " + message);
    }

} // namespace System
