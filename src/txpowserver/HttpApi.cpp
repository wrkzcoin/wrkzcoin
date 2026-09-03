// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "HttpApi.h"

#include <common/StringTools.h>
#include <logger/Logger.h>
#include <version.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>

namespace
{
    /* Larger than any prefix the network accepts, small enough that a flood
       of oversized bodies costs little. */
    constexpr size_t MAX_BODY_BYTES = 512 * 1024;

    constexpr const char *JOB_ID_PATTERN = R"(/pow/([0-9a-f]{32}))";

    uint64_t currentMinute()
    {
        const uint64_t now = static_cast<uint64_t>(std::time(nullptr));

        return now - (now % 60);
    }
} // namespace

HttpApi::HttpApi(const TxPowServerConfig &config, PowService &service): m_config(config), m_service(service) {}

HttpApi::~HttpApi()
{
    stop();
}

void HttpApi::configure(httplib::Server &server)
{
    /* Long polls hold a handler thread each, so size the pool for the number
       of jobs that can be waiting rather than for the CPU count. */
    const size_t baseThreads = std::max<size_t>(8, m_config.maxQueue);
    const size_t maxThreads = baseThreads * 4;

    server.new_task_queue = [baseThreads, maxThreads] { return new httplib::ThreadPool(baseThreads, maxThreads); };

    server.set_tcp_nodelay(true);
    server.set_payload_max_length(MAX_BODY_BYTES);
    server.set_read_timeout(30, 0);
    server.set_write_timeout(30, 0);
    server.set_keep_alive_max_count(64);

    server.set_exception_handler([this](const httplib::Request &, httplib::Response &res, std::exception_ptr ep) {
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

        reply(res, 500, {{"status", "error"}, {"error", what}});
    });

    setupRoutes(server);
}

void HttpApi::setupRoutes(httplib::Server &server)
{
    server.Options(".*", [this](const httplib::Request &req, httplib::Response &res) { handleOptions(req, res); });

    server.Get("/", [this](const httplib::Request &req, httplib::Response &res) { handleRoot(req, res); });

    server.Get("/health", [this](const httplib::Request &req, httplib::Response &res) { handleHealth(req, res); });

    server.Get("/stats", [this](const httplib::Request &req, httplib::Response &res) { handleStats(req, res); });

    server.Post("/pow", [this](const httplib::Request &req, httplib::Response &res) { handleSubmit(req, res); });

    server.Get(JOB_ID_PATTERN, [this](const httplib::Request &req, httplib::Response &res) { handlePoll(req, res); });

    server.Delete(
        JOB_ID_PATTERN, [this](const httplib::Request &req, httplib::Response &res) { handleCancel(req, res); });
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

    m_thread = std::thread([this] { m_server->listen_after_bind(); });

    if (!m_config.bindIpv6Address.empty())
    {
        m_ipv6Server = std::make_unique<httplib::Server>();

        configure(*m_ipv6Server);

        if (m_ipv6Server->bind_to_port(m_config.bindIpv6Address, m_config.bindPort))
        {
            m_ipv6Thread = std::thread([this] { m_ipv6Server->listen_after_bind(); });
        }
        else
        {
            Logger::logger.log(
                "Could not bind the IPv6 listener to [" + m_config.bindIpv6Address + "]:"
                    + std::to_string(m_config.bindPort) + ", continuing without it",
                Logger::WARNING,
                {Logger::DAEMON_RPC});

            m_ipv6Server.reset();
        }
    }

    return true;
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
}

void HttpApi::reply(httplib::Response &res, const int status, const nlohmann::json &body) const
{
    res.status = status;

    if (!m_config.corsHeader.empty())
    {
        res.set_header("Access-Control-Allow-Origin", m_config.corsHeader);
    }

    res.set_content(body.dump(), "application/json");
}

std::string HttpApi::clientAddress(const httplib::Request &req) const
{
    const auto &proxies = m_config.trustedProxies;

    if (std::find(proxies.begin(), proxies.end(), req.remote_addr) == proxies.end())
    {
        return req.remote_addr;
    }

    const auto trim = [](std::string s) {
        const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
        return s;
    };

    if (req.has_header("X-Real-IP"))
    {
        const std::string real = trim(req.get_header_value("X-Real-IP"));

        if (!real.empty())
        {
            return real;
        }
    }

    if (req.has_header("X-Forwarded-For"))
    {
        /* Proxies append the address they saw, so the last entry is the one
           the trusted proxy vouches for; anything before it came from the
           client and could say anything. */
        const std::string forwarded = req.get_header_value("X-Forwarded-For");

        const auto comma = forwarded.find_last_of(',');

        const std::string last = trim(comma == std::string::npos ? forwarded : forwarded.substr(comma + 1));

        if (!last.empty())
        {
            return last;
        }
    }

    return req.remote_addr;
}

