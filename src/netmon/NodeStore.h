// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NetMon
{
    /* What the last probe of an address found. Everything except Open is a
       "grey" address in the daemon's vocabulary: known, but not usable. */
    enum class Reach : uint8_t
    {
        Unknown = 0, /* never probed */
        Open = 1,    /* handshake completed */
        Timeout = 2, /* connected or not, no answer in time */
        Refused = 3, /* connection actively refused */
        Failed = 4,  /* answered, but not with a usable handshake */
    };

    const char *reachToString(Reach reach);

    struct DayStat
    {
        uint32_t offered = 0;

        uint32_t answered = 0;
    };

    struct NodeRecord
    {
        /* Identity. address is System::IpAddress::toString(), so IPv6 arrives
           already bracketed and "address:port" is unambiguous either way. */
        std::string address;

        uint16_t port = 0;

        bool isSeed = false;

        /* ---- measured by us ---- */
        Reach reach = Reach::Unknown;

        uint64_t rttMs = 0;

        uint64_t firstSeen = 0;

        uint64_t lastSeen = 0; /* last completed handshake */

        uint64_t lastTried = 0;

        uint64_t sweepsOffered = 0;

        uint64_t sweepsAnswered = 0;

        uint32_t peersAdvertised = 0;

        /* ---- claimed by the node ---- */
        bool haveHandshake = false;

        uint8_t p2pVersion = 0;

        uint64_t peerId = 0;

        uint32_t height = 0;

        Crypto::Hash topId {};

        uint32_t capabilityFlags = 0;

        uint32_t prunedHeight = 0;

        uint32_t liteStartHeight = 0;

        int64_t clockSkewSeconds = 0;

        uint32_t advertisedPort = 0;

        /* Only ever set from an RPC probe, which most nodes refuse. */
        std::string softwareVersion;

        /* ---- derived from a database, not observed ---- */
        std::string country;

        std::string asn;

        std::string asName;

        /* dayIndex (unix days) -> offered/answered */
        std::map<uint32_t, DayStat> history;

        std::string key() const;

        bool isPruned() const;

        bool isLite() const;
    };

    /* One point of the "reachable nodes over time" series. */
    struct SweepPoint
    {
        uint64_t finishedAt = 0;

        uint32_t probed = 0;

        uint32_t reachable = 0;
    };

    struct Bucket
    {
        std::string label;

        uint64_t count = 0;
    };

    struct Summary
    {
        uint64_t known = 0;

        uint64_t reachable = 0;

        uint64_t grey = 0;

        uint32_t networkHeight = 0;

        uint64_t ipv4 = 0;

        uint64_t ipv6 = 0;

        uint64_t full = 0;

        uint64_t pruned = 0;

        uint64_t lite = 0;

        uint64_t softwareKnown = 0;

        uint64_t rttMedianMs = 0;

        uint64_t rttP95Ms = 0;

        uint64_t clockWithin5s = 0;

        uint64_t clockUnder60s = 0;

        uint64_t clockOver60s = 0;

        uint64_t joined24h = 0;

        uint64_t left24h = 0;

        /* Reachable nodes exactly at networkHeight. heightBuckets carries the
           same figure, but a consumer building a one-line summary should not
           have to know which bucket index means "at the tip". */
        uint64_t atTip = 0;

        /* Distinct top block hashes across reachable nodes. One is agreement;
           more than one means somebody is on a different chain, which is the
           single most important thing this tool can say. */
        uint64_t distinctTips = 0;

        /* Median of what the pruned and lite nodes report about themselves, or
           0 when there are none of that kind. A count alone does not answer
           "how much of the chain does the network actually still hold". */
        uint64_t prunedMedianDepth = 0;

        uint64_t liteMedianFloor = 0;

        /* Reachable nodes by how much of their offered sweeps they answered,
           over the whole retained history. */
        uint64_t uptimeOver99 = 0;

        uint64_t uptimeOver90 = 0;

        uint64_t uptimeUnder50 = 0;

        /* Reachable nodes the location database could place. */
        uint64_t located = 0;

        /* Share of reachable nodes held by the three largest autonomous
           systems, as a percentage. The decentralisation number worth
           publishing; the dashboard used to compute it in JavaScript. */
        double top3NetworkShare = 0.0;

        /* Earliest firstSeen across reachable nodes, so a consumer can say how
           long the longest-serving node has been up. 0 when nothing is known. */
        uint64_t oldestFirstSeen = 0;

        /* Heights relative to networkHeight. */
        std::vector<Bucket> heightBuckets;

        /* Top block hash -> nodes claiming it, commonest first. */
        std::vector<Bucket> topHashes;

        /* P2P protocol version -> nodes. */
        std::vector<Bucket> versions;

        /* Two-letter country -> nodes, commonest first. */
        std::vector<Bucket> countries;

        /* "AS24940 Hetzner" -> nodes, commonest first. */
        std::vector<Bucket> networks;

        uint64_t sweepNumber = 0;

        uint64_t lastSweepStarted = 0;

        uint64_t lastSweepFinished = 0;

        uint32_t lastSweepProbed = 0;

        uint32_t lastSweepDurationMs = 0;
    };

    /* Written by the crawler thread, read by the HTTP threads. Every public
       method takes the lock; nothing hands out a reference to the interior. */
    class NodeStore
    {
      public:
        explicit NodeStore(uint32_t historyDays);

        /* Adds an address if it is new. Returns true when it was new, which is
           what the "N new to us" figure counts. */
        bool addCandidate(const std::string &address, uint16_t port, bool isSeed);

        /* Merges one probe outcome. dayIndex is unix time / 86400. */
        void recordProbe(
            const std::string &address,
            uint16_t port,
            Reach reach,
            uint64_t rttMs,
            uint64_t now,
            const NodeRecord *reported);

        void setLocation(
            const std::string &key,
            const std::string &country,
            const std::string &asn,
            const std::string &asName);

        void setSoftwareVersion(const std::string &key, const std::string &version);

        /* Addresses worth probing this sweep: everything known, minus those
           still inside their failure backoff. Capped when maxTargets != 0,
           oldest-tried first so a cap cannot starve anyone. */
        std::vector<std::pair<std::string, uint16_t>>
            pickTargets(uint64_t now, uint32_t backoffSeconds, uint32_t maxTargets) const;

        std::vector<NodeRecord> snapshot() const;

        bool find(const std::string &key, NodeRecord &out) const;

        Summary summarise() const;

        std::vector<SweepPoint> sweepHistory() const;

        void beginSweep(uint64_t now);

        void endSweep(uint64_t now, uint32_t probed, uint32_t reachable);

        /* Newline-delimited JSON, one node per line, plus a header line. A
           partial write leaves the previous file untouched: it is written
           beside the target and renamed over it. */
        bool save(const std::string &dataDir, std::string &error) const;

        bool load(const std::string &dataDir, std::string &error);

        /* Keeps one copy of the node table per day beside it, named
           nodes-YYYY-MM-DD.ndjson, and prunes to the newest keepDays. Called
           after a successful save; does nothing when keepDays is 0, and does
           nothing more than once a day.

           The point is recovery by rename: if nodes.ndjson will not parse,
           moving a backup over it brings the crawl back with at most a day
           lost, instead of starting again from the seeds. */
        static bool rotateBackups(const std::string &dataDir, uint32_t keepDays, std::string &error);

        /* Backup filenames present, newest first. Used to make the "table will
           not load" message name the file to rename. */
        static std::vector<std::string> listBackups(const std::string &dataDir);

      private:
        /* Caller holds m_mutex. */
        void pruneHistory(NodeRecord &record) const;

        mutable std::mutex m_mutex;

        std::unordered_map<std::string, NodeRecord> m_nodes;

        std::deque<SweepPoint> m_sweeps;

        uint32_t m_historyDays;

        uint64_t m_sweepNumber = 0;

        uint64_t m_lastSweepStarted = 0;

        uint64_t m_lastSweepFinished = 0;

        uint32_t m_lastSweepProbed = 0;

        uint32_t m_lastSweepDurationMs = 0;
    };
} // namespace NetMon
