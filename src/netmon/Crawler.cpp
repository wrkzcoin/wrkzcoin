// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "Crawler.h"

#include <algorithm>
#include <chrono>
#include <config/CryptoNoteConfig.h>
#include <logger/Logger.h>
#include <system/ContextGroup.h>
#include <system/Dispatcher.h>
#include <system/IpAddress.h>
#include <system/IpResolver.h>

namespace NetMon
{
    namespace
    {
        /* Matches the daemon's P2P_SEED_RERESOLVE_INTERVAL_SECONDS. */
        constexpr uint64_t SEED_RERESOLVE_SECONDS = 60 * 60;

        constexpr size_t RPC_PROBE_THREADS = 16;

        uint64_t unixNow()
        {
            return static_cast<uint64_t>(std::time(nullptr));
        }

        void logLine(const std::string &message, const Logger::LogLevel level)
        {
            Logger::logger.log(message, level, {Logger::DAEMON});
        }

        /* "host:port", "[v6]:port", or a bare host that takes the default P2P
           port. Hostnames are resolved by the caller. */
        bool splitHostPort(const std::string &text, std::string &host, uint16_t &port)
        {
            port = static_cast<uint16_t>(CryptoNote::P2P_DEFAULT_PORT);

            if (text.empty())
            {
                return false;
            }

            if (text.front() == '[')
            {
                const size_t close = text.find(']');

                if (close == std::string::npos)
                {
                    return false;
                }

                host = text.substr(1, close - 1);

                if (close + 1 < text.size() && text[close + 1] == ':')
                {
                    port = static_cast<uint16_t>(std::stoul(text.substr(close + 2)));
                }

                return !host.empty();
            }

            const size_t colon = text.rfind(':');

            /* A bare IPv6 literal has several colons and no port. */
            if (colon == std::string::npos || text.find(':') != colon)
            {
                host = text;
                return true;
            }

            host = text.substr(0, colon);

            try
            {
                port = static_cast<uint16_t>(std::stoul(text.substr(colon + 1)));
            }
            catch (...)
            {
                return false;
            }

            return !host.empty() && port != 0;
        }
    } // namespace

    Crawler::Crawler(const NetMonConfig &config, NodeStore &store, const GeoIp &geoIp):
        m_config(config),
        m_store(store),
        m_geoIp(geoIp)
    {
    }

    Crawler::~Crawler()
    {
        stop();
    }

    bool Crawler::start()
    {
        std::string error;

        if (!buildCrawlerIdentity(m_identity, error))
        {
            logLine("Cannot start the crawler: " + error, Logger::FATAL);
            return false;
        }

        m_thread = std::thread(&Crawler::run, this);

        return true;
    }

