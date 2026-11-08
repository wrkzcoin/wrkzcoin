// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "GlobalIndexPlanTests.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <utilities/Utilities.h>
#include <utility>
#include <vector>

namespace GlobalIndexPlanTests
{
    namespace
    {
        /* The values the wallet uses: see GLOBAL_INDEXES_OBSCURITY and
           GLOBAL_INDEXES_MAX_MERGED_RANGE in walletbackend/Constants.h, which
           this test cannot include without the wallet backend. */
        const uint64_t WINDOW = 10;

        const uint64_t MAX_SPAN = 90;

        typedef std::vector<std::pair<uint64_t, uint64_t>> Ranges;

        std::string describe(const Ranges &ranges)
        {
            std::string result;

            for (const auto &[start, end] : ranges)
            {
                result += "[" + std::to_string(start) + "," + std::to_string(end) + ") ";
            }

            return result.empty() ? "(none)" : result;
        }

        void check(const std::string &what, const Ranges &expected, const Ranges &actual)
        {
            if (expected != actual)
            {
                std::cout << std::endl
                          << "Global index plan test FAILED: " << what << std::endl
                          << "  expected: " << describe(expected) << std::endl
                          << "  actual:   " << describe(actual) << std::endl
                          << "Terminating." << std::endl;

                exit(1);
            }
        }

        void fail(const std::string &what, const uint64_t seed)
        {
            std::cout << std::endl
                      << "Global index plan test FAILED (seed " << seed << "): " << what << std::endl
                      << "Terminating." << std::endl;

            exit(1);
        }
    } // namespace

    void runAll()
    {
        std::cout << std::endl << "Test Global Index Request Planning" << std::endl << std::endl;

        std::cout << "Windows deduplicated and joined:   ";

        {
            std::vector<uint64_t> heights = {1000, 1003, 1015, 1047};

            for (uint64_t height = 1100; height <= 1190; height += 10)
            {
                heights.push_back(height);
            }

            check(
                "a pool wallet's chunk",
                {{1000, 1020}, {1040, 1050}, {1100, 1190}, {1190, 1200}},
                Utilities::planGlobalIndexRanges(heights, WINDOW, MAX_SPAN));
        }

        std::cout << "PASSED" << std::endl;

        std::cout << "Order and repeats do not matter:   ";

        check(
            "unsorted, repeated heights",
            {{20, 40}},
            Utilities::planGlobalIndexRanges({39, 21, 20, 29, 30, 25}, WINDOW, MAX_SPAN));

        check("no heights", {}, Utilities::planGlobalIndexRanges({}, WINDOW, MAX_SPAN));

        check("height zero", {{0, 10}}, Utilities::planGlobalIndexRanges({0, 9}, WINDOW, MAX_SPAN));

        std::cout << "PASSED" << std::endl;

        std::cout << "Joined ranges stay within limits:  ";

        /* Against a naive model: every range lies within MAX_SPAN, the ranges
           are ascending and disjoint, and together they name exactly the
           heights of the windows asked for - nothing more. */
        for (uint64_t seed = 0; seed < 2000; seed++)
        {
            std::mt19937_64 rng(seed);

            std::vector<uint64_t> heights(rng() % 60);

            const uint64_t base = rng() % 100000;

            for (auto &height : heights)
            {
                height = base + rng() % 400;
            }

            std::set<uint64_t> expectedWindows;

            for (const uint64_t height : heights)
            {
                expectedWindows.insert(height - height % WINDOW);
            }

            std::set<uint64_t> coveredWindows;

            uint64_t previousEnd = 0;

            for (const auto &[start, end] : Utilities::planGlobalIndexRanges(heights, WINDOW, MAX_SPAN))
            {
                if (end <= start || end - start > MAX_SPAN || start % WINDOW != 0 || end % WINDOW != 0)
                {
                    fail("range [" + std::to_string(start) + "," + std::to_string(end) + ") out of shape", seed);
                }

                if (start < previousEnd)
                {
                    fail("ranges overlap or are out of order", seed);
                }

                previousEnd = end;

                for (uint64_t window = start; window < end; window += WINDOW)
                {
                    coveredWindows.insert(window);
                }
            }

            if (coveredWindows != expectedWindows)
            {
                fail("ranges do not cover exactly the windows asked for", seed);
            }
        }

        std::cout << "PASSED" << std::endl;
    }
} // namespace GlobalIndexPlanTests
