// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "HttpApi.h"

#include "json.hpp"

#include <algorithm>
#include <map>

#include <common/StringTools.h>
#include <config/CryptoNoteConfig.h>
#include <logger/Logger.h>
#include <version.h>

namespace NetMon
{
    namespace
    {
        constexpr size_t MAX_BODY_BYTES = 64 * 1024;

        constexpr uint32_t DEFAULT_PAGE_SIZE = 100;

        constexpr uint32_t MAX_PAGE_SIZE = 1000;

        /* Mirrors NodeStore's own cap on the retained series. */
        constexpr uint32_t MAX_SWEEP_POINTS = 1024;

        /* The published shape of /api/stats. Bump it when a field changes
           meaning or goes away, so a consumer keyed on this can tell. */
        constexpr int STATS_SCHEMA = 1;

        nlohmann::json bucketsToJson(const std::vector<Bucket> &buckets)
        {
            nlohmann::json out = nlohmann::json::array();

            for (const Bucket &bucket : buckets)
            {
                out.push_back({{"label", bucket.label}, {"count", bucket.count}});
            }

            return out;
        }

        nlohmann::json recordToJson(const NodeRecord &record, const bool includeHistory)
        {
            nlohmann::json j;

            j["key"] = record.key();
            j["address"] = record.address;
            j["port"] = record.port;
            j["isSeed"] = record.isSeed;

            /* Measured here. */
            j["reach"] = reachToString(record.reach);
            j["rttMs"] = record.rttMs;
            j["firstSeen"] = record.firstSeen;
            j["lastSeen"] = record.lastSeen;
            j["lastTried"] = record.lastTried;
            j["sweepsOffered"] = record.sweepsOffered;
            j["sweepsAnswered"] = record.sweepsAnswered;
            j["peersAdvertised"] = record.peersAdvertised;

            /* Claimed by the node. Grouped under its own key so a consumer
               cannot mistake one for the other. */
            nlohmann::json reported;
            reported["haveHandshake"] = record.haveHandshake;
            reported["p2pVersion"] = record.p2pVersion;
            reported["peerId"] = record.peerId;
            reported["height"] = record.height;
            reported["topId"] = record.haveHandshake ? Common::podToHex(record.topId) : std::string();
            reported["capabilityFlags"] = record.capabilityFlags;
            reported["pruned"] = record.isPruned();
            reported["prunedHeight"] = record.prunedHeight;
            reported["lite"] = record.isLite();
            reported["liteStartHeight"] = record.liteStartHeight;
            reported["clockSkewSeconds"] = record.clockSkewSeconds;
            reported["advertisedPort"] = record.advertisedPort;
            reported["softwareVersion"] = record.softwareVersion;
            j["reported"] = reported;

            /* Derived from a database, not observed and not claimed. */
            nlohmann::json location;
            location["country"] = record.country;
            location["asn"] = record.asn;
            location["asName"] = record.asName;
            j["location"] = location;

            if (includeHistory)
            {
                nlohmann::json history = nlohmann::json::array();

                for (const auto &day : record.history)
                {
                    history.push_back(
                        {{"day", day.first}, {"offered", day.second.offered}, {"answered", day.second.answered}});
                }

                j["history"] = history;
            }

            return j;
        }

        uint32_t queryUint(const httplib::Request &request, const std::string &name, const uint32_t fallback)
        {
            if (!request.has_param(name))
            {
                return fallback;
            }

            try
            {
                return static_cast<uint32_t>(std::stoul(request.get_param_value(name)));
            }
            catch (...)
            {
                return fallback;
            }
        }
    } // namespace

    HttpApi::HttpApi(const NetMonConfig &config, NodeStore &store, const Crawler &crawler):
        m_config(config),
        m_store(store),
        m_crawler(crawler)
    {
    }

    HttpApi::~HttpApi()
    {
        stop();
    }

