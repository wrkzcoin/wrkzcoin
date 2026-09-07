// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/* wrkz-netmon: walks the P2P network and serves what it finds.

   It speaks the real handshake, so everything it reports about a node is
   either something it measured itself (the port answered, how long that took,
   how often it has answered before) or something the node claimed in its
   handshake (protocol version, height, top block hash, capability flags).
   The dashboard keeps those two apart on purpose.

   It needs no blockchain and no local daemon: the network id and the genesis
   hash are compiled in, so it can bootstrap from the seed nodes on its own.
   What it does need is unrestricted outbound TCP, because peers advertise
   whatever port they like.

   See NETMON.md. */

#include "Crawler.h"
#include "GeoIp.h"
#include "HttpApi.h"
#include "NetMonConfig.h"
#include "NodeStore.h"

#include <atomic>
#include <chrono>
#include <common/SignalHandler.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <fstream>
#include <iostream>
#include <logger/Logger.h>
#include <thread>

int main(int argc, char **argv)
{
    const NetMonConfig config = parseNetMonArguments(argc, argv);

    Logger::logger.setLogLevel(config.logLevel);

    std::ofstream logFile;

    if (config.logFile)
    {
        logFile.open(*config.logFile, std::ios_base::app);

        if (!logFile)
        {
            std::cout << "Could not open log file " << *config.logFile << std::endl;
            return 1;
        }
    }

    Logger::logger.setLogCallback(
        [&config, &logFile](
            const std::string prettyMessage,
            const std::string message,
            const Logger::LogLevel level,
            const std::vector<Logger::LogCategory> categories) {
            std::cout << prettyMessage << std::endl;

            if (config.logFile)
            {
                logFile << prettyMessage << std::endl;
            }
        });

    std::cout << CryptoNote::getProjectCLIHeader() << std::endl;

    NetMon::GeoIp geoIp;

    if (!config.geoipDb.empty() || !config.asnDb.empty())
    {
        std::string error;
        uint64_t ranges = 0;
        uint64_t skipped = 0;

        if (!geoIp.load(config.geoipDb, config.asnDb, error, ranges, skipped))
        {
            std::cout << "Could not load the location database: " << error << std::endl;
            std::cout << "See NETMON.md for where to get the DB-IP Lite CSVs, or drop --geoip-db / --asn-db "
                         "and run without them."
                      << std::endl;
            return 1;
        }

        std::cout << "Location database: " << ranges << " ranges loaded";

        if (skipped != 0)
        {
            std::cout << ", " << skipped << " lines skipped";
        }

        std::cout << std::endl;
    }

    NetMon::NodeStore store(config.historyDays);

    {
        std::string error;

        if (!store.load(config.dataDir, error))
        {
            /* A table that will not parse is not worth starting over silently:
               the operator loses every day of history it held. Recovery is a
               rename, so name the file to rename. */
            std::cout << "Could not load the node table: " << error << "\n" << std::endl;

            const std::vector<std::string> backups = NetMon::NodeStore::listBackups(config.dataDir);

            if (backups.empty())
            {
                std::cout << "There are no backups in " << config.dataDir
                          << ". Move or delete nodes.ndjson to start a fresh crawl;\n"
                             "the seeds will rebuild the address set within a sweep or two."
                          << std::endl;
            }
            else
            {
                std::cout << "Backups available in " << config.dataDir << ", newest first:" << std::endl;

                for (const std::string &backup : backups)
                {
                    std::cout << "  " << backup << std::endl;
                }

                std::cout << "\nTo recover, put the newest one back and restart:\n"
                          << "  mv " << config.dataDir << "/" << backups.front() << " "
                          << config.dataDir << "/nodes.ndjson\n\n"
                          << "Or move nodes.ndjson aside to start a fresh crawl instead." << std::endl;
            }

            return 1;
        }
    }

    const NetMon::Summary loaded = store.summarise();

    if (loaded.known != 0)
    {
        std::cout << "Resuming from " << loaded.known << " known addresses (" << loaded.reachable
                  << " reachable at the last sweep)" << std::endl;
    }

    NetMon::Crawler crawler(config, store, geoIp);

    if (!crawler.start())
    {
        return 1;
    }

    NetMon::HttpApi api(config, store, crawler);

    if (!api.start())
    {
        crawler.stop();
        return 1;
    }

    std::cout << "\nNetwork monitor listening on http://" << config.bindIp << ":" << config.bindPort;

    if (!config.bindIpv6Address.empty())
    {
        std::cout << " and http://[" << config.bindIpv6Address << "]:" << config.bindPort;
    }

    std::cout << "\n  dashboard:  "
              << (config.webRoot.empty() ? std::string("not served (no --web-root); JSON API only")
                                         : config.webRoot)
              << "\n  api:        /api/stats (aggregates, safe to publish)"
              << "\n              /api/summary  /api/peers  /api/peers/<addr:port>  /api/geo  /api/versions"
              << "\n  sweep:      every " << config.sweepIntervalSeconds << " s, " << config.concurrency
              << " probes in flight, " << config.probeTimeoutMs << " ms each"
              << "\n  data:       " << config.dataDir << " (" << config.historyDays
              << " days of history, "
              << (config.backupDays == 0 ? std::string("no backups")
                                         : std::to_string(config.backupDays) + " daily backups")
              << ")"
              << "\n  location:   "
              << (config.geoipDb.empty() && config.asnDb.empty() ? std::string("no database loaded")
                                                                 : std::string("loaded"))
              << "\n  rpc probe:  " << (config.probeRpc ? "on" : "off (no software versions)") << std::endl;

    std::cout << "\nThis crawler announces my_port=0 and height 0, and hangs up after one handshake, so it is\n"
                 "never added to a peer list, never poisons a peer's observed height, and never holds an\n"
                 "inbound slot. Do not change those. See NETMON.md."
              << std::endl;

    std::atomic<bool> stopRequested(false);

    Tools::SignalHandler::install([&stopRequested] { stopRequested = true; });

    while (!stopRequested)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::cout << "\nShutting down..." << std::endl;

    api.stop();
    crawler.stop();

    {
        std::string error;

        if (!store.save(config.dataDir, error))
        {
            std::cout << "Could not save the node table: " << error << std::endl;
        }
        else if (!NetMon::NodeStore::rotateBackups(config.dataDir, config.backupDays, error))
        {
            std::cout << "Could not rotate the node table backups: " << error << std::endl;
        }
    }

    return 0;
}
