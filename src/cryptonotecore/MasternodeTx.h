// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <crypto/crypto.h>
#include <cstdint>
#include <string>
#include <vector>

namespace CryptoNote
{
    enum class MasternodeTxType : uint8_t
    {
        Register = 1,
        Activate = 2,
        Deactivate = 3,
        Penalize = 4,
        Revoke = 5,
        Heartbeat = 6,
        Attest = 7
    };

    enum class MasternodeTxParseResult : uint8_t
    {
        NotFound = 0,
        Valid = 1,
        Invalid = 2
    };

    struct MasternodeTxPayload
    {
        MasternodeTxType type;
        Crypto::Hash masternodeId;
        bool healthy = true;
        bool hasPayoutKey = false;
        Crypto::PublicKey payoutKey = Crypto::PublicKey {{0}};
        bool hasRegistrationToken = false;
        Crypto::Hash registrationTokenId = Crypto::Hash {{0}};
        uint32_t registrationExpiresAtHeight = 0;
        bool hasCollateral = false;
        uint64_t collateralAmount = 0;
        uint32_t collateralGlobalOutputIndex = 0;
        Crypto::KeyImage collateralKeyImage = Crypto::KeyImage {{0}};
        Crypto::PublicKey collateralOutputKey = Crypto::PublicKey {{0}};
        bool hasEndpointCommitment = false;
        Crypto::Hash endpointCommitment = Crypto::Hash {{0}};
        bool hasVerifierKey = false;
        Crypto::PublicKey verifierKey = Crypto::PublicKey {{0}};
        bool hasCollateralSignature = false;
        Crypto::Signature collateralSignature = Crypto::Signature {{0}};
        bool hasSignature = false;
        Crypto::Signature signature = Crypto::Signature {{0}};
        std::vector<uint8_t> unsignedPayload;
    };

    MasternodeTxParseResult
        parseMasternodeTxPayload(const Transaction &transaction, MasternodeTxPayload &payload, std::string &error);

    bool getMasternodeTxSigningHash(const MasternodeTxPayload &payload, Crypto::Hash &signingHash);

} // namespace CryptoNote