    void HttpApi::configure(httplib::Server &server)
    {
        server.set_tcp_nodelay(true);
        server.set_payload_max_length(MAX_BODY_BYTES);
        server.set_read_timeout(15, 0);
        server.set_write_timeout(30, 0);
        server.set_keep_alive_max_count(64);

        server.set_exception_handler(
            [this](const httplib::Request &, httplib::Response &response, std::exception_ptr ep) {
                std::string what = "unknown error";

                try
                {
                    if (ep)
                    {
                        std::rethrow_exception(ep);
                    }
                }
                catch (const std::exception &e)
                {
                    what = e.what();
                }
                catch (...)
                {
                }

                nlohmann::json j;
                j["status"] = "error";
                j["error"] = what;

                reply(response, 500, j.dump());
            });

        /* The only use for --trusted-proxy: naming the real client in the log
           when nginx is in front. It is deliberately not wired to anything
           that grants access, so a spoofed header buys nothing. */
        server.set_logger([this](const httplib::Request &request, const httplib::Response &response) {
            if (!Logger::logger.shouldLog(Logger::DEBUG))
            {
                return;
            }

            Logger::logger.log(
                "[" + clientAddress(request) + "] " + request.method + " " + request.path + " -> "
                    + std::to_string(response.status),
                Logger::DEBUG,
                {Logger::DAEMON_RPC});
        });

        setupRoutes(server);

        /* Mounted after the routes for readability only - httplib consults
           mount points first regardless, which is why every API path is under
           /api/. See the note in HttpApi.h. */
        if (!m_config.webRoot.empty())
        {
            if (!server.set_mount_point("/", m_config.webRoot))
            {
                Logger::logger.log(
                    "Could not serve --web-root " + m_config.webRoot + "; the API is still up",
                    Logger::WARNING,
                    {Logger::DAEMON_RPC});
            }

            /* The dashboard assets are not content-hashed - they carry a ?v=
               stamp the way the block explorer's do - so let the browser
               revalidate and let the ETag httplib already sets make that a
               304 rather than a fresh download. */
            server.set_file_request_handler([](const httplib::Request &, httplib::Response &response) {
                response.set_header("Cache-Control", "no-cache");
            });
        }
    }

    void HttpApi::setupRoutes(httplib::Server &server)
    {
        server.Options(".*", [this](const httplib::Request &request, httplib::Response &response) {
            handleOptions(request, response);
        });

        /* Aggregates only, and deliberately nothing else: this is the route
           meant to be published, embedded in a page or polled by a chat bot,
           so it must stay safe to expose whatever the proxy in front is
           configured to do. Adding a per-node field here defeats the point. */
        server.Get("/api/stats", [this](const httplib::Request &request, httplib::Response &response) {
            handleStats(request, response);
        });

        server.Get("/api/summary", [this](const httplib::Request &request, httplib::Response &response) {
            handleSummary(request, response);
        });

        server.Get("/api/peers", [this](const httplib::Request &request, httplib::Response &response) {
            handlePeers(request, response);
        });

        /* The key is "address:port", and an IPv6 address arrives bracketed, so
           the pattern has to admit colons, dots, hex, and both brackets. */
        server.Get(
            R"(/api/peers/([0-9a-fA-F:%\.\[\]]+))",
            [this](const httplib::Request &request, httplib::Response &response) { handlePeer(request, response); });

        server.Get("/api/geo", [this](const httplib::Request &request, httplib::Response &response) {
            handleGeo(request, response);
        });

        server.Get("/api/versions", [this](const httplib::Request &request, httplib::Response &response) {
            handleVersions(request, response);
        });

        server.Get("/api/health", [this](const httplib::Request &request, httplib::Response &response) {
            handleHealth(request, response);
        });

        /* Kept outside /api/ as well, because a health check is the one thing
           an operator's tooling will look for at a conventional path. A file
           named "health" in the web root would shadow it; nothing in
           extras/netmon is called that. */
        server.Get("/health", [this](const httplib::Request &request, httplib::Response &response) {
            handleHealth(request, response);
        });
    }

