// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "HttpServer.h"

#include <boost/scope_exit.hpp>
#include <http/HttpParser.h>
#include <memory>
#include <system/InterruptedException.h>
#include <system/IpAddress.h>
#include <system/TcpStream.h>

using namespace Logging;

namespace CryptoNote
{
    HttpServer::HttpServer(System::Dispatcher &dispatcher, std::shared_ptr<Logging::ILogger> log):
        m_dispatcher(dispatcher),
        workingContextGroup(dispatcher),
        logger(log, "HttpServer")
    {
    }

    void HttpServer::start(const std::string &address, uint16_t port)
    {
        m_listener = System::TcpListener(m_dispatcher, System::IpAddress(address), port);
        workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));
    }

    void HttpServer::stop()
    {
        workingContextGroup.interrupt();
        workingContextGroup.wait();
    }

    void HttpServer::acceptLoop()
    {
        try
        {
            System::TcpConnection connection;
            bool accepted = false;

            while (!accepted)
            {
                try
                {
                    connection = m_listener.accept();
                    accepted = true;
                }
                catch (System::InterruptedException &)
                {
                    throw;
                }
                catch (std::exception &)
                {
                    // try again
                }
            }

            m_connections.insert(&connection);
            BOOST_SCOPE_EXIT_ALL(this, &connection)
            {
                m_connections.erase(&connection);
            };

            workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

            auto peerIp = connection.getPeerIpAddress().toString();
            auto peerPort = connection.getPeerAddressAndPort().second;

            logger(DEBUGGING) << "Incoming connection from " << peerIp << ":" << peerPort;

            System::TcpStreambuf streambuf(connection);
            std::iostream stream(&streambuf);
            HttpParser parser;

            for (;;)
            {
                HttpRequest req;
                HttpResponse resp;

                parser.receiveRequest(stream, req);
                processRequest(req, resp);

                stream << resp;
                stream.flush();

                if (stream.peek() == std::iostream::traits_type::eof())
                {
                    break;
                }
            }

            logger(DEBUGGING) << "Closing connection from " << peerIp << ":" << peerPort
                              << " total=" << m_connections.size();
        }
        catch (System::InterruptedException &)
        {
        }
        catch (std::exception &e)
        {
            logger(DEBUGGING) << "Connection error: " << e.what();
        }
    }

} // namespace CryptoNote