bool HttpApi::addressRateLimited(const std::string &address)
{
    if (m_config.rateLimitPerMinute == 0)
    {
        return false;
    }

    const uint64_t window = currentMinute();

    std::lock_guard<std::mutex> lock(m_rateMutex);

    /* One entry per address that ever connected would grow without bound. */
    if (window != m_addressWindow)
    {
        m_requestsByAddress.clear();
        m_addressWindow = window;
    }

    uint32_t &count = m_requestsByAddress[address];

    if (count >= m_config.rateLimitPerMinute)
    {
        return true;
    }

    count++;

    return false;
}

bool HttpApi::jobRateLimited()
{
    if (m_config.maxJobsPerMinute == 0)
    {
        return false;
    }

    const uint64_t window = currentMinute();

    std::lock_guard<std::mutex> lock(m_rateMutex);

    if (window != m_jobWindow)
    {
        m_jobsThisWindow = 0;
        m_jobWindow = window;
    }

    if (m_jobsThisWindow >= m_config.maxJobsPerMinute)
    {
        return true;
    }

    m_jobsThisWindow++;

    return false;
}

bool HttpApi::admit(const httplib::Request &req, httplib::Response &res, const bool rateLimitApplies)
{
    m_service.requests++;

    if (!m_config.apiKey.empty())
    {
        if (!req.has_header("X-API-KEY") || req.get_header_value("X-API-KEY") != m_config.apiKey)
        {
            m_service.unauthorized++;

            reply(res, 401, {{"status", "error"}, {"error", "missing or incorrect X-API-KEY header"}});

            return false;
        }
    }

    if (rateLimitApplies && addressRateLimited(clientAddress(req)))
    {
        m_service.rateLimited++;

        reply(res, 429, {{"status", "error"}, {"error", "too many requests from this address, retry later"}});

        return false;
    }

    return true;
}

nlohmann::json HttpApi::jobJson(const PowJob &job) const
{
    const PowJobState state = job.state.load();

    nlohmann::json j = {
        {"job_id", job.id},
        {"state", powJobStateName(state)},
        {"difficulty", job.difficulty},
        {"inputs", job.inputs},
        {"outputs", job.outputs},
        {"hashes", job.hashes.load()},
        {"elapsed_ms", job.elapsedMs()},
    };

    switch (state)
    {
        case PowJobState::Done:
            j["status"] = "done";
            j["nonce"] = Common::toHex(job.nonce.data(), job.nonce.size());
            break;
        case PowJobState::Failed:
            j["status"] = "error";
            j["error"] = job.error;
            break;
        case PowJobState::Cancelled:
            j["status"] = "cancelled";
            break;
        case PowJobState::Queued:
        case PowJobState::Running:
            j["status"] = "pending";
            break;
    }

    if (state == PowJobState::Queued)
    {
        j["queue_length"] = m_service.queued();
    }

    return j;
}

uint32_t HttpApi::waitMs(const httplib::Request &req, const nlohmann::json *body) const
{
    uint64_t wait = 0;

    if (body != nullptr && body->contains("wait_ms") && (*body)["wait_ms"].is_number_unsigned())
    {
        wait = (*body)["wait_ms"].get<uint64_t>();
    }
    else if (req.has_param("wait_ms"))
    {
        try
        {
            wait = std::stoull(req.get_param_value("wait_ms"));
        }
        catch (const std::exception &)
        {
            wait = 0;
        }
    }

    return static_cast<uint32_t>(std::min<uint64_t>(wait, m_config.maxWaitMs));
}

