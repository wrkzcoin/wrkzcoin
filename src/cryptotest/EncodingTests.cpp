// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "EncodingTests.h"

#include "CryptoTypes.h"
#include "common/StringTools.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace EncodingTests
{
    namespace
    {
        void fail(const std::string &what, const std::string &expected, const std::string &actual)
        {
            std::cout << std::endl
                      << "Encoding test FAILED: " << what << std::endl
                      << "  expected: " << expected << std::endl
                      << "  actual:   " << actual << std::endl
                      << "Terminating." << std::endl;

            exit(1);
        }

        void check(const std::string &what, const std::string &expected, const std::string &actual)
        {
            if (expected != actual)
            {
                fail(what, expected, actual);
            }
        }

        void checkTrue(const std::string &what, const bool condition)
        {
            if (!condition)
            {
                fail(what, "true", "false");
            }
        }

        void checkFalse(const std::string &what, const bool condition)
        {
            if (condition)
            {
                fail(what, "false", "true");
            }
        }

        std::string encode(const std::string &plain)
        {
            return Common::toBase64(plain.data(), plain.size());
        }

        std::string decode(const std::string &encoded)
        {
            std::vector<uint8_t> out;

            if (!Common::fromBase64(encoded, out))
            {
                return "<rejected>";
            }

            return std::string(out.begin(), out.end());
        }
    } // namespace

    void runAll()
    {
        std::cout << "Base64 encoding tests: " << std::flush;

        /* RFC 4648 section 10. These cover every padding case. */
        check("RFC 4648 \"\"", "", encode(""));
        check("RFC 4648 \"f\"", "Zg==", encode("f"));
        check("RFC 4648 \"fo\"", "Zm8=", encode("fo"));
        check("RFC 4648 \"foo\"", "Zm9v", encode("foo"));
        check("RFC 4648 \"foob\"", "Zm9vYg==", encode("foob"));
        check("RFC 4648 \"fooba\"", "Zm9vYmE=", encode("fooba"));
        check("RFC 4648 \"foobar\"", "Zm9vYmFy", encode("foobar"));

        check("RFC 4648 decode \"\"", "", decode(""));
        check("RFC 4648 decode \"Zg==\"", "f", decode("Zg=="));
        check("RFC 4648 decode \"Zm8=\"", "fo", decode("Zm8="));
        check("RFC 4648 decode \"Zm9v\"", "foo", decode("Zm9v"));
        check("RFC 4648 decode \"Zm9vYg==\"", "foob", decode("Zm9vYg=="));
        check("RFC 4648 decode \"Zm9vYmE=\"", "fooba", decode("Zm9vYmE="));
        check("RFC 4648 decode \"Zm9vYmFy\"", "foobar", decode("Zm9vYmFy"));

        /* Both of the high alphabet characters, which a lookup table gets wrong
           more often than the letters do. */
        {
            const std::vector<uint8_t> bytes = {0xfb, 0xff, 0xbf};

            const std::string encoded = Common::toBase64(bytes);

            check("high alphabet + and /", "+/+/", encoded);

            std::vector<uint8_t> roundTripped;

            checkTrue("high alphabet decodes", Common::fromBase64(encoded, roundTripped));
            checkTrue("high alphabet round trips", roundTripped == bytes);
        }

        /* Every byte value, so a sign extension bug on the top half shows up. */
        {
            std::vector<uint8_t> all;

            for (size_t i = 0; i < 256; i++)
            {
                all.push_back(static_cast<uint8_t>(i));
            }

            std::vector<uint8_t> roundTripped;

            checkTrue("all byte values decode", Common::fromBase64(Common::toBase64(all), roundTripped));
            checkTrue("all byte values round trip", roundTripped == all);
        }

        /* The shape the sync path actually uses: a 32 byte value, and the
           lengths that let a reader tell hex from base64 without being told
           which one it is looking at. */
        {
            Crypto::Hash hash {};

            for (size_t i = 0; i < sizeof(hash.data); i++)
            {
                hash.data[i] = static_cast<uint8_t>(i * 7);
            }

            const std::string encoded = Common::podToBase64(hash);

            check("32 byte pod encodes to 44 characters", "44", std::to_string(encoded.size()));

            check("32 byte pod encodes to 64 hex characters", "64", std::to_string(Common::podToHex(hash).size()));

            Crypto::Hash decoded {};

            checkTrue("32 byte pod decodes", Common::podFromBase64(encoded, decoded));
            checkTrue("32 byte pod round trips", std::equal(std::begin(hash.data), std::end(hash.data), std::begin(decoded.data)));

            /* The hex form must not decode as base64 or the two would be
               ambiguous, which is the whole basis of telling them apart. */
            Crypto::Hash confused {};

            checkFalse("hex of a 32 byte pod is not valid base64 of one", Common::podFromBase64(Common::podToHex(hash), confused));
        }

        /* Malformed input has to be rejected, not silently truncated into a
           short but plausible looking key. */
        checkTrue("length not a multiple of four is rejected", decode("Zm9vY") == "<rejected>");
        checkTrue("stray character is rejected", decode("Zm9*") == "<rejected>");
        checkTrue("padding in the middle is rejected", decode("Zg==Zg==") == "<rejected>");
        checkTrue("padding in third position alone is rejected", decode("Zg=v") == "<rejected>");
        checkTrue("all padding is rejected", decode("====") == "<rejected>");

        /* A buffer too small for the decoded value must fail rather than
           overrun it. */
        {
            uint8_t small[2] = {0, 0};

            uint64_t size = 0;

            checkFalse("decoding into an undersized buffer fails", Common::fromBase64("Zm9vYmFy", small, sizeof(small), size));
        }

        std::cout << "PASSED" << std::endl;
    }
} // namespace EncodingTests
