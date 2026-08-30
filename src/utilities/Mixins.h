// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>

namespace Utilities
{
    /* Returns {minMixin, maxMixin, defaultMixin} */
    std::tuple<uint64_t, uint64_t, uint64_t> getMixinAllowableRange(const uint64_t height);

    /* Given the mixin we just tried and what the chain turned out to support,
       the next mixin worth trying, or nothing when there is nothing lower left
       to try. Lives here rather than in either wallet stack so both degrade the
       same way - walletbackend and WalletGreen build transactions by different
       routes, and a ring size that one will settle for should be one the other
       settles for too. */
    std::optional<uint64_t> nextFallbackMixin(
        const uint64_t triedMixin,
        const uint64_t achievableMixin,
        const uint64_t minMixin);
} // namespace Utilities