void HttpApi::handleSubmit(const httplib::Request &req, httplib::Response &res)
{
    if (!admit(req, res, true))
    {
        return;
    }

    nlohmann::json body;

    try
    {
        body = nlohmann::json::parse(req.body);
    }
    catch (const std::exception &)
    {
        reply(res, 400, {{"status", "error"}, {"error", "body is not valid JSON"}});
        return;
    }

    if (!body.is_object() || !body.contains("prefix") || !body["prefix"].is_string())
    {
        reply(res, 400, {{"status", "error"}, {"error", "body must be an object with a hex 'prefix' field"}});
        return;
    }

    std::vector<uint8_t> prefix;

    if (!Common::fromHex(body["prefix"].get<std::string>(), prefix))
    {
        reply(res, 400, {{"status", "error"}, {"error", "'prefix' is not valid hex"}});
        return;
    }

    std::optional<uint64_t> height;

    if (body.contains("height") && body["height"].is_number_unsigned())
    {
        height = body["height"].get<uint64_t>();
    }

    /* The jobs-per-minute budget is spent only on prefixes that passed
       validation, so a flood of garbage cannot starve real wallets of it. */
    const auto submitted = m_service.submit(prefix, height, [this] {
        if (jobRateLimited())
        {
            m_service.globalLimited++;
            return false;
        }

        return true;
    });

    if (!submitted.job)
    {
        reply(res, submitted.httpStatus, {{"status", "error"}, {"error", submitted.error}});
        return;
    }

    Logger::logger.log(
        "Job " + submitted.job->id + " from " + clientAddress(req), Logger::DEBUG, {Logger::DAEMON_RPC});

    const uint32_t wait = waitMs(req, &body);

    if (wait > 0)
    {
        submitted.job->waitFor(std::chrono::milliseconds(wait));
    }

    reply(res, 200, jobJson(*submitted.job));
}

void HttpApi::handlePoll(const httplib::Request &req, httplib::Response &res)
{
    if (!admit(req, res, true))
    {
        return;
    }

    const auto job = m_service.find(req.matches[1]);

    if (!job)
    {
        reply(res, 404, {{"status", "error"}, {"error", "unknown or expired job"}});
        return;
    }

    const uint32_t wait = waitMs(req, nullptr);

    if (wait > 0 && !job->finished())
    {
        job->waitFor(std::chrono::milliseconds(wait));
    }

    reply(res, 200, jobJson(*job));
}

void HttpApi::handleCancel(const httplib::Request &req, httplib::Response &res)
{
    if (!admit(req, res, true))
    {
        return;
    }

    const std::string id = req.matches[1];

    if (!m_service.cancel(id))
    {
        const auto job = m_service.find(id);

        if (!job)
        {
            reply(res, 404, {{"status", "error"}, {"error", "unknown or expired job"}});
        }
        else
        {
            reply(res, 200, jobJson(*job));
        }

        return;
    }

    reply(res, 200, {{"status", "cancelled"}, {"job_id", id}});
}

void HttpApi::handleStats(const httplib::Request &req, httplib::Response &res)
{
    if (!admit(req, res, false))
    {
        return;
    }

    /* Counters only. The bind addresses, proxy list, limits and CORS origin
       describe the deployment, not the service, and /stats is meant to be
       readable by anyone the wallets are; operators have them in the
       start-up banner and their own command line. The two limits a client
       can act on, max_difficulty and max_wait_ms, are the exception. */
    nlohmann::json stats = m_service.statsJson();

    stats["version"] = PROJECT_VERSION_LONG;

    stats["limits"] = {
        {"max_difficulty", m_config.maxDifficulty},
        {"max_wait_ms", m_config.maxWaitMs},
    };

    reply(res, 200, stats);
}

void HttpApi::handleHealth(const httplib::Request &req, httplib::Response &res)
{
    /* Deliberately outside the API key and rate limit so load balancers and
       monitors can always reach it. */
    m_service.requests++;

    const size_t waiting = m_service.queued();

    reply(
        res,
        200,
        {
            {"status", "OK"},
            {"queue", waiting},
            {"capacity", m_service.limits().maxQueue},
            {"threads", m_service.limits().threads},
        });

    (void)req;
}

void HttpApi::handleRoot(const httplib::Request &req, httplib::Response &res)
{
    if (!admit(req, res, false))
    {
        return;
    }

    reply(
        res,
        200,
        {
            {"name", "wrkz-txpow-server"},
            {"version", PROJECT_VERSION_LONG},
            {"endpoints",
             {"POST /pow", "GET /pow/<job_id>", "DELETE /pow/<job_id>", "GET /stats", "GET /health"}},
        });
}

void HttpApi::handleOptions(const httplib::Request &req, httplib::Response &res)
{
    if (!m_config.corsHeader.empty())
    {
        res.set_header("Access-Control-Allow-Origin", m_config.corsHeader);
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, X-API-KEY");
        res.set_header("Access-Control-Max-Age", "600");
    }

    res.status = 204;

    (void)req;
}
