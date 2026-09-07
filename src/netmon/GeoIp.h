// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace NetMon
{
    struct Location
    {
        std::string country;

        std::string asn;

        std::string asName;
    };

    /* Range lookups over the DB-IP Lite CSVs. Neither file is bundled - they
       carry their own licence and their own monthly release - so both are
       optional and everything works without them. See NETMON.md.

       Loaded once at startup and then read-only, so lookups need no lock. */
    class GeoIp
    {
      public:
        /* Either path may be empty, which loads nothing for that dimension.
           Returns false only when a path was given and could not be read at
           all; a file with some unparseable lines loads what it can and
           reports the count through skippedLines. */
        bool load(
            const std::string &countryCsv,
            const std::string &asnCsv,
            std::string &error,
            uint64_t &loadedRanges,
            uint64_t &skippedLines);

        /* address is System::IpAddress::toString() form, so an IPv6 literal
           arrives bracketed. Unknown dimensions come back empty. */
        Location lookup(const std::string &address) const;

        bool haveCountry() const
        {
            return !m_country4.empty() || !m_country6.empty();
        }

        bool haveAsn() const
        {
            return !m_asn4.empty() || !m_asn6.empty();
        }

      private:
        using Bytes16 = std::array<uint8_t, 16>;

        struct Range4
        {
            uint32_t start = 0;

            uint32_t end = 0;

            uint32_t payload = 0; /* index into m_strings */
        };

        struct Range6
        {
            Bytes16 start {};

            Bytes16 end {};

            uint32_t payload = 0;
        };

        /* Interned so the ~500k ranges of a country file do not each carry
           their own two-character std::string. The cache is only alive for
           the duration of load(). */
        uint32_t intern(const std::string &value);

        std::unordered_map<std::string, uint32_t> m_internCache;

        bool loadFile(const std::string &path, bool isAsn, std::string &error, uint64_t &loaded, uint64_t &skipped);

        const std::string *find4(const std::vector<Range4> &ranges, uint32_t ip) const;

        const std::string *find6(const std::vector<Range6> &ranges, const Bytes16 &ip) const;

        std::vector<std::string> m_strings;

        std::vector<Range4> m_country4;

        std::vector<Range6> m_country6;

        std::vector<Range4> m_asn4;

        std::vector<Range6> m_asn6;

        /* ASN files carry a number and a name; the interned payload holds
           "AS13335\x1fCLOUDFLARENET" and this splits it on lookup. */
        static void splitAsn(const std::string &packed, std::string &asn, std::string &name);
    };
} // namespace NetMon
