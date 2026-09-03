// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "TxPowServerConfig.h"

#include <config/CliHeader.h>
#include <cxxopts.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

TxPowServerConfig parseTxPowServerArguments(int argc, char **argv)
{
    TxPowServerConfig config;

    cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

    bool help = false;
    bool version = false;

    std::string logLevel = "info";
    std::string logFile;

    options.add_options("Core")(
        "h,help",
        "Display this help message",
        cxxopts::value<bool>(help)->default_value("false")->implicit_value("true"))

        ("v,version",
         "Output software version information",
         cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"))

        ("log-level",
         "One of trace, debug, info, warning, fatal, disabled",
         cxxopts::value<std::string>(logLevel)->default_value(logLevel),
         "<level>")

        ("log-file",
         "Also append log lines to <file>",
         cxxopts::value<std::string>(logFile),
         "<file>");

    options.add_options("Network")(
        "bind-ip",
        "Interface to listen on. Use 0.0.0.0 or :: to accept remote wallets",
        cxxopts::value<std::string>(config.bindIp)->default_value(config.bindIp),
        "<ip>")

        ("bind-port",
         "TCP port to listen on",
         cxxopts::value<uint16_t>(config.bindPort)->default_value(std::to_string(config.bindPort)),
         "#")

        ("bind-ipv6-address",
         "Additional IPv6 address to listen on, same port. Empty disables it",
         cxxopts::value<std::string>(config.bindIpv6Address)->default_value(""),
         "<ipv6>")

        ("trusted-proxy",
         "Address of a reverse proxy in front of this server, e.g. 127.0.0.1 for a local nginx. Requests from it "
         "are attributed to the client in X-Real-IP or X-Forwarded-For. Repeat or comma-separate for several",
         cxxopts::value<std::vector<std::string>>(config.trustedProxies),
         "<ip>")

        ("enable-cors",
         "Value for the Access-Control-Allow-Origin header, for the web wallet. Use * for any origin",
         cxxopts::value<std::string>(config.corsHeader),
         "<domain>")

        ("api-key",
         "Require this value in the X-API-KEY header on every request",
         cxxopts::value<std::string>(config.apiKey),
         "<key>");

    options.add_options("Work")(
        "threads",
        "Hashing threads. 0 uses one per hardware thread",
        cxxopts::value<unsigned int>(config.threads)->default_value("0"),
        "#")

        ("rate-limit",
         "Requests per minute allowed from one client address. 0 disables the limit",
         cxxopts::value<uint32_t>(config.rateLimitPerMinute)->default_value(std::to_string(config.rateLimitPerMinute)),
         "#")

        ("max-jobs-per-minute",
         "Jobs accepted per minute across all clients. 0 disables the limit",
         cxxopts::value<uint32_t>(config.maxJobsPerMinute)->default_value(std::to_string(config.maxJobsPerMinute)),
         "#")

        ("max-queue",
         "Jobs allowed to wait for a free worker before new ones are refused",
         cxxopts::value<uint32_t>(config.maxQueue)->default_value(std::to_string(config.maxQueue)),
         "#")

        ("max-difficulty",
         "Refuse jobs whose difficulty is above this",
         cxxopts::value<uint64_t>(config.maxDifficulty)->default_value(std::to_string(config.maxDifficulty)),
         "#")

        ("max-wait-ms",
         "Longest a request may be held open waiting for its result",
         cxxopts::value<uint32_t>(config.maxWaitMs)->default_value(std::to_string(config.maxWaitMs)),
         "#")

        ("job-timeout",
         "Seconds after which an uncollected job is dropped",
         cxxopts::value<uint32_t>(config.jobTimeoutSeconds)->default_value(std::to_string(config.jobTimeoutSeconds)),
         "#")

        ("result-ttl",
         "Seconds a finished result stays available for polling",
         cxxopts::value<uint32_t>(config.resultTtlSeconds)->default_value(std::to_string(config.resultTtlSeconds)),
         "#");

    try
    {
        const auto result = options.parse(argc, argv);

        if (help)
        {
            std::cout << options.help({}) << std::endl;
            exit(0);
        }

        if (version)
        {
            std::cout << CryptoNote::getProjectCLIHeader() << std::endl;
            exit(0);
        }
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl << std::endl;
        std::cout << options.help({}) << std::endl;
        exit(1);
    }

    std::transform(logLevel.begin(), logLevel.end(), logLevel.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    static const char *const levels[] = {"trace", "debug", "info", "warning", "fatal", "disabled"};

    if (std::find(std::begin(levels), std::end(levels), logLevel) == std::end(levels))
    {
        std::cout << "--log-level must be one of trace, debug, info, warning, fatal, disabled" << std::endl;
        exit(1);
    }

    config.logLevel = Logger::stringToLogLevel(logLevel);

    if (!logFile.empty())
    {
        config.logFile = logFile;
    }

    if (config.threads == 0)
    {
        config.threads = std::max(1u, std::thread::hardware_concurrency());
    }

    if (config.maxQueue == 0)
    {
        std::cout << "--max-queue must be at least 1" << std::endl;
        exit(1);
    }

    if (config.maxDifficulty == 0)
    {
        std::cout << "--max-difficulty must be at least 1" << std::endl;
        exit(1);
    }

    if (config.bindPort == 0)
    {
        std::cout << "--bind-port must be between 1 and 65535" << std::endl;
        exit(1);
    }

    /* Access-Control-Allow-Origin is only ever "*", "null", or a scheme://host
       origin. Anything else is silently rejected by every browser, so a typo
       here does not fail here - it fails much later, as a web wallet that
       cannot reach this server for reasons nothing on this side reports.
       The specific accident worth catching: an unquoted `--enable-cors *` is
       expanded by the shell into the first filename in the working directory,
       which is how one deployment ended up advertising "1.wallet" as its
       allowed origin. */
    if (!config.corsHeader.empty() && config.corsHeader != "*" && config.corsHeader != "null")
    {
        const bool looksLikeOrigin = config.corsHeader.rfind("http://", 0) == 0
                                     || config.corsHeader.rfind("https://", 0) == 0;

        if (!looksLikeOrigin)
        {
            std::cout << "--enable-cors must be *, null, or a full origin such as "
                         "https://web-wallet.example.com - got \""
                      << config.corsHeader << "\"" << std::endl;
            std::cout << "If you meant any origin, quote it: --enable-cors '*'  "
                         "(unquoted, the shell expands * to a filename)" << std::endl;
            exit(1);
        }

        if (config.corsHeader.back() == '/')
        {
            std::cout << "--enable-cors must not have a trailing slash: an origin is "
                         "scheme://host[:port], and browsers compare it verbatim - got \""
                      << config.corsHeader << "\"" << std::endl;
            exit(1);
        }
    }

    return config;
}
