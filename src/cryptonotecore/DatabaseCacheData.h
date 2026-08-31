// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cryptonotecore/BlockchainCache.h>
#include <map>

namespace CryptoNote
{
    struct KeyOutputInfo
    {
        Crypto::PublicKey publicKey;

        Crypto::Hash transactionHash;

        uint64_t unlockTime;

        uint16_t outputIndex;

        /* The block this output was created in. Decoy maturity and the "how many
           outputs of this amount existed at height X" search both need it, and
           both used to reach it through a parallel table under the "b" prefix
           holding a PackedOutIndex for the same (amount, globalIndex) key. Those
           records were 1:1 with these, so the field moved here and the table
           halved: one lookup instead of two on the decoy path, and 78 million
           fewer records. See LITENODE.md. */
        uint32_t blockIndex;

        void serialize(CryptoNote::ISerializer &s);
    };

    // inherit here to avoid breaking IBlockchainCache interface
    struct ExtendedTransactionInfo : CachedTransactionInfo
    {
        // CachedTransactionInfo tx;
        std::map<IBlockchainCache::Amount, std::vector<IBlockchainCache::GlobalOutputIndex>>
            amountToKeyIndexes; // global key output indexes spawned in this transaction
        void serialize(ISerializer &s);
    };

} // namespace CryptoNote
