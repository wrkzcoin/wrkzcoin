// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <logger/Logger.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct NetMonConfig
{
    /* ---- HTTP listener -------------------------------------------------- */

    /* Where the dashboard and its JSON API are served. Loopback by default:
       this thing has no authentication of its own and publishes per-node
       addresses, so exposing it is a deliberate act. See NETMON.md. */
    std::string bindIp = "127.0.0.1";

    uint16_t bindPort = 17871;

    /* Second listener on the same port. Empty = disabled. */
    std::string bindIpv6Address;

    /* Directory of static files (extras/netmon) served at /. Empty serves the
       JSON API only, which is what you want behind a web server that has the
       files itself. */
    std::string webRoot;

    /* Access-Control-Allow-Origin value. Empty = no CORS headers. */
    std::string corsHeader;

    /* Reverse proxies whose X-Real-IP / X-Forwarded-For may be believed when
       logging a client. Requests from any other address keep their own. */
    std::vector<std::string> trustedProxies;

    /* ---- Crawler -------------------------------------------------------- */

    /* Seconds between sweeps. The daemon re-dials a failed address no sooner
       than 10 minutes and handshakes a live one once a minute, so anything in
       the 5-15 minute range is indistinguishable from an ordinary peer. Below
       60 s is refused. */
    uint32_t sweepIntervalSeconds = 600;

    /* Probes in flight at once. These are fibers on one dispatcher thread, not
       OS threads. Kept modest on Windows, whose dispatcher did not receive two
       of the fiber lifetime fixes the other platforms have. */
    uint32_t concurrency = 64;

    /* Per-probe ceiling: connect, one COMMAND_HANDSHAKE, close. The daemon
       gives its own handshake 5 s (P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT). */
    uint32_t probeTimeoutMs = 5000;

    /* An address that refused or timed out is left alone this long, matching
       the daemon's P2P_FAILED_PEER_FORGET_SECONDS, so one dead host cannot
       eat a slot in every sweep. */
    uint32_t failureBackoffSeconds = 600;

    /* Addresses probed per sweep. 0 = every address known. */
    uint32_t maxTargetsPerSweep = 0;

    /* Extra bootstrap addresses, "host:port" or "[v6]:port", on top of the
       compiled-in seeds. */
    std::vector<std::string> seedNodes;

    /* Skip the compiled-in SEED_NODES / DNS_SEED_NODES. Only sensible with
       --seed-node, and mostly useful against a test network. */
    bool noDefaultSeeds = false;

    /* Also probe RPC on the peer's port + 1 for /info, which is the only place
       a software version string exists. Most nodes bind RPC to loopback, so
       expect this to answer for a minority. Off by default: it is a second
       connection to every node in the network. */
    bool probeRpc = false;

    /* Port offset used for that probe. RPC_DEFAULT_PORT - P2P_DEFAULT_PORT. */
    int32_t rpcPortOffset = 1;

    uint32_t rpcTimeoutMs = 3000;

    /* ---- Data ----------------------------------------------------------- */

    /* Where the node table and its history are kept. Created if absent. */
    std::string dataDir = "netmon-data";

    /* Days of per-node daily reachability to keep. */
    uint32_t historyDays = 30;

    /* Daily copies of the node table to keep beside it, so a table that will
       not parse can be recovered by renaming yesterday's over it rather than
       re-crawling from the seeds. 0 disables them. */
    uint32_t backupDays = 7;

    /* DB-IP Lite CSVs. Both optional; without them the map is empty and every
       other view still works. See NETMON.md for how to fetch them. */
    std::string geoipDb;

    std::string asnDb;

    /* ---- Logging -------------------------------------------------------- */

    Logger::LogLevel logLevel = Logger::INFO;

    std::optional<std::string> logFile;
};

NetMonConfig parseNetMonArguments(int argc, char **argv);
