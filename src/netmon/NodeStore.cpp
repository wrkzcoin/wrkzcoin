// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "NodeStore.h"

#include "json.hpp"

#include <common/StringTools.h>
#include <p2p/P2pProtocolDefinitions.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace NetMon
{
    namespace
    {
        constexpr uint64_t SECONDS_PER_DAY = 86400;

        /* Points kept for the "reachable nodes over time" series. At the
           default ten-minute sweep this is a little over a week. */
        constexpr size_t MAX_SWEEP_POINTS = 1024;

        uint32_t dayIndexOf(const uint64_t unixTime)
        {
            return static_cast<uint32_t>(unixTime / SECONDS_PER_DAY);
        }

        void sortBucketsDescending(std::vector<Bucket> &buckets)
        {
            std::sort(buckets.begin(), buckets.end(), [](const Bucket &a, const Bucket &b) {
                if (a.count != b.count)
                {
                    return a.count > b.count;
                }

                return a.label < b.label;
            });
        }
    } // namespace

    const char *reachToString(const Reach reach)
    {
        switch (reach)
        {
            case Reach::Open:
                return "open";
            case Reach::Timeout:
                return "timeout";
            case Reach::Refused:
                return "refused";
            case Reach::Failed:
                return "failed";
            case Reach::Unknown:
            default:
                return "unknown";
        }
    }

    namespace
    {
        Reach reachFromString(const std::string &text)
        {
            if (text == "open")
            {
                return Reach::Open;
            }

            if (text == "timeout")
            {
                return Reach::Timeout;
            }

            if (text == "refused")
            {
                return Reach::Refused;
            }

            if (text == "failed")
            {
                return Reach::Failed;
            }

            return Reach::Unknown;
        }
    } // namespace

    std::string NodeRecord::key() const
    {
        return address + ":" + std::to_string(port);
    }

    bool NodeRecord::isPruned() const
    {
        return (capabilityFlags & CryptoNote::NODE_CAPABILITY_FLAG_PRUNED) != 0;
    }

    bool NodeRecord::isLite() const
    {
        return (capabilityFlags & CryptoNote::NODE_CAPABILITY_FLAG_LITE) != 0;
    }

    NodeStore::NodeStore(const uint32_t historyDays): m_historyDays(historyDays) {}

    bool NodeStore::addCandidate(const std::string &address, const uint16_t port, const bool isSeed)
    {
        if (address.empty() || port == 0)
        {
            return false;
        }

        const std::string key = address + ":" + std::to_string(port);

        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_nodes.find(key);

        if (it != m_nodes.end())
        {
            /* A seed we already knew about keeps the flag. */
            it->second.isSeed = it->second.isSeed || isSeed;
            return false;
        }

        NodeRecord record;
        record.address = address;
        record.port = port;
        record.isSeed = isSeed;

        m_nodes.emplace(key, std::move(record));

        return true;
    }

    void NodeStore::recordProbe(
        const std::string &address,
        const uint16_t port,
        const Reach reach,
        const uint64_t rttMs,
        const uint64_t now,
        const NodeRecord *reported)
    {
        const std::string key = address + ":" + std::to_string(port);

        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_nodes.find(key);

        if (it == m_nodes.end())
        {
            NodeRecord fresh;
            fresh.address = address;
            fresh.port = port;
            it = m_nodes.emplace(key, std::move(fresh)).first;
        }

        NodeRecord &record = it->second;

        record.reach = reach;
        record.lastTried = now;
        record.sweepsOffered++;

        DayStat &day = record.history[dayIndexOf(now)];
        day.offered++;

        if (reach == Reach::Open)
        {
            record.lastSeen = now;
            record.rttMs = rttMs;
            record.sweepsAnswered++;
            day.answered++;

            if (record.firstSeen == 0)
            {
                record.firstSeen = now;
            }
        }

        /* Only a completed handshake refreshes the claimed fields. A node that
           has gone away keeps the last thing it said, which is what the table
           should show beside a "last seen 6 d" - not a row of blanks. */
        if (reported != nullptr && reach == Reach::Open)
        {
            record.haveHandshake = true;
            record.p2pVersion = reported->p2pVersion;
            record.peerId = reported->peerId;
            record.height = reported->height;
            record.topId = reported->topId;
            record.capabilityFlags = reported->capabilityFlags;
            record.prunedHeight = reported->prunedHeight;
            record.liteStartHeight = reported->liteStartHeight;
            record.clockSkewSeconds = reported->clockSkewSeconds;
            record.advertisedPort = reported->advertisedPort;
            record.peersAdvertised = reported->peersAdvertised;
        }

        pruneHistory(record);
    }

    void NodeStore::setLocation(
        const std::string &key,
        const std::string &country,
        const std::string &asn,
        const std::string &asName)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_nodes.find(key);

        if (it == m_nodes.end())
        {
            return;
        }

        it->second.country = country;
        it->second.asn = asn;
        it->second.asName = asName;
    }

    void NodeStore::setSoftwareVersion(const std::string &key, const std::string &version)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_nodes.find(key);

        if (it != m_nodes.end())
        {
            it->second.softwareVersion = version;
        }
    }

    std::vector<std::pair<std::string, uint16_t>>
        NodeStore::pickTargets(const uint64_t now, const uint32_t backoffSeconds, const uint32_t maxTargets) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<const NodeRecord *> candidates;
        candidates.reserve(m_nodes.size());

        for (const auto &entry : m_nodes)
        {
            const NodeRecord &record = entry.second;

            /* A seed is always dialled: it is how the crawler recovers when
               every address it holds has gone stale. */
            if (!record.isSeed && record.reach != Reach::Open && record.reach != Reach::Unknown
                && record.lastTried != 0 && now < record.lastTried + backoffSeconds)
            {
                continue;
            }

            candidates.push_back(&record);
        }

        /* Oldest attempt first, so a --max-targets cap rotates through the
           whole set instead of re-probing the same head every sweep. */
        std::sort(candidates.begin(), candidates.end(), [](const NodeRecord *a, const NodeRecord *b) {
            if (a->lastTried != b->lastTried)
            {
                return a->lastTried < b->lastTried;
            }

            return a->address < b->address;
        });

        if (maxTargets != 0 && candidates.size() > maxTargets)
        {
            candidates.resize(maxTargets);
        }

        std::vector<std::pair<std::string, uint16_t>> targets;
        targets.reserve(candidates.size());

        for (const NodeRecord *record : candidates)
        {
            targets.emplace_back(record->address, record->port);
        }

        return targets;
    }

    std::vector<NodeRecord> NodeStore::snapshot() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<NodeRecord> out;
        out.reserve(m_nodes.size());

        for (const auto &entry : m_nodes)
        {
            out.push_back(entry.second);
        }

        return out;
    }

    bool NodeStore::find(const std::string &key, NodeRecord &out) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        const auto it = m_nodes.find(key);

        if (it == m_nodes.end())
        {
            return false;
        }

        out = it->second;

        return true;
    }

    std::vector<SweepPoint> NodeStore::sweepHistory() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return {m_sweeps.begin(), m_sweeps.end()};
    }

    void NodeStore::beginSweep(const uint64_t now)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_sweepNumber++;
        m_lastSweepStarted = now;
    }

    void NodeStore::endSweep(const uint64_t now, const uint32_t probed, const uint32_t reachable)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_lastSweepFinished = now;
        m_lastSweepProbed = probed;
        m_lastSweepDurationMs =
            m_lastSweepStarted == 0 ? 0 : static_cast<uint32_t>((now - m_lastSweepStarted) * 1000);

        SweepPoint point;
        point.finishedAt = now;
        point.probed = probed;
        point.reachable = reachable;

        m_sweeps.push_back(point);

        while (m_sweeps.size() > MAX_SWEEP_POINTS)
        {
            m_sweeps.pop_front();
        }
    }

    void NodeStore::pruneHistory(NodeRecord &record) const
    {
        while (record.history.size() > m_historyDays)
        {
            record.history.erase(record.history.begin());
        }
    }

    Summary NodeStore::summarise() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        Summary summary;
        summary.known = m_nodes.size();
        summary.sweepNumber = m_sweepNumber;
        summary.lastSweepStarted = m_lastSweepStarted;
        summary.lastSweepFinished = m_lastSweepFinished;
        summary.lastSweepProbed = m_lastSweepProbed;
        summary.lastSweepDurationMs = m_lastSweepDurationMs;

        std::vector<uint64_t> rtts;
        std::map<std::string, uint64_t> hashCounts;
        std::map<uint32_t, uint64_t> versionCounts;
        std::map<std::string, uint64_t> countryCounts;
        std::map<std::string, uint64_t> networkCounts;

        for (const auto &entry : m_nodes)
        {
            const NodeRecord &record = entry.second;

            if (record.reach != Reach::Open)
            {
                continue;
            }

            summary.reachable++;

            if (record.address.rfind('[', 0) == 0)
            {
                summary.ipv6++;
            }
            else
            {
                summary.ipv4++;
            }

            if (record.isLite())
            {
                summary.lite++;
            }
            else if (record.isPruned())
            {
                summary.pruned++;
            }
            else
            {
                summary.full++;
            }

            if (!record.softwareVersion.empty())
            {
                summary.softwareKnown++;
            }

            if (record.rttMs != 0)
            {
                rtts.push_back(record.rttMs);
            }

            const int64_t skew = record.clockSkewSeconds < 0 ? -record.clockSkewSeconds : record.clockSkewSeconds;

            if (skew <= 5)
            {
                summary.clockWithin5s++;
            }
            else if (skew <= 60)
            {
                summary.clockUnder60s++;
            }
            else
            {
                summary.clockOver60s++;
            }

            summary.networkHeight = std::max(summary.networkHeight, record.height);

            if (record.haveHandshake && record.height != 0)
            {
                hashCounts[Common::podToHex(record.topId)]++;
            }

            versionCounts[record.p2pVersion]++;

            if (!record.country.empty())
            {
                countryCounts[record.country]++;
            }

            if (!record.asn.empty())
            {
                networkCounts[record.asn + (record.asName.empty() ? "" : " " + record.asName)]++;
            }
        }

        summary.grey = summary.known - summary.reachable;

        /* Second pass: height buckets need networkHeight, which the first pass
           is what establishes. */
        uint64_t atTip = 0;
        uint64_t behind1to2 = 0;
        uint64_t behind3to10 = 0;
        uint64_t behind11to100 = 0;
        uint64_t behindOver100 = 0;
        uint64_t noHeight = 0;

        for (const auto &entry : m_nodes)
        {
            const NodeRecord &record = entry.second;

            if (record.reach != Reach::Open)
            {
                continue;
            }

            if (!record.haveHandshake || record.height == 0)
            {
                noHeight++;
                continue;
            }

            const uint32_t behind =
                summary.networkHeight > record.height ? summary.networkHeight - record.height : 0;

            if (behind == 0)
            {
                atTip++;
            }
            else if (behind <= 2)
            {
                behind1to2++;
            }
            else if (behind <= 10)
            {
                behind3to10++;
            }
            else if (behind <= 100)
            {
                behind11to100++;
            }
            else
            {
                behindOver100++;
            }
        }

        summary.heightBuckets = {
            {"at the tip", atTip},
            {"1-2 behind", behind1to2},
            {"3-10 behind", behind3to10},
            {"11-100 behind", behind11to100},
            {"over 100 behind", behindOver100},
            {"no height reported", noHeight},
        };

        for (const auto &entry : hashCounts)
        {
            summary.topHashes.push_back({entry.first, entry.second});
        }

        sortBucketsDescending(summary.topHashes);

        for (const auto &entry : versionCounts)
        {
            summary.versions.push_back({"v" + std::to_string(entry.first), entry.second});
        }

        for (const auto &entry : countryCounts)
        {
            summary.countries.push_back({entry.first, entry.second});
        }

        sortBucketsDescending(summary.countries);

        for (const auto &entry : networkCounts)
        {
            summary.networks.push_back({entry.first, entry.second});
        }

        sortBucketsDescending(summary.networks);

        if (!rtts.empty())
        {
            std::sort(rtts.begin(), rtts.end());
            summary.rttMedianMs = rtts[rtts.size() / 2];
            summary.rttP95Ms = rtts[static_cast<size_t>(rtts.size() * 95 / 100)];
        }

        /* Churn over the last day, from the per-node daily history rather than
           the sweep series: a node that answered yesterday and not today has
           left, whatever the sweep counts did in between. */
        const uint64_t nowDay = m_lastSweepFinished == 0 ? 0 : dayIndexOf(m_lastSweepFinished);

        if (nowDay > 0)
        {
            for (const auto &entry : m_nodes)
            {
                const auto &history = entry.second.history;

                const auto today = history.find(static_cast<uint32_t>(nowDay));
                const auto yesterday = history.find(static_cast<uint32_t>(nowDay - 1));

                const bool answeredToday = today != history.end() && today->second.answered > 0;
                const bool answeredYesterday = yesterday != history.end() && yesterday->second.answered > 0;

                if (answeredToday && !answeredYesterday)
                {
                    summary.joined24h++;
                }
                else if (!answeredToday && answeredYesterday)
                {
                    summary.left24h++;
                }
            }
        }

        return summary;
    }

    bool NodeStore::save(const std::string &dataDir, std::string &error) const
    {
        std::error_code ec;
        fs::create_directories(dataDir, ec);

        const fs::path target = fs::path(dataDir) / "nodes.ndjson";
        const fs::path temp = fs::path(dataDir) / "nodes.ndjson.tmp";

        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);

            if (!out)
            {
                error = "could not open " + temp.string();
                return false;
            }

            std::lock_guard<std::mutex> lock(m_mutex);

            nlohmann::json header;
            header["kind"] = "netmon-header";
            header["version"] = 1;
            header["sweepNumber"] = m_sweepNumber;
            header["lastSweepStarted"] = m_lastSweepStarted;
            header["lastSweepFinished"] = m_lastSweepFinished;
            header["lastSweepProbed"] = m_lastSweepProbed;
            header["lastSweepDurationMs"] = m_lastSweepDurationMs;

            nlohmann::json sweeps = nlohmann::json::array();

            for (const SweepPoint &point : m_sweeps)
            {
                sweeps.push_back({{"t", point.finishedAt}, {"p", point.probed}, {"r", point.reachable}});
            }

            header["sweeps"] = sweeps;

            out << header.dump() << "\n";

            for (const auto &entry : m_nodes)
            {
                const NodeRecord &record = entry.second;

                nlohmann::json j;
                j["address"] = record.address;
                j["port"] = record.port;
                j["isSeed"] = record.isSeed;
                j["reach"] = reachToString(record.reach);
                j["rttMs"] = record.rttMs;
                j["firstSeen"] = record.firstSeen;
                j["lastSeen"] = record.lastSeen;
                j["lastTried"] = record.lastTried;
                j["sweepsOffered"] = record.sweepsOffered;
                j["sweepsAnswered"] = record.sweepsAnswered;
                j["peersAdvertised"] = record.peersAdvertised;
                j["haveHandshake"] = record.haveHandshake;
                j["p2pVersion"] = record.p2pVersion;
                j["peerId"] = record.peerId;
                j["height"] = record.height;
                j["topId"] = Common::podToHex(record.topId);
                j["capabilityFlags"] = record.capabilityFlags;
                j["prunedHeight"] = record.prunedHeight;
                j["liteStartHeight"] = record.liteStartHeight;
                j["clockSkewSeconds"] = record.clockSkewSeconds;
                j["advertisedPort"] = record.advertisedPort;
                j["softwareVersion"] = record.softwareVersion;
                j["country"] = record.country;
                j["asn"] = record.asn;
                j["asName"] = record.asName;

                nlohmann::json history = nlohmann::json::object();

                for (const auto &day : record.history)
                {
                    history[std::to_string(day.first)] = {day.second.offered, day.second.answered};
                }

                j["history"] = history;

                out << j.dump() << "\n";
            }

            out.flush();

            if (!out)
            {
                error = "write failed on " + temp.string();
                return false;
            }
        }

        /* Rename over the target so a crash mid-write cannot leave a truncated
           table behind - the previous one stays until the new one is whole. */
        fs::rename(temp, target, ec);

        if (ec)
        {
            fs::remove(target, ec);
            fs::rename(temp, target, ec);
        }

        if (ec)
        {
            error = "could not replace " + target.string() + ": " + ec.message();
            return false;
        }

        return true;
    }

    bool NodeStore::load(const std::string &dataDir, std::string &error)
    {
        const fs::path target = fs::path(dataDir) / "nodes.ndjson";

        std::error_code ec;

        if (!fs::exists(target, ec))
        {
            return true; /* a first run is not a failure */
        }

        std::ifstream in(target, std::ios::binary);

        if (!in)
        {
            error = "could not open " + target.string();
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        m_nodes.clear();
        m_sweeps.clear();

        std::string line;
        size_t lineNumber = 0;

        while (std::getline(in, line))
        {
            lineNumber++;

            if (line.empty())
            {
                continue;
            }

            const auto j = nlohmann::json::parse(line, nullptr, false);

            if (j.is_discarded() || !j.is_object())
            {
                error = "line " + std::to_string(lineNumber) + " of " + target.string() + " is not JSON";
                return false;
            }

            if (j.value("kind", std::string()) == "netmon-header")
            {
                m_sweepNumber = j.value("sweepNumber", uint64_t {0});
                m_lastSweepStarted = j.value("lastSweepStarted", uint64_t {0});
                m_lastSweepFinished = j.value("lastSweepFinished", uint64_t {0});
                m_lastSweepProbed = j.value("lastSweepProbed", uint32_t {0});
                m_lastSweepDurationMs = j.value("lastSweepDurationMs", uint32_t {0});

                if (j.contains("sweeps") && j["sweeps"].is_array())
                {
                    for (const auto &entry : j["sweeps"])
                    {
                        SweepPoint point;
                        point.finishedAt = entry.value("t", uint64_t {0});
                        point.probed = entry.value("p", uint32_t {0});
                        point.reachable = entry.value("r", uint32_t {0});
                        m_sweeps.push_back(point);
                    }
                }

                continue;
            }

            NodeRecord record;
            record.address = j.value("address", std::string());
            record.port = j.value("port", uint16_t {0});

            if (record.address.empty() || record.port == 0)
            {
                continue;
            }

            record.isSeed = j.value("isSeed", false);
            record.reach = reachFromString(j.value("reach", std::string("unknown")));
            record.rttMs = j.value("rttMs", uint64_t {0});
            record.firstSeen = j.value("firstSeen", uint64_t {0});
            record.lastSeen = j.value("lastSeen", uint64_t {0});
            record.lastTried = j.value("lastTried", uint64_t {0});
            record.sweepsOffered = j.value("sweepsOffered", uint64_t {0});
            record.sweepsAnswered = j.value("sweepsAnswered", uint64_t {0});
            record.peersAdvertised = j.value("peersAdvertised", uint32_t {0});
            record.haveHandshake = j.value("haveHandshake", false);
            record.p2pVersion = j.value("p2pVersion", uint8_t {0});
            record.peerId = j.value("peerId", uint64_t {0});
            record.height = j.value("height", uint32_t {0});
            Common::podFromHex(j.value("topId", std::string()), record.topId);
            record.capabilityFlags = j.value("capabilityFlags", uint32_t {0});
            record.prunedHeight = j.value("prunedHeight", uint32_t {0});
            record.liteStartHeight = j.value("liteStartHeight", uint32_t {0});
            record.clockSkewSeconds = j.value("clockSkewSeconds", int64_t {0});
            record.advertisedPort = j.value("advertisedPort", uint32_t {0});
            record.softwareVersion = j.value("softwareVersion", std::string());
            record.country = j.value("country", std::string());
            record.asn = j.value("asn", std::string());
            record.asName = j.value("asName", std::string());

            if (j.contains("history") && j["history"].is_object())
            {
                for (auto it = j["history"].begin(); it != j["history"].end(); ++it)
                {
                    if (!it.value().is_array() || it.value().size() != 2)
                    {
                        continue;
                    }

                    DayStat stat;
                    stat.offered = it.value()[0].get<uint32_t>();
                    stat.answered = it.value()[1].get<uint32_t>();

                    record.history[static_cast<uint32_t>(std::stoul(it.key()))] = stat;
                }
            }

            pruneHistory(record);

            m_nodes.emplace(record.key(), std::move(record));
        }

        return true;
    }
} // namespace NetMon
