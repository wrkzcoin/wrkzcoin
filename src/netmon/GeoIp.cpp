// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "GeoIp.h"

#include <algorithm>
#include <fstream>
#include <system/IpAddress.h>
#include <unordered_map>

namespace NetMon
{
    namespace
    {
        constexpr char ASN_SEPARATOR = '\x1f';

        /* DB-IP publishes its Lite CSVs both quoted and unquoted depending on
           the release, and the ASN name field can itself contain a comma, so
           this is a real (if small) CSV split rather than a find(',') loop. */
        std::vector<std::string> splitCsv(const std::string &line)
        {
            std::vector<std::string> fields;
            std::string current;
            bool inQuotes = false;

            for (size_t i = 0; i < line.size(); i++)
            {
                const char c = line[i];

                if (c == '"')
                {
                    if (inQuotes && i + 1 < line.size() && line[i + 1] == '"')
                    {
                        current.push_back('"');
                        i++;
                    }
                    else
                    {
                        inQuotes = !inQuotes;
                    }

                    continue;
                }

                if (c == ',' && !inQuotes)
                {
                    fields.push_back(current);
                    current.clear();
                    continue;
                }

                if (c == '\r')
                {
                    continue;
                }

                current.push_back(c);
            }

            fields.push_back(current);

            return fields;
        }

        std::string trim(const std::string &value)
        {
            size_t begin = 0;
            size_t end = value.size();

            while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
            {
                begin++;
            }

            while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
            {
                end--;
            }

            return value.substr(begin, end - begin);
        }

        bool parseAddress(const std::string &text, bool &isV6, uint32_t &v4, std::array<uint8_t, 16> &v6)
        {
            try
            {
                const System::IpAddress parsed(text);

                if (parsed.isV4())
                {
                    isV6 = false;
                    v4 = parsed.toV4();
                    return true;
                }

                isV6 = true;
                std::copy(parsed.getBytes(), parsed.getBytes() + 16, v6.begin());

                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    } // namespace

    uint32_t GeoIp::intern(const std::string &value)
    {
        const auto it = m_internCache.find(value);

        if (it != m_internCache.end())
        {
            return it->second;
        }

        const uint32_t index = static_cast<uint32_t>(m_strings.size());
        m_strings.push_back(value);
        m_internCache.emplace(value, index);

        return index;
    }

    void GeoIp::splitAsn(const std::string &packed, std::string &asn, std::string &name)
    {
        const size_t separator = packed.find(ASN_SEPARATOR);

        if (separator == std::string::npos)
        {
            asn = packed;
            name.clear();
            return;
        }

        asn = packed.substr(0, separator);
        name = packed.substr(separator + 1);
    }

    bool GeoIp::loadFile(
        const std::string &path,
        const bool isAsn,
        std::string &error,
        uint64_t &loaded,
        uint64_t &skipped)
    {
        std::ifstream in(path, std::ios::binary);

        if (!in)
        {
            error = "could not open " + path;
            return false;
        }

        std::vector<Range4> &ranges4 = isAsn ? m_asn4 : m_country4;
        std::vector<Range6> &ranges6 = isAsn ? m_asn6 : m_country6;

        std::string line;

        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            const std::vector<std::string> fields = splitCsv(line);

            /* Country lite is start,end,cc. ASN lite is start,end,number,name
               - and some releases add a trailing empty field, so this is a
               minimum rather than an exact count. */
            if (fields.size() < 3)
            {
                skipped++;
                continue;
            }

            bool startIsV6 = false;
            bool endIsV6 = false;
            uint32_t start4 = 0;
            uint32_t end4 = 0;
            Range6 range6;

            if (!parseAddress(trim(fields[0]), startIsV6, start4, range6.start)
                || !parseAddress(trim(fields[1]), endIsV6, end4, range6.end) || startIsV6 != endIsV6)
            {
                skipped++;
                continue;
            }

            std::string payload;

            if (isAsn)
            {
                const std::string number = trim(fields[2]);
                const std::string name = fields.size() > 3 ? trim(fields[3]) : std::string();

                if (number.empty() || number == "0")
                {
                    skipped++;
                    continue;
                }

                payload = "AS" + number;
                payload.push_back(ASN_SEPARATOR);
                payload += name;
            }
            else
            {
                payload = trim(fields[2]);

                /* DB-IP writes "ZZ" for an unallocated range; carrying it
                   through would show as a country on the dashboard. */
                if (payload.empty() || payload == "ZZ")
                {
                    skipped++;
                    continue;
                }
            }

            const uint32_t interned = intern(payload);

            if (startIsV6)
            {
                range6.payload = interned;
                ranges6.push_back(range6);
            }
            else
            {
                Range4 range;
                range.start = start4;
                range.end = end4;
                range.payload = interned;
                ranges4.push_back(range);
            }

            loaded++;
        }

        std::sort(ranges4.begin(), ranges4.end(), [](const Range4 &a, const Range4 &b) {
            return a.start < b.start;
        });

        std::sort(ranges6.begin(), ranges6.end(), [](const Range6 &a, const Range6 &b) {
            return a.start < b.start;
        });

        return true;
    }

    bool GeoIp::load(
        const std::string &countryCsv,
        const std::string &asnCsv,
        std::string &error,
        uint64_t &loadedRanges,
        uint64_t &skippedLines)
    {
        loadedRanges = 0;
        skippedLines = 0;

        if (!countryCsv.empty() && !loadFile(countryCsv, false, error, loadedRanges, skippedLines))
        {
            return false;
        }

        if (!asnCsv.empty() && !loadFile(asnCsv, true, error, loadedRanges, skippedLines))
        {
            return false;
        }

        /* The ranges hold indexes into m_strings from here on, so the lookup
           table the load needed is dead weight. On a country file it is the
           larger of the two allocations. */
        m_internCache.clear();

        return true;
    }

    const std::string *GeoIp::find4(const std::vector<Range4> &ranges, const uint32_t ip) const
    {
        /* First range whose start is above ip, then step back one: ranges are
           disjoint in both files, so that is the only candidate. */
        const auto it = std::upper_bound(ranges.begin(), ranges.end(), ip, [](const uint32_t value, const Range4 &r) {
            return value < r.start;
        });

        if (it == ranges.begin())
        {
            return nullptr;
        }

        const Range4 &range = *(it - 1);

        if (ip < range.start || ip > range.end)
        {
            return nullptr;
        }

        return &m_strings[range.payload];
    }

    const std::string *GeoIp::find6(const std::vector<Range6> &ranges, const Bytes16 &ip) const
    {
        const auto it =
            std::upper_bound(ranges.begin(), ranges.end(), ip, [](const Bytes16 &value, const Range6 &r) {
                return value < r.start;
            });

        if (it == ranges.begin())
        {
            return nullptr;
        }

        const Range6 &range = *(it - 1);

        if (ip < range.start || ip > range.end)
        {
            return nullptr;
        }

        return &m_strings[range.payload];
    }

    Location GeoIp::lookup(const std::string &address) const
    {
        Location location;

        bool isV6 = false;
        uint32_t v4 = 0;
        Bytes16 v6 {};

        if (!parseAddress(address, isV6, v4, v6))
        {
            return location;
        }

        const std::string *country = isV6 ? find6(m_country6, v6) : find4(m_country4, v4);

        if (country != nullptr)
        {
            location.country = *country;
        }

        const std::string *asn = isV6 ? find6(m_asn6, v6) : find4(m_asn4, v4);

        if (asn != nullptr)
        {
            splitAsn(*asn, location.asn, location.asName);
        }

        return location;
    }
} // namespace NetMon
