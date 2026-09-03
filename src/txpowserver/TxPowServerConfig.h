// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <logger/Logger.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct TxPowServerConfig
{
    /* Where to listen. An IPv6 literal works here too; the separate IPv6
       address below is for listening on both families at once. */
    std::string bindIp = "127.0.0.1";

    uint16_t bindPort = 17870;

    /* Second listener, same port. Empty = disabled. */
    std::string bindIpv6Address;

    /* Hashing threads. 0 = one per hardware thread. */
    unsigned int threads = 0;

    /* Addresses of reverse proxies (nginx, caddy, ...) in front of this
       server. A request from one of them is attributed, for rate limiting and
       logging, to the client named in X-Real-IP or X-Forwarded-For. Requests
       from any other address keep their own address, so the headers cannot be
       used to dodge the limit. */
    std::vector<std::string> trustedProxies;

    /* Requests per minute one client address may make. 0 = unlimited. */
    uint32_t rateLimitPerMinute = 60;

    /* Jobs the server accepts per minute across all clients. 0 = unlimited. */
    uint32_t maxJobsPerMinute = 120;

    /* Jobs waiting to be solved before new ones are refused. */
    uint32_t maxQueue = 64;

    /* Largest difficulty the server will work on. A normal transaction with
       two inputs and six outputs needs 66,000; ninety outputs would need
       about 400,000 plus the inputs. */
    uint64_t maxDifficulty = 1'000'000;

    /* Longest a client may ask a request to wait for its result. */
    uint32_t maxWaitMs = 30'000;

    /* A job nobody has collected within this long is dropped. */
    uint32_t jobTimeoutSeconds = 600;

    /* How long a finished result stays available for polling. */
    uint32_t resultTtlSeconds = 300;

    /* When set, every request must carry it in the X-API-KEY header. */
    std::string apiKey;

    /* Access-Control-Allow-Origin value. Empty = no CORS headers. */
    std::string corsHeader;

    Logger::LogLevel logLevel = Logger::INFO;

    std::optional<std::string> logFile;
};

TxPowServerConfig parseTxPowServerArguments(int argc, char **argv);
