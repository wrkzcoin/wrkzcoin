// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "NetMonConfig.h"

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cxxopts.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

NetMonConfig parseNetMonArguments(int argc, char **argv)
{
    NetMonConfig config;

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
        "Interface the dashboard and API listen on",
        cxxopts::value<std::string>(config.bindIp)->default_value(config.bindIp),
        "<ip>")

        ("bind-port",
         "TCP port the dashboard and API listen on",
         cxxopts::value<uint16_t>(config.bindPort)->default_value(std::to_string(config.bindPort)),
         "#")

        ("bind-ipv6-address",
         "Additional IPv6 address to listen on, same port. Empty disables it",
         cxxopts::value<std::string>(config.bindIpv6Address)->default_value(""),
         "<ipv6>")

        ("web-root",
         "Directory of static dashboard files to serve at / (extras/netmon). "
         "Omit to serve the JSON API only",
         cxxopts::value<std::string>(config.webRoot)->default_value(""),
         "<dir>")

        ("enable-cors",
         "Value for Access-Control-Allow-Origin. Quote it: --enable-cors '*'",
         cxxopts::value<std::string>(config.corsHeader)->default_value(""),
         "<origin>")

        ("trusted-proxy",
         "Address of a reverse proxy whose X-Real-IP may be believed. Repeatable",
         cxxopts::value<std::vector<std::string>>(config.trustedProxies),
         "<ip>");

    options.add_options("Crawler")(
        "sweep-interval",
        "Seconds between sweeps of the whole known address set (minimum 60)",
        cxxopts::value<uint32_t>(config.sweepIntervalSeconds)
            ->default_value(std::to_string(config.sweepIntervalSeconds)),
        "#")

        ("concurrency",
         "Probes in flight at once",
         cxxopts::value<uint32_t>(config.concurrency)->default_value(std::to_string(config.concurrency)),
         "#")

        ("probe-timeout",
         "Milliseconds allowed for one connect + handshake",
         cxxopts::value<uint32_t>(config.probeTimeoutMs)->default_value(std::to_string(config.probeTimeoutMs)),
         "#")

        ("failure-backoff",
         "Seconds an address that refused or timed out is skipped for",
         cxxopts::value<uint32_t>(config.failureBackoffSeconds)
             ->default_value(std::to_string(config.failureBackoffSeconds)),
         "#")

        ("max-targets",
         "Addresses probed per sweep. 0 probes every address known",
         cxxopts::value<uint32_t>(config.maxTargetsPerSweep)
             ->default_value(std::to_string(config.maxTargetsPerSweep)),
         "#")

        ("seed-node",
         "Extra bootstrap address, host:port or [v6]:port. Repeatable",
         cxxopts::value<std::vector<std::string>>(config.seedNodes),
         "<addr>")

        ("no-default-seeds",
         "Ignore the compiled-in seed and DNS seed lists",
         cxxopts::value<bool>(config.noDefaultSeeds)->default_value("false")->implicit_value("true"))

        ("probe-rpc",
         "Also ask each peer's RPC port for /info, the only source of a software "
         "version string. Most nodes bind RPC to loopback and will not answer",
         cxxopts::value<bool>(config.probeRpc)->default_value("false")->implicit_value("true"))

        ("rpc-port-offset",
         "Peer RPC port relative to its P2P port",
         cxxopts::value<int32_t>(config.rpcPortOffset)->default_value(std::to_string(config.rpcPortOffset)),
         "#");

    options.add_options("Data")(
        "data-dir",
        "Directory for the node table and its history",
        cxxopts::value<std::string>(config.dataDir)->default_value(config.dataDir),
        "<dir>")

        ("history-days",
         "Days of per-node daily reachability to keep",
         cxxopts::value<uint32_t>(config.historyDays)->default_value(std::to_string(config.historyDays)),
         "#")

        ("geoip-db",
         "DB-IP Lite country CSV. Without it, locations are unknown",
         cxxopts::value<std::string>(config.geoipDb)->default_value(""),
         "<file>")

        ("asn-db",
         "DB-IP Lite ASN CSV. Without it, networks are unknown",
         cxxopts::value<std::string>(config.asnDb)->default_value(""),
         "<file>");

    try
    {
        const auto result = options.parse(argc, argv);
        (void)result;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error: unable to parse command line argument options: " << e.what() << "\n\n"
                  << options.help({}) << std::endl;
        exit(1);
    }

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

    if (config.bindPort == 0)
    {
        std::cout << "--bind-port must be between 1 and 65535" << std::endl;
        exit(1);
    }

    /* The floor exists because this is other people's infrastructure. A sweep
       opens one connection to every node on the network; at a minute apart
       that is already more often than those nodes handshake each other. */
    if (config.sweepIntervalSeconds < 60)
    {
        std::cout << "--sweep-interval must be at least 60 seconds. A crawler opens a connection to every\n"
                     "node on the network each sweep; sweeping faster than the daemon's own handshake\n"
                     "interval is indistinguishable from an attack. See NETMON.md." << std::endl;
        exit(1);
    }

    if (config.concurrency == 0)
    {
        std::cout << "--concurrency must be at least 1" << std::endl;
        exit(1);
    }

    if (config.concurrency > 512)
    {
        std::cout << "--concurrency above 512 is refused: these are fibers on one dispatcher thread,\n"
                     "and the Windows dispatcher is missing two of the fiber lifetime fixes the other\n"
                     "platforms carry." << std::endl;
        exit(1);
    }

    if (config.probeTimeoutMs < 500)
    {
        std::cout << "--probe-timeout must be at least 500 ms" << std::endl;
        exit(1);
    }

    if (config.historyDays == 0 || config.historyDays > 365)
    {
        std::cout << "--history-days must be between 1 and 365" << std::endl;
        exit(1);
    }

    /* Checked here rather than at mount time. httplib's set_mount_point just
       returns false for a path that is not a directory, and a dashboard that
       404s every asset with nothing in the log to say why is a bad afternoon. */
    if (!config.webRoot.empty())
    {
        std::error_code ec;

        if (!fs::is_directory(config.webRoot, ec))
        {
            std::cout << "--web-root is not a directory: " << config.webRoot << "\n"
                      << "Point it at the extras/netmon folder, or leave it out to serve only the JSON API."
                      << std::endl;
            exit(1);
        }
    }

    for (const std::string &db : {config.geoipDb, config.asnDb})
    {
        if (db.empty())
        {
            continue;
        }

        std::error_code ec;

        if (!fs::is_regular_file(db, ec))
        {
            std::cout << "Not a file: " << db << "\nSee NETMON.md for where to get the DB-IP Lite CSVs."
                      << std::endl;
            exit(1);
        }
    }

    /* Same accident as the tx PoW server's: an unquoted `--enable-cors *` is
       expanded by the shell into the first filename in the working directory. */
    if (!config.corsHeader.empty() && config.corsHeader != "*" && config.corsHeader != "null")
    {
        const bool looksLikeOrigin =
            config.corsHeader.rfind("http://", 0) == 0 || config.corsHeader.rfind("https://", 0) == 0;

        if (!looksLikeOrigin)
        {
            std::cout << "--enable-cors must be *, null, or a full origin such as https://netmon.example.com "
                         "- got \""
                      << config.corsHeader << "\"" << std::endl;
            std::cout << "If you meant any origin, quote it: --enable-cors '*'" << std::endl;
            exit(1);
        }

        if (config.corsHeader.back() == '/')
        {
            std::cout << "--enable-cors must not have a trailing slash - got \"" << config.corsHeader << "\""
                      << std::endl;
            exit(1);
        }
    }

    return config;
}