    void Crawler::stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_stopMutex);
            m_stop = true;
        }

        m_stopCv.notify_all();

        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    std::vector<std::string> Crawler::seeds() const
    {
        std::lock_guard<std::mutex> lock(m_seedMutex);

        return m_seeds;
    }

    bool Crawler::sleepFor(const uint32_t seconds)
    {
        std::unique_lock<std::mutex> lock(m_stopMutex);

        m_stopCv.wait_for(lock, std::chrono::seconds(seconds), [this] { return m_stop.load(); });

        return !m_stop;
    }

    void Crawler::resolveSeeds()
    {
        std::vector<std::string> resolved;

        const auto addSeed = [&](const std::string &host, const uint16_t port) {
            /* An address literal needs no lookup and no thread; a hostname
               does. IpResolver's default constructor has no dispatcher, which
               is what makes it usable from this plain thread. */
            std::vector<System::IpAddress> addresses;

            try
            {
                addresses.push_back(System::IpAddress(host));
            }
            catch (...)
            {
                try
                {
                    System::IpResolver resolver;
                    addresses = resolver.resolveAll(host);
                }
                catch (const std::exception &e)
                {
                    logLine("Could not resolve seed " + host + ": " + e.what(), Logger::WARNING);
                    return;
                }
            }

            for (const System::IpAddress &address : addresses)
            {
                const std::string text = address.toString();

                m_store.addCandidate(text, port, true);
                resolved.push_back(text + ":" + std::to_string(port));
            }
        };

        if (!m_config.noDefaultSeeds)
        {
            for (const char *const seed : CryptoNote::SEED_NODES)
            {
                std::string host;
                uint16_t port = 0;

                if (splitHostPort(seed, host, port))
                {
                    addSeed(host, port);
                }
            }

            for (const char *const seed : CryptoNote::DNS_SEED_NODES)
            {
                addSeed(seed, static_cast<uint16_t>(CryptoNote::P2P_DEFAULT_PORT));
            }
        }

        for (const std::string &seed : m_config.seedNodes)
        {
            std::string host;
            uint16_t port = 0;

            if (splitHostPort(seed, host, port))
            {
                addSeed(host, port);
            }
            else
            {
                logLine("Ignoring --seed-node " + seed + ": expected host:port or [v6]:port", Logger::WARNING);
            }
        }

        /* The static seeds and the DNS seeds routinely resolve to the same
           hosts - node-fin.wrkz.work and seeds.wrkz.work answer with the same
           two addresses today - so the raw list double-counts them. */
        std::sort(resolved.begin(), resolved.end());
        resolved.erase(std::unique(resolved.begin(), resolved.end()), resolved.end());

        {
            std::lock_guard<std::mutex> lock(m_seedMutex);
            m_seeds = resolved;
        }

        m_lastSeedResolve = unixNow();

        logLine(
            "Seed bootstrap resolved " + std::to_string(resolved.size()) + " distinct addresses",
            Logger::INFO);
    }

    void Crawler::run()
    {
        resolveSeeds();

        while (!m_stop)
        {
            if (unixNow() > m_lastSeedResolve + SEED_RERESOLVE_SECONDS)
            {
                resolveSeeds();
            }

            runP2pSweep();

            if (!m_stop && m_config.probeRpc)
            {
                runRpcSweep();
            }

            if (!m_stop)
            {
                persist();
            }

            if (!sleepFor(m_config.sweepIntervalSeconds))
            {
                break;
            }
        }
    }

    void Crawler::runP2pSweep()
    {
        const uint64_t started = unixNow();

        m_store.beginSweep(started);

        const std::vector<std::pair<std::string, uint16_t>> targets =
            m_store.pickTargets(started, m_config.failureBackoffSeconds, m_config.maxTargetsPerSweep);

        if (targets.empty())
        {
            logLine("Sweep found no addresses to probe; seeds may be unreachable", Logger::WARNING);
            m_store.endSweep(unixNow(), 0, 0);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_rpcQueueMutex);
            m_rpcQueue.clear();
        }

        /* Everything below runs as fibers on this dispatcher, so the counters
           need no synchronisation: only one of them is ever running. */
        size_t nextTarget = 0;
        uint32_t probed = 0;
        uint32_t reachable = 0;
        uint32_t discovered = 0;

        {
            System::Dispatcher dispatcher;
            System::ContextGroup workers(dispatcher);

            const uint32_t workerCount =
                static_cast<uint32_t>(std::min<size_t>(m_config.concurrency, targets.size()));

            for (uint32_t i = 0; i < workerCount; i++)
            {
                workers.spawn([&] {
                    while (!m_stop)
                    {
                        if (nextTarget >= targets.size())
                        {
                            return;
                        }

                        const std::pair<std::string, uint16_t> target = targets[nextTarget++];

                        probed++;

                        ProbeResult result;
                        result.address = target.first;
                        result.port = target.second;

                        try
                        {
                            const System::IpAddress address(target.first);

                            result = probePeer(
                                dispatcher, address, target.second, m_config.probeTimeoutMs, m_identity);
                        }
                        catch (const std::exception &e)
                        {
                            result.reach = Reach::Failed;
                            result.error = e.what();
                        }

                        if (result.reach == Reach::Open)
                        {
                            reachable++;
                        }
                        else if (Logger::logger.shouldLog(Logger::DEBUG))
                        {
                            logLine(
                                "Probe of " + target.first + ":" + std::to_string(target.second) + " -> "
                                    + reachToString(result.reach) + " (" + result.error + ")",
                                Logger::DEBUG);
                        }

                        for (const HarvestedPeer &peer : result.harvested)
                        {
                            if (m_store.addCandidate(peer.address, peer.port, false))
                            {
                                discovered++;
                            }
                        }

                        m_store.recordProbe(
                            target.first,
                            target.second,
                            result.reach,
                            result.rttMs,
                            unixNow(),
                            result.reach == Reach::Open ? &result.reported : nullptr);

                        if (result.reach == Reach::Open)
                        {
                            const std::string key = target.first + ":" + std::to_string(target.second);

                            if (m_geoIp.haveCountry() || m_geoIp.haveAsn())
                            {
                                const Location location = m_geoIp.lookup(target.first);
                                m_store.setLocation(key, location.country, location.asn, location.asName);
                            }

                            if (m_config.probeRpc)
                            {
                                const int64_t rpcPort =
                                    static_cast<int64_t>(target.second) + m_config.rpcPortOffset;

                                if (rpcPort > 0 && rpcPort < 65536)
                                {
                                    std::lock_guard<std::mutex> lock(m_rpcQueueMutex);
                                    m_rpcQueue.emplace_back(target.first, static_cast<uint16_t>(rpcPort));
                                }
                            }
                        }
                    }
                });
            }

            workers.wait();
        }

        const uint64_t finished = unixNow();

        m_store.endSweep(finished, probed, reachable);

        logLine(
            "Sweep " + std::to_string(m_store.summarise().sweepNumber) + ": probed " + std::to_string(probed)
                + ", reachable " + std::to_string(reachable) + ", new addresses " + std::to_string(discovered)
                + ", took " + std::to_string(finished - started) + " s",
            Logger::INFO);
    }

    void Crawler::runRpcSweep()
    {
        std::vector<std::pair<std::string, uint16_t>> queue;

        {
            std::lock_guard<std::mutex> lock(m_rpcQueueMutex);
            queue.swap(m_rpcQueue);
        }

        if (queue.empty())
        {
            return;
        }

        /* Plain threads, not fibers: httplib blocks, and a blocking call
           inside a fiber stalls every other fiber on that dispatcher. */
        std::atomic<size_t> next {0};
        std::atomic<uint32_t> answered {0};

        std::vector<std::thread> threads;
        const size_t threadCount = std::min(RPC_PROBE_THREADS, queue.size());

        threads.reserve(threadCount);

        for (size_t i = 0; i < threadCount; i++)
        {
            threads.emplace_back([&] {
                for (;;)
                {
                    if (m_stop)
                    {
                        return;
                    }

                    const size_t index = next++;

                    if (index >= queue.size())
                    {
                        return;
                    }

                    const std::pair<std::string, uint16_t> &target = queue[index];

                    const std::string version =
                        probeSoftwareVersion(target.first, target.second, m_config.rpcTimeoutMs);

                    if (!version.empty())
                    {
                        /* The store is keyed on the P2P port, which is the one
                           the offset was applied to. */
                        const int64_t p2pPort = static_cast<int64_t>(target.second) - m_config.rpcPortOffset;

                        if (p2pPort > 0 && p2pPort < 65536)
                        {
                            m_store.setSoftwareVersion(
                                target.first + ":" + std::to_string(p2pPort), version);
                            answered++;
                        }
                    }
                }
            });
        }

        for (std::thread &thread : threads)
        {
            thread.join();
        }

        logLine(
            "RPC probe: " + std::to_string(answered.load()) + " of " + std::to_string(queue.size())
                + " reachable nodes published a version",
            Logger::INFO);
    }

    void Crawler::persist()
    {
        std::string error;

        if (!m_store.save(m_config.dataDir, error))
        {
            logLine("Could not save the node table: " + error, Logger::WARNING);
        }
    }
} // namespace NetMon
