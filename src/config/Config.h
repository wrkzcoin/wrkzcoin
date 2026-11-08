// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>

namespace Config
{
    class WalletConfig
    {
      public:
        /* Whether the wallet leaves coinbase (miner reward) transactions out
           of the scan. Off by default, so mining rewards are always found.
           Turning it on lets the daemon drop coinbase-only blocks, which syncs
           much faster, but rewards passed while it is on are only found again
           by a reset. */
        bool skipCoinbaseTransactions = false;

        /* The most blocks one sync request may ask a daemon for; the batch
           grows towards this while requests succeed. Zero means
           WalletConfig::maxBlocksPerSyncRequest. Raising it only helps against
           a daemon whose --rpc-max-block-count is raised too - one that
           refuses the size answers 400, and the wallet settles on half. */
        uint64_t syncMaxBlocks = 0;
    };

    class DaemonConfig
    {
      public:
    };

    class GlobalConfig
    {
      public:
    };

    /* Global config, exposed as `config`.
       Example: `if (Config::config.wallet.skipCoinbaseTransactions)` */
    class Config
    {
      public:
        Config() {};

        /* Configuration for wallets */
        WalletConfig wallet;

        /* Configuration for the daemon */
        DaemonConfig daemon;

        /* Configuration for throughout the codebase */
        GlobalConfig global;
    };

    extern Config config;
} // namespace Config
