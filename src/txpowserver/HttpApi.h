// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "PowService.h"
#include "TxPowServerConfig.h"

#include "httplib.h"
#include "json.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

/* HTTP front of the PoW service.

     POST   /pow            {"prefix": "<hex>", "wait_ms": N, "height": H?}
     GET    /pow/<id>?wait_ms=N
     DELETE /pow/<id>
     GET    /stats
     GET    /health
     GET    /

   A job reply is {"status": "done", "nonce": "<16 hex>", ...},
   {"status": "pending", "job_id": ..., ...} or {"status": "error", "error": ...}. */
class HttpApi
{
  public:
    HttpApi(const TxPowServerConfig &config, PowService &service);

    ~HttpApi();

    /* Binds the listeners. False when the primary one could not bind; the
       IPv6 listener failing is only logged. */
    bool start();

    void stop();

  private:
    void configure(httplib::Server &server);

    void setupRoutes(httplib::Server &server);

    /* CORS, API key and per-address rate limit. False when the request was
       already answered. */
    bool admit(const httplib::Request &req, httplib::Response &res, bool rateLimitApplies);

    /* The connecting address, or the client a trusted proxy vouches for. */
    std::string clientAddress(const httplib::Request &req) const;

    bool addressRateLimited(const std::string &address);

    bool jobRateLimited();

    void reply(httplib::Response &res, int status, const nlohmann::json &body) const;

    nlohmann::json jobJson(const PowJob &job) const;

    uint32_t waitMs(const httplib::Request &req, const nlohmann::json *body) const;

    void handleSubmit(const httplib::Request &req, httplib::Response &res);

    void handlePoll(const httplib::Request &req, httplib::Response &res);

    void handleCancel(const httplib::Request &req, httplib::Response &res);

    void handleStats(const httplib::Request &req, httplib::Response &res);

    void handleHealth(const httplib::Request &req, httplib::Response &res);

    void handleRoot(const httplib::Request &req, httplib::Response &res);

    void handleOptions(const httplib::Request &req, httplib::Response &res);

    const TxPowServerConfig &m_config;

    PowService &m_service;

    std::unique_ptr<httplib::Server> m_server;

    std::unique_ptr<httplib::Server> m_ipv6Server;

    std::thread m_thread;

    std::thread m_ipv6Thread;

    std::mutex m_rateMutex;

    uint64_t m_addressWindow = 0;

    std::unordered_map<std::string, uint32_t> m_requestsByAddress;

    uint64_t m_jobWindow = 0;

    uint32_t m_jobsThisWindow = 0;
};
