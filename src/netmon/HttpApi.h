// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "Crawler.h"
#include "NetMonConfig.h"
#include "NodeStore.h"

#include "httplib.h"

#include <memory>
#include <string>
#include <thread>

namespace NetMon
{
    /* Serves the JSON API and, when --web-root is given, the dashboard files
       themselves.

       One caution shapes the route layout: httplib checks its mount points
       BEFORE its registered handlers (Server::routing), whatever order they
       were added in. A file on disk therefore shadows a same-named route. So
       everything the API answers lives under /api/, where no filename in the
       web root can collide with it. */
    class HttpApi
    {
      public:
        HttpApi(const NetMonConfig &config, NodeStore &store, const Crawler &crawler);

        ~HttpApi();

        HttpApi(const HttpApi &) = delete;

        HttpApi &operator=(const HttpApi &) = delete;

        bool start();

        void stop();

      private:
        void configure(httplib::Server &server);

        void setupRoutes(httplib::Server &server);

        void listen();

        void listenIpv6();

        std::string clientAddress(const httplib::Request &request) const;

        void reply(httplib::Response &response, int status, const std::string &body) const;

        /* Same as reply(), plus an ETag and a Cache-Control whose lifetime is
           whatever is left of the current sweep, and a 304 when the caller
           already has that version.

           The numbers only change once per sweep - ten minutes by default -
           so without this a dashboard tab or a Discord bot re-downloads an
           identical body every poll, and a CDN in front cannot help because
           it has nothing to key on. */
        void replyCached(
            const httplib::Request &request,
            httplib::Response &response,
            const Summary &summary,
            const std::string &body) const;

        void handleStats(const httplib::Request &request, httplib::Response &response) const;

        void handleSummary(const httplib::Request &request, httplib::Response &response) const;

        void handlePeers(const httplib::Request &request, httplib::Response &response) const;

        void handlePeer(const httplib::Request &request, httplib::Response &response) const;

        void handleGeo(const httplib::Request &request, httplib::Response &response) const;

        void handleVersions(const httplib::Request &request, httplib::Response &response) const;

        void handleHealth(const httplib::Request &request, httplib::Response &response) const;

        void handleOptions(const httplib::Request &request, httplib::Response &response) const;

        const NetMonConfig &m_config;

        NodeStore &m_store;

        const Crawler &m_crawler;

        std::unique_ptr<httplib::Server> m_server;

        std::unique_ptr<httplib::Server> m_ipv6Server;

        std::thread m_thread;

        std::thread m_ipv6Thread;
    };
} // namespace NetMon