    bool HttpApi::start()
    {
        m_server = std::make_unique<httplib::Server>();

        configure(*m_server);

        if (!m_server->bind_to_port(m_config.bindIp, m_config.bindPort))
        {
            Logger::logger.log(
                "Could not bind to " + m_config.bindIp + ":" + std::to_string(m_config.bindPort)
                    + ". Is the port in use, or the address not local?",
                Logger::FATAL,
                {Logger::DAEMON_RPC});

            return false;
        }

        m_thread = std::thread(&HttpApi::listen, this);

        if (!m_config.bindIpv6Address.empty())
        {
            m_ipv6Server = std::make_unique<httplib::Server>();

            configure(*m_ipv6Server);

            if (!m_ipv6Server->bind_to_port(m_config.bindIpv6Address, m_config.bindPort))
            {
                /* Same policy as the daemon's second listener: a failed extra
                   listener warns and the service keeps serving on the one that
                   came up. */
                Logger::logger.log(
                    "Could not bind to [" + m_config.bindIpv6Address + "]:" + std::to_string(m_config.bindPort),
                    Logger::WARNING,
                    {Logger::DAEMON_RPC});

                m_ipv6Server.reset();
            }
            else
            {
                m_ipv6Thread = std::thread(&HttpApi::listenIpv6, this);
            }
        }

        return true;
    }

    void HttpApi::listen()
    {
        m_server->listen_after_bind();
    }

    void HttpApi::listenIpv6()
    {
        m_ipv6Server->listen_after_bind();
    }

    void HttpApi::stop()
    {
        if (m_server)
        {
            m_server->stop();
        }

        if (m_ipv6Server)
        {
            m_ipv6Server->stop();
        }

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        if (m_ipv6Thread.joinable())
        {
            m_ipv6Thread.join();
        }

        m_server.reset();
        m_ipv6Server.reset();
    }

    std::string HttpApi::clientAddress(const httplib::Request &request) const
    {
        const std::string remote = request.remote_addr;

        const bool trusted =
            std::find(m_config.trustedProxies.begin(), m_config.trustedProxies.end(), remote)
            != m_config.trustedProxies.end();

        if (!trusted)
        {
            return remote;
        }

        const std::string realIp = request.get_header_value("X-Real-IP");

        if (!realIp.empty())
        {
            return realIp;
        }

        const std::string forwarded = request.get_header_value("X-Forwarded-For");

        if (forwarded.empty())
        {
            return remote;
        }

        const size_t comma = forwarded.find(',');

        return comma == std::string::npos ? forwarded : forwarded.substr(0, comma);
    }

    void HttpApi::reply(httplib::Response &response, const int status, const std::string &body) const
    {
        if (!m_config.corsHeader.empty())
        {
            response.set_header("Access-Control-Allow-Origin", m_config.corsHeader);
        }

        response.status = status;
        response.set_content(body, "application/json");
    }

    void HttpApi::replyCached(
        const httplib::Request &request,
        httplib::Response &response,
        const Summary &summary,
        const std::string &body) const
    {
        /* Weak, and keyed on the sweep plus the body length. ETags are
           per-URL by definition, so two pages of /api/peers cannot collide
           on the sweep number alone. */
        const std::string etag = "W/\"" + std::to_string(summary.sweepNumber) + "-"
                                 + std::to_string(body.size()) + "\"";

        /* Cache for however long this answer stays true. A sweep that runs
           late only means the cached copy is still the current one. */
        uint64_t maxAge = m_config.sweepIntervalSeconds;

        if (summary.lastSweepFinished != 0)
        {
            const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
            const uint64_t elapsed = now > summary.lastSweepFinished ? now - summary.lastSweepFinished : 0;

            maxAge = elapsed >= m_config.sweepIntervalSeconds ? 15
                                                              : m_config.sweepIntervalSeconds - elapsed;
        }

        if (maxAge < 15)
        {
            maxAge = 15;
        }

        if (!m_config.corsHeader.empty())
        {
            response.set_header("Access-Control-Allow-Origin", m_config.corsHeader);
        }

        response.set_header("ETag", etag);
        response.set_header("Cache-Control", "public, max-age=" + std::to_string(maxAge));

        if (request.get_header_value("If-None-Match") == etag)
        {
            response.status = 304;
            return;
        }

        response.status = 200;
        response.set_content(body, "application/json");
    }

    void HttpApi::handleOptions(const httplib::Request &, httplib::Response &response) const
    {
        if (!m_config.corsHeader.empty())
        {
            response.set_header("Access-Control-Allow-Origin", m_config.corsHeader);
            response.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
            response.set_header("Access-Control-Allow-Headers", "Content-Type");
        }

        response.status = 200;
    }

