// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "HttpServer.h"

#include <common/IpcSocket.h>
#include <http/HttpParser.h>
#include <memory>
#include <stdexcept>
#include <system/InterruptedException.h>
#include <system/IpAddress.h>
#include <system/TcpStream.h>

using namespace Logging;

namespace
{
    /* Runs an action when the enclosing scope exits, by any path including an
       exception. Replaces BOOST_SCOPE_EXIT_ALL. */
    template<class F> class ScopeExit
    {
      public:
        explicit ScopeExit(F action): m_action(std::move(action)) {}

        ~ScopeExit()
        {
            m_action();
        }

        ScopeExit(const ScopeExit &) = delete;

        ScopeExit &operator=(const ScopeExit &) = delete;

      private:
        F m_action;
    };
} // namespace

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

    void HttpServer::startIpc(
        const std::string &socketPath,
        const uint32_t socketMode,
        const std::string &socketGroup)
    {
        std::string error;

        if (!Common::Ipc::supported())
        {
            throw std::runtime_error(Common::Ipc::unsupportedReason());
        }

        if (!Common::Ipc::validatePath(socketPath, error) || !Common::Ipc::removeStaleSocket(socketPath, error))
        {
            throw std::runtime_error(error);
        }

        /* The listener creates the socket under a umask derived from
           socketMode, so it is never briefly world reachable; this makes the
           result exact and applies the group. */
        m_listener = System::TcpListener(m_dispatcher, socketPath, socketMode);

        if (!Common::Ipc::applyPermissions(socketPath, socketMode, socketGroup, error))
        {
            m_listener = System::TcpListener();
            Common::Ipc::cleanup(socketPath);
            throw std::runtime_error(error);
        }

        m_ipcPath = socketPath;

        workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));
    }

    void HttpServer::stop()
    {
        workingContextGroup.interrupt();
        workingContextGroup.wait();

        if (!m_ipcPath.empty())
        {
            Common::Ipc::cleanup(m_ipcPath);
            m_ipcPath.clear();
        }
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
            ScopeExit eraseConnection([this, &connection] { m_connections.erase(&connection); });

            workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

            /* A local socket peer has no address at all, and asking for one
               throws. There is nothing to name it by beyond the socket it came
               in on, which is the same for every caller here. */
            std::string peer;

            try
            {
                peer = connection.getPeerIpAddress().toString() + ":"
                       + std::to_string(connection.getPeerAddressAndPort().second);
            }
            catch (const std::exception &)
            {
                peer = m_ipcPath.empty() ? "unknown" : m_ipcPath;
            }

            logger(DEBUGGING) << "Incoming connection from " << peer;

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

            logger(DEBUGGING) << "Closing connection from " << peer << " total=" << m_connections.size();
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
