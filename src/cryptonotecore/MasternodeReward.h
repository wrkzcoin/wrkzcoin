// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <cstddef>
#include <cstdint>

namespace CryptoNote
{
    /* Masternode reward output derivation, shared by consensus (Core) and tests.
     *
     * A block that pays a masternode reward uses a deterministic coinbase transaction secret key
     * so that every node can recompute the winner's one-time output key:
     *
     *   r = Hs("MNCB1" || height_LE4 || previousBlockHash)
     *   R = r*G                       (coinbase tx public key)
     *   P = Hs(r*A || index)*G + B    (A = payout view key, B = payout spend key)
     *
     * The operator's wallet detects and spends P exactly like any other output (a*R == r*A). */
    namespace MasternodeReward
    {
        Crypto::SecretKey deriveCoinbaseTxSecretKey(uint32_t height, const Crypto::Hash &previousBlockHash);

        bool deriveRewardOutputKey(
            const Crypto::PublicKey &payoutSpendKey,
            const Crypto::PublicKey &payoutViewKey,
            const Crypto::SecretKey &txSecretKey,
            size_t outputIndex,
            Crypto::PublicKey &outputKey);
    } // namespace MasternodeReward
} // namespace CryptoNote
