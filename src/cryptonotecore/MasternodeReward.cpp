// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeReward.h"

#include <algorithm>
#include <crypto/crypto.h>
#include <vector>

namespace CryptoNote
{
    namespace MasternodeReward
    {
        Crypto::SecretKey deriveCoinbaseTxSecretKey(uint32_t height, const Crypto::Hash &previousBlockHash)
        {
            /* r = Hs("MNCB1" || height_LE4 || previousBlockHash) — unique per chain position, public,
             * and recomputable by every validator. */
            std::vector<uint8_t> preimage;
            preimage.reserve(5 + sizeof(uint32_t) + sizeof(Crypto::Hash));
            preimage.push_back('M');
            preimage.push_back('N');
            preimage.push_back('C');
            preimage.push_back('B');
            preimage.push_back('1');
            for (size_t i = 0; i < sizeof(uint32_t); ++i)
            {
                preimage.push_back(static_cast<uint8_t>((height >> (8 * i)) & 0xff));
            }
            preimage.insert(preimage.end(), previousBlockHash.data, previousBlockHash.data + sizeof(Crypto::Hash));

            Crypto::EllipticCurveScalar scalar;
            Crypto::hashToScalar(preimage.data(), preimage.size(), scalar);

            Crypto::SecretKey secret;
            std::copy(std::begin(scalar.data), std::end(scalar.data), std::begin(secret.data));
            return secret;
        }

        bool deriveRewardOutputKey(
            const Crypto::PublicKey &payoutSpendKey,
            const Crypto::PublicKey &payoutViewKey,
            const Crypto::SecretKey &txSecretKey,
            size_t outputIndex,
            Crypto::PublicKey &outputKey)
        {
            Crypto::KeyDerivation derivation;
            if (!Crypto::generate_key_derivation(payoutViewKey, txSecretKey, derivation))
            {
                return false;
            }

            return Crypto::derive_public_key(derivation, outputIndex, payoutSpendKey, outputKey);
        }
    } // namespace MasternodeReward
} // namespace CryptoNote
