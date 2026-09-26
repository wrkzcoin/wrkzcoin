// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IDataBase.h"

#include <string>

namespace CryptoNote
{
    /* Which network a database's chain belongs to. A simnet's blocks carry no
       proof of work, so its chain must never be served as mainnet's, nor the
       reverse.

       The record is written the first time a new database is opened as a
       simnet, and absent from every mainnet database - including every one
       built before simnets existed - so nothing about an existing node changes.
       From then on the choice is permanent for that database in both
       directions:

       | recorded | asked for | outcome                                    |
       | -------- | --------- | ------------------------------------------ |
       | none     | mainnet   | opened: no mainnet database has a record   |
       | none     | simnet    | a new database adopts it; one with a chain |
       |          |           | is refused, since that chain is mainnet's  |
       | simnet   | simnet    | opened                                     |
       | simnet   | mainnet   | refused                                    |

       Call it after the scheme version check and before the core loads, which
       is when the genesis block is written. Returns an empty string when the
       database may be opened, and otherwise the reason it may not. */
    std::string settleNetworkProfile(IDataBase &database, const bool simnet);
} // namespace CryptoNote
