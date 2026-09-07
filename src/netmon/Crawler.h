// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "GeoIp.h"
#include "NetMonConfig.h"
#include "NodeStore.h"
#include "Probe.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace NetMon
{
    /* Walks the network: dial an address, complete one handshake, take the
       peer list it answers with, repeat over everything that turns up. Runs on
       one thread of its own.

       Each sweep builds a fresh System::Dispatcher and runs the probes as
       fibers on it. A dispatcher per sweep rather than one for the process
       lifetime is deliberate: the optional RPC phase uses blocking sockets and
       would stall every fiber if it ran inside one, and at ten-minute
       intervals the setup cost does not register. */
    class Crawler
    {
      public:
        Crawler(const NetMonConfig &config, NodeStore &store, const GeoIp &geoIp);

        ~Crawler();

        Crawler(const Crawler &) = delete;

        Crawler &operator=(const Crawler &) = delete;

        bool start();

        void stop();

        /* Seed addresses resolved at the last bootstrap, for the status line
           and /api/summary. */
        std::vector<std::string> seeds() const;

      private:
        void run();

        /* Resolves the compiled-in seeds plus any --seed-node, adds them as
           candidates and marks them. Re-run hourly, matching the daemon's
           P2P_SEED_RERESOLVE_INTERVAL_SECONDS, so a seed that moved is picked
           up without a restart. */
        void resolveSeeds();

        void runP2pSweep();

        void runRpcSweep();

        void persist();

        /* Returns false when stop() was called while waiting. */
        bool sleepFor(uint32_t seconds);

        const NetMonConfig &m_config;

        NodeStore &m_store;

        const GeoIp &m_geoIp;

        CrawlerIdentity m_identity;

        std::thread m_thread;

        std::atomic<bool> m_stop {false};

        std::mutex m_stopMutex;

        std::condition_variable m_stopCv;

        mutable std::mutex m_seedMutex;

        std::vector<std::string> m_seeds;

        uint64_t m_lastSeedResolve = 0;

        /* Addresses whose RPC port is worth asking, filled by the P2P sweep
           and drained by the RPC sweep. Only used with --probe-rpc. */
        std::vector<std::pair<std::string, uint16_t>> m_rpcQueue;

        std::mutex m_rpcQueueMutex;
    };
} // namespace NetMon