    void HttpApi::handleHealth(const httplib::Request &, httplib::Response &response) const
    {
        const Summary summary = m_store.summarise();

        nlohmann::json j;
        j["status"] = "OK";
        j["version"] = PROJECT_VERSION_LONG;
        j["sweepNumber"] = summary.sweepNumber;
        j["lastSweepFinished"] = summary.lastSweepFinished;
        j["known"] = summary.known;
        j["reachable"] = summary.reachable;

        reply(response, 200, j.dump());
    }

    void HttpApi::handleSummary(const httplib::Request &request, httplib::Response &response) const
    {
        const Summary summary = m_store.summarise();

        nlohmann::json j;

        j["status"] = "OK";
        j["version"] = PROJECT_VERSION_LONG;

        j["known"] = summary.known;
        j["reachable"] = summary.reachable;
        j["grey"] = summary.grey;
        j["networkHeight"] = summary.networkHeight;

        j["transport"] = {{"ipv4", summary.ipv4}, {"ipv6", summary.ipv6}};

        j["capability"] = {{"full", summary.full}, {"pruned", summary.pruned}, {"lite", summary.lite}};

        j["rtt"] = {{"medianMs", summary.rttMedianMs}, {"p95Ms", summary.rttP95Ms}};

        j["clockSkew"] = {
            {"within5s", summary.clockWithin5s},
            {"under60s", summary.clockUnder60s},
            {"over60s", summary.clockOver60s}};

        j["churn24h"] = {{"joined", summary.joined24h}, {"left", summary.left24h}};

        j["heightBuckets"] = bucketsToJson(summary.heightBuckets);
        j["topHashes"] = bucketsToJson(summary.topHashes);
        j["versions"] = bucketsToJson(summary.versions);

        j["softwareKnown"] = summary.softwareKnown;

        j["sweep"] = {
            {"number", summary.sweepNumber},
            {"startedAt", summary.lastSweepStarted},
            {"finishedAt", summary.lastSweepFinished},
            {"probed", summary.lastSweepProbed},
            {"durationMs", summary.lastSweepDurationMs},
            {"intervalSeconds", m_config.sweepIntervalSeconds}};

        j["seeds"] = m_crawler.seeds();

        /* What the dashboard needs to caption its own panels honestly. */
        j["capabilities"] = {
            {"geoip", nlohmann::json(!summary.countries.empty())},
            {"asn", nlohmann::json(!summary.networks.empty())},
            {"rpcProbe", nlohmann::json(m_config.probeRpc)}};

        /* The series is capped at a thousand points, which at a ten minute
           sweep is a week - about 56 KB of JSON riding along on every poll of
           a response that is otherwise a kilobyte. Callers say how much of the
           tail they actually draw. */
        const uint32_t points = std::min<uint32_t>(MAX_SWEEP_POINTS, queryUint(request, "points", 288));

        const std::vector<SweepPoint> history = m_store.sweepHistory();

        nlohmann::json sweeps = nlohmann::json::array();

        for (size_t i = history.size() > points ? history.size() - points : 0; i < history.size(); i++)
        {
            sweeps.push_back(
                {{"finishedAt", history[i].finishedAt},
                 {"probed", history[i].probed},
                 {"reachable", history[i].reachable}});
        }

        j["sweepHistory"] = sweeps;

        replyCached(request, response, summary, j.dump());
    }

    void HttpApi::handlePeers(const httplib::Request &request, httplib::Response &response) const
    {
        std::vector<NodeRecord> nodes = m_store.snapshot();

        const std::string filter =
            request.has_param("filter") ? request.get_param_value("filter") : std::string("all");

        const std::string search = request.has_param("q") ? request.get_param_value("q") : std::string();

        const Summary summary = m_store.summarise();

        const std::string majorityHash = summary.topHashes.empty() ? std::string() : summary.topHashes[0].label;

        nodes.erase(
            std::remove_if(
                nodes.begin(),
                nodes.end(),
                [&](const NodeRecord &record) {
                    if (!search.empty() && record.key().find(search) == std::string::npos
                        && record.country.find(search) == std::string::npos
                        && record.asn.find(search) == std::string::npos)
                    {
                        return true;
                    }

                    if (filter == "all")
                    {
                        return false;
                    }

                    if (filter == "reachable")
                    {
                        return record.reach != Reach::Open;
                    }

                    if (filter == "grey")
                    {
                        return record.reach == Reach::Open;
                    }

                    if (filter == "full")
                    {
                        return record.reach != Reach::Open || record.isPruned() || record.isLite();
                    }

                    if (filter == "pruned")
                    {
                        return record.reach != Reach::Open || !record.isPruned();
                    }

                    if (filter == "lite")
                    {
                        return record.reach != Reach::Open || !record.isLite();
                    }

                    if (filter == "ipv6")
                    {
                        return record.address.rfind('[', 0) != 0;
                    }

                    if (filter == "behind")
                    {
                        if (record.reach != Reach::Open || !record.haveHandshake || majorityHash.empty())
                        {
                            return true;
                        }

                        return Common::podToHex(record.topId) == majorityHash;
                    }

                    return false;
                }),
            nodes.end());

        /* Last seen first, which puts the live network at the top and the
           long-dead addresses at the bottom. */
        std::sort(nodes.begin(), nodes.end(), [](const NodeRecord &a, const NodeRecord &b) {
            if (a.lastSeen != b.lastSeen)
            {
                return a.lastSeen > b.lastSeen;
            }

            if (a.lastTried != b.lastTried)
            {
                return a.lastTried > b.lastTried;
            }

            return a.address < b.address;
        });

        const uint32_t total = static_cast<uint32_t>(nodes.size());
        const uint32_t limit = std::min(MAX_PAGE_SIZE, queryUint(request, "limit", DEFAULT_PAGE_SIZE));
        const uint32_t offset = queryUint(request, "offset", 0);

        nlohmann::json items = nlohmann::json::array();

        for (uint32_t i = offset; i < total && i < offset + limit; i++)
        {
            items.push_back(recordToJson(nodes[i], false));
        }

        nlohmann::json j;
        j["status"] = "OK";
        j["total"] = total;
        j["offset"] = offset;
        j["limit"] = limit;
        j["majorityHash"] = majorityHash;
        j["peers"] = items;

        reply(response, 200, j.dump());
    }

    void HttpApi::handlePeer(const httplib::Request &request, httplib::Response &response) const
    {
        if (request.matches.size() < 2)
        {
            reply(response, 400, nlohmann::json({{"status", "error"}, {"error", "no peer key"}}).dump());
            return;
        }

        const std::string key = request.matches[1].str();

        NodeRecord record;

        if (!m_store.find(key, record))
        {
            reply(response, 404, nlohmann::json({{"status", "error"}, {"error", "unknown peer"}}).dump());
            return;
        }

        const Summary summary = m_store.summarise();

        nlohmann::json j = recordToJson(record, true);

        j["status"] = "OK";
        j["networkHeight"] = summary.networkHeight;
        j["majorityHash"] = summary.topHashes.empty() ? std::string() : summary.topHashes[0].label;
        j["majorityHashNodes"] = summary.topHashes.empty() ? uint64_t {0} : summary.topHashes[0].count;

        reply(response, 200, j.dump());
    }

    void HttpApi::handleStats(const httplib::Request &request, httplib::Response &response) const
    {
        const Summary s = m_store.summarise();

        const auto share = [](const uint64_t part, const uint64_t whole) {
            if (whole == 0)
            {
                return 0.0;
            }

            /* One decimal place. A consumer printing this into a chat message
               should not have to decide how to round it. */
            const double raw = static_cast<double>(part) * 100.0 / static_cast<double>(whole);

            return static_cast<double>(static_cast<int64_t>(raw * 10.0 + 0.5)) / 10.0;
        };

        const auto topList = [](const std::vector<Bucket> &buckets, const size_t limit, const char *key) {
            nlohmann::json out = nlohmann::json::array();

            for (size_t i = 0; i < buckets.size() && i < limit; i++)
            {
                out.push_back({{key, buckets[i].label}, {"n", buckets[i].count}});
            }

            return out;
        };

        const auto othersOf = [](const std::vector<Bucket> &buckets, const size_t limit) {
            uint64_t total = 0;

            for (size_t i = limit; i < buckets.size(); i++)
            {
                total += buckets[i].count;
            }

            return total;
        };

        nlohmann::json j;

        j["schema"] = STATS_SCHEMA;
        j["status"] = "OK";
        j["monitorVersion"] = PROJECT_VERSION_LONG;

        /* Unix seconds throughout: Discord renders <t:1788795591:R> as live
           relative time on its own, so converting to a string here would only
           take that away. */
        j["generatedAt"] = static_cast<uint64_t>(std::time(nullptr));

        j["sweep"] = {
            {"number", s.sweepNumber},
            {"finishedAt", s.lastSweepFinished},
            {"probed", s.lastSweepProbed},
            {"durationMs", s.lastSweepDurationMs},
            {"intervalSeconds", m_config.sweepIntervalSeconds}};

        j["nodes"] = {
            {"known", s.known},
            {"reachable", s.reachable},
            {"grey", s.grey},
            {"reachablePct", share(s.reachable, s.known)}};

        j["chain"] = {
            {"height", s.networkHeight},
            {"atTip", s.atTip},
            {"atTipPct", share(s.atTip, s.reachable)},
            {"distinctTips", s.distinctTips},
            /* Zero tips means nothing has handshaked yet, which is not
               agreement - so this is deliberately not distinctTips <= 1. */
            {"agreed", s.distinctTips == 1},
            {"buckets", topList(s.heightBuckets, s.heightBuckets.size(), "label")}};

        j["capability"] = {
            {"full", s.full},
            {"pruned",
             {{"count", s.pruned},
              {"medianDepth", s.prunedMedianDepth},
              {"pct", share(s.pruned, s.reachable)}}},
            {"lite",
             {{"count", s.lite},
              {"medianFloor", s.liteMedianFloor},
              {"pct", share(s.lite, s.reachable)}}}};

        j["transport"] = {
            {"ipv4", s.ipv4},
            {"ipv6", s.ipv6},
            {"ipv6Pct", share(s.ipv6, s.reachable)}};

        nlohmann::json versions = nlohmann::json::array();

        for (const Bucket &bucket : s.versions)
        {
            uint32_t value = 0;

            try
            {
                value = static_cast<uint32_t>(std::stoul(bucket.label.substr(1)));
            }
            catch (...)
            {
                continue;
            }

            versions.push_back({{"v", value}, {"n", bucket.count}, {"pct", share(bucket.count, s.reachable)}});
        }

        /* Newest protocol first, which is the order a version table reads in. */
        std::sort(versions.begin(), versions.end(), [](const nlohmann::json &a, const nlohmann::json &b) {
            return a["v"].get<uint32_t>() > b["v"].get<uint32_t>();
        });

        j["versions"] = versions;

        j["protocol"] = {
            {"current", CryptoNote::P2P_CURRENT_VERSION},
            {"floor", CryptoNote::P2P_MINIMUM_VERSION}};

        /* What raising the handshake floor would cost today. The reason this
           endpoint is worth publishing to the people who would be cut off. */
        nlohmann::json floors = nlohmann::json::array();

        for (uint32_t candidate = CryptoNote::P2P_MINIMUM_VERSION + 1u;
             candidate <= CryptoNote::P2P_CURRENT_VERSION;
             candidate++)
        {
            uint64_t cutOff = 0;

            for (const Bucket &bucket : s.versions)
            {
                try
                {
                    if (static_cast<uint32_t>(std::stoul(bucket.label.substr(1))) < candidate)
                    {
                        cutOff += bucket.count;
                    }
                }
                catch (...)
                {
                    continue;
                }
            }

            floors.push_back({{"to", candidate}, {"cutOff", cutOff}, {"pct", share(cutOff, s.reachable)}});
        }

        j["raiseFloor"] = floors;

        j["geo"] = {
            {"countries", s.countries.size()},
            {"located", s.located},
            {"unlocated", s.reachable > s.located ? s.reachable - s.located : 0},
            {"top", topList(s.countries, 10, "cc")},
            {"othersN", othersOf(s.countries, 10)},
            {"haveDb", !m_config.geoipDb.empty()}};

        j["network"] = {
            {"asns", s.networks.size()},
            {"top3Pct", s.top3NetworkShare},
            {"top", topList(s.networks, 10, "asn")},
            {"othersN", othersOf(s.networks, 10)},
            {"haveDb", !m_config.asnDb.empty()}};

        j["uptime"] = {
            {"over99", s.uptimeOver99},
            {"over90", s.uptimeOver90},
            {"under50", s.uptimeUnder50}};

        uint64_t oldestDays = 0;

        if (s.oldestFirstSeen != 0)
        {
            const uint64_t now = static_cast<uint64_t>(std::time(nullptr));
            oldestDays = now > s.oldestFirstSeen ? (now - s.oldestFirstSeen) / 86400 : 0;
        }

        j["health"] = {
            {"rttMedianMs", s.rttMedianMs},
            {"rttP95Ms", s.rttP95Ms},
            {"clockWithin5s", s.clockWithin5s},
            {"clockUnder60s", s.clockUnder60s},
            {"clockOver60s", s.clockOver60s},
            {"oldestNodeFirstSeen", s.oldestFirstSeen},
            {"oldestNodeAgeDays", oldestDays}};

        j["churn24h"] = {{"joined", s.joined24h}, {"left", s.left24h}};

        j["software"] = {
            {"known", s.softwareKnown},
            {"probeEnabled", m_config.probeRpc},
            {"pct", share(s.softwareKnown, s.reachable)}};

        replyCached(request, response, s, j.dump());
    }

    void HttpApi::handleGeo(const httplib::Request &, httplib::Response &response) const
    {
        const Summary summary = m_store.summarise();

        uint64_t located = 0;

        for (const Bucket &bucket : summary.countries)
        {
            located += bucket.count;
        }

        nlohmann::json j;
        j["status"] = "OK";
        j["reachable"] = summary.reachable;
        j["located"] = located;
        j["unlocated"] = summary.reachable > located ? summary.reachable - located : 0;
        j["countries"] = bucketsToJson(summary.countries);
        j["networks"] = bucketsToJson(summary.networks);

        /* Whether a database was loaded at all, so the page can say "no
           database" instead of drawing an empty map. */
        j["haveCountryDb"] = !m_config.geoipDb.empty();
        j["haveAsnDb"] = !m_config.asnDb.empty();

        reply(response, 200, j.dump());
    }

    void HttpApi::handleVersions(const httplib::Request &, httplib::Response &response) const
    {
        const Summary summary = m_store.summarise();

        nlohmann::json j;
        j["status"] = "OK";
        j["reachable"] = summary.reachable;
        j["versions"] = bucketsToJson(summary.versions);

        j["p2pCurrentVersion"] = CryptoNote::P2P_CURRENT_VERSION;
        j["p2pMinimumVersion"] = CryptoNote::P2P_MINIMUM_VERSION;

        /* What raising P2P_MINIMUM_VERSION would cost today. This is the
           question the config's own comment says is blocked on the network
           turning over, and it is the reason this endpoint exists. */
        nlohmann::json floors = nlohmann::json::array();

        for (uint32_t candidate = CryptoNote::P2P_MINIMUM_VERSION + 1u;
             candidate <= CryptoNote::P2P_CURRENT_VERSION;
             candidate++)
        {
            uint64_t cutOff = 0;

            for (const Bucket &bucket : summary.versions)
            {
                /* Labels are "v16", "v17", ... */
                if (bucket.label.size() < 2)
                {
                    continue;
                }

                uint32_t value = 0;

                try
                {
                    value = static_cast<uint32_t>(std::stoul(bucket.label.substr(1)));
                }
                catch (...)
                {
                    continue;
                }

                if (value < candidate)
                {
                    cutOff += bucket.count;
                }
            }

            floors.push_back(
                {{"version", candidate},
                 {"cutOff", cutOff},
                 {"share", summary.reachable == 0 ? 0.0 : static_cast<double>(cutOff) / summary.reachable}});
        }

        j["raiseFloor"] = floors;

        j["softwareKnown"] = summary.softwareKnown;
        j["softwareProbeEnabled"] = m_config.probeRpc;

        /* Software version histogram, over the minority of nodes that publish
           one. Built here rather than in the store: it is the only view that
           wants it. */
        std::map<std::string, uint64_t> builds;

        for (const NodeRecord &record : m_store.snapshot())
        {
            if (record.reach == Reach::Open && !record.softwareVersion.empty())
            {
                builds[record.softwareVersion]++;
            }
        }

        nlohmann::json buildsJson = nlohmann::json::array();

        for (const auto &entry : builds)
        {
            buildsJson.push_back({{"label", entry.first}, {"count", entry.second}});
        }

        j["builds"] = buildsJson;

        reply(response, 200, j.dump());
    }
} // namespace NetMon
