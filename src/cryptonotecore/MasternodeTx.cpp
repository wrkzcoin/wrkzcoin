// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeTx.h"

#include <algorithm>
#include <crypto/hash.h>
#include <utilities/ParseExtra.h>

namespace CryptoNote
{
    namespace
    {
        constexpr uint8_t MN_MAGIC_0 = 'M';
        constexpr uint8_t MN_MAGIC_1 = 'N';
        constexpr uint8_t MN_MAGIC_2 = '0';
        constexpr uint8_t MN_MAGIC_3 = '1';
        constexpr size_t MN_BASE_PAYLOAD_SIZE = 4 + 1 + sizeof(Crypto::Hash);
        constexpr size_t MN_SIG_PAYLOAD_SIZE = sizeof(Crypto::Signature);
        // Unsigned portion of a Register payload (no signing key — legacy v1).
        constexpr size_t MN_REGISTER_UNSIGNED_PAYLOAD_SIZE_V1 =
            MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash) + sizeof(uint32_t)
            + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(Crypto::KeyImage) + sizeof(Crypto::PublicKey)
            + sizeof(Crypto::Hash);
        constexpr size_t MN_REGISTER_PAYLOAD_SIZE_V1 =
            MN_REGISTER_UNSIGNED_PAYLOAD_SIZE_V1 + MN_SIG_PAYLOAD_SIZE + MN_SIG_PAYLOAD_SIZE;

        // Unsigned portion of a Register payload including the dedicated signing key (v2).
        constexpr size_t MN_REGISTER_UNSIGNED_PAYLOAD_SIZE =
            MN_REGISTER_UNSIGNED_PAYLOAD_SIZE_V1 + sizeof(Crypto::PublicKey); // + signingKey
        constexpr size_t MN_REGISTER_PAYLOAD_SIZE = MN_REGISTER_UNSIGNED_PAYLOAD_SIZE + MN_SIG_PAYLOAD_SIZE + MN_SIG_PAYLOAD_SIZE;
        constexpr size_t MN_HEARTBEAT_PAYLOAD_SIZE = MN_BASE_PAYLOAD_SIZE + 1 + MN_SIG_PAYLOAD_SIZE;
        constexpr size_t MN_ATTEST_UNSIGNED_PAYLOAD_SIZE = MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + 1;
        constexpr size_t MN_ATTEST_PAYLOAD_SIZE = MN_ATTEST_UNSIGNED_PAYLOAD_SIZE + MN_SIG_PAYLOAD_SIZE;
        constexpr size_t MN_ACTION_PAYLOAD_SIZE = MN_BASE_PAYLOAD_SIZE + MN_SIG_PAYLOAD_SIZE;
        constexpr size_t MN_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE = MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::Hash);
        constexpr size_t MN_UPDATE_ENDPOINT_PAYLOAD_SIZE = MN_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE + MN_SIG_PAYLOAD_SIZE;

        uint32_t readUint32LE(const std::vector<uint8_t> &data, size_t offset)
        {
            return static_cast<uint32_t>(data[offset])
                   | (static_cast<uint32_t>(data[offset + 1]) << 8)
                   | (static_cast<uint32_t>(data[offset + 2]) << 16)
                   | (static_cast<uint32_t>(data[offset + 3]) << 24);
        }

        uint64_t readUint64LE(const std::vector<uint8_t> &data, size_t offset)
        {
            return static_cast<uint64_t>(data[offset])
                   | (static_cast<uint64_t>(data[offset + 1]) << 8)
                   | (static_cast<uint64_t>(data[offset + 2]) << 16)
                   | (static_cast<uint64_t>(data[offset + 3]) << 24)
                   | (static_cast<uint64_t>(data[offset + 4]) << 32)
                   | (static_cast<uint64_t>(data[offset + 5]) << 40)
                   | (static_cast<uint64_t>(data[offset + 6]) << 48)
                   | (static_cast<uint64_t>(data[offset + 7]) << 56);
        }
    }

    MasternodeTxParseResult
        parseMasternodeTxPayload(const Transaction &transaction, MasternodeTxPayload &payload, std::string &error)
    {
        error.clear();

        const auto data = Utilities::getExtraDataFromExtra(transaction.extra);
        if (data.empty())
        {
            return MasternodeTxParseResult::NotFound;
        }

        if (data.size() < 4)
        {
            return MasternodeTxParseResult::NotFound;
        }

        if (data[0] != MN_MAGIC_0 || data[1] != MN_MAGIC_1 || data[2] != MN_MAGIC_2 || data[3] != MN_MAGIC_3)
        {
            return MasternodeTxParseResult::NotFound;
        }

        if (data.size() < MN_BASE_PAYLOAD_SIZE)
        {
            error = "Masternode payload is truncated";
            return MasternodeTxParseResult::Invalid;
        }

        const auto type = static_cast<MasternodeTxType>(data[4]);
        switch (type)
        {
            case MasternodeTxType::Register:
            case MasternodeTxType::Activate:
            case MasternodeTxType::Deactivate:
            case MasternodeTxType::Penalize:
            case MasternodeTxType::Revoke:
            case MasternodeTxType::Heartbeat:
            case MasternodeTxType::Attest:
            case MasternodeTxType::UpdateEndpoint:
                break;
            default:
                error = "Unknown masternode payload type";
                return MasternodeTxParseResult::Invalid;
        }

        payload.type = type;
        std::copy_n(data.begin() + 5, sizeof(Crypto::Hash), payload.masternodeId.data);
        payload.healthy = true;
        payload.hasPayoutKey = false;
        payload.hasRegistrationToken = false;
        payload.registrationTokenId = Crypto::Hash {{0}};
        payload.registrationExpiresAtHeight = 0;
        payload.hasCollateral = false;
        payload.collateralAmount = 0;
        payload.collateralGlobalOutputIndex = 0;
        payload.collateralKeyImage = Crypto::KeyImage {{0}};
        payload.collateralOutputKey = Crypto::PublicKey {{0}};
        payload.hasEndpointCommitment = false;
        payload.endpointCommitment = Crypto::Hash {{0}};
        payload.hasNewEndpointCommitment = false;
        payload.newEndpointCommitment = Crypto::Hash {{0}};
        payload.hasSigningKey = false;
        payload.signingKey = Crypto::PublicKey {{0}};
        payload.hasVerifierKey = false;
        payload.verifierKey = Crypto::PublicKey {{0}};
        payload.hasCollateralSignature = false;
        payload.hasSignature = false;
        payload.unsignedPayload.clear();

        if (type == MasternodeTxType::Register)
        {
            // Accept both v1 (no signingKey) and v2 (with signingKey) payloads.
            const bool isV2 = (data.size() == MN_REGISTER_PAYLOAD_SIZE);
            const bool isV1 = (data.size() == MN_REGISTER_PAYLOAD_SIZE_V1);
            if (!isV1 && !isV2)
            {
                error = "Register payload has invalid size";
                return MasternodeTxParseResult::Invalid;
            }

            const size_t unsignedSize = isV2 ? MN_REGISTER_UNSIGNED_PAYLOAD_SIZE : MN_REGISTER_UNSIGNED_PAYLOAD_SIZE_V1;

            payload.hasPayoutKey = true;
            std::copy_n(data.begin() + MN_BASE_PAYLOAD_SIZE, sizeof(Crypto::PublicKey), payload.payoutKey.data);
            std::copy_n(
                data.begin() + MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey),
                sizeof(Crypto::Hash),
                payload.registrationTokenId.data);
            payload.registrationExpiresAtHeight = readUint32LE(
                data,
                MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash));
            payload.hasRegistrationToken = true;
            payload.collateralAmount = readUint64LE(
                data,
                MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash) + sizeof(uint32_t));
            payload.collateralGlobalOutputIndex = readUint32LE(
                data,
                MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash) + sizeof(uint32_t)
                    + sizeof(uint64_t));
            std::copy_n(
                data.begin() + MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash)
                    + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t),
                sizeof(Crypto::KeyImage),
                payload.collateralKeyImage.data);
            std::copy_n(
                data.begin() + MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash)
                    + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(Crypto::KeyImage),
                sizeof(Crypto::PublicKey),
                payload.collateralOutputKey.data);
            const size_t endpointOffset =
                MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey) + sizeof(Crypto::Hash)
                + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(Crypto::KeyImage)
                + sizeof(Crypto::PublicKey);
            std::copy_n(data.begin() + endpointOffset, sizeof(Crypto::Hash), payload.endpointCommitment.data);
            payload.hasEndpointCommitment = true;
            payload.hasCollateral = true;

            if (isV2)
            {
                // signingKey follows endpointCommitment in the unsigned portion
                std::copy_n(
                    data.begin() + endpointOffset + sizeof(Crypto::Hash),
                    sizeof(Crypto::PublicKey),
                    payload.signingKey.data);
                payload.hasSigningKey = true;
            }

            std::copy_n(data.begin() + unsignedSize, sizeof(Crypto::Signature), payload.signature.data);
            payload.hasSignature = true;
            std::copy_n(
                data.begin() + unsignedSize + sizeof(Crypto::Signature),
                sizeof(Crypto::Signature),
                payload.collateralSignature.data);
            payload.hasCollateralSignature = true;
            payload.unsignedPayload.assign(data.begin(), data.begin() + unsignedSize);
        }
        else if (type == MasternodeTxType::Heartbeat)
        {
            if (data.size() != MN_HEARTBEAT_PAYLOAD_SIZE)
            {
                error = "Heartbeat payload has invalid size";
                return MasternodeTxParseResult::Invalid;
            }

            if (data[MN_BASE_PAYLOAD_SIZE] > 1)
            {
                error = "Heartbeat health flag must be 0 or 1";
                return MasternodeTxParseResult::Invalid;
            }

            payload.healthy = data[MN_BASE_PAYLOAD_SIZE] == 1;
            std::copy_n(
                data.begin() + MN_BASE_PAYLOAD_SIZE + 1,
                sizeof(Crypto::Signature),
                payload.signature.data);
            payload.hasSignature = true;
            payload.unsignedPayload.assign(data.begin(), data.begin() + MN_BASE_PAYLOAD_SIZE + 1);
        }
        else if (type == MasternodeTxType::Attest)
        {
            if (data.size() != MN_ATTEST_PAYLOAD_SIZE)
            {
                error = "Attest payload has invalid size";
                return MasternodeTxParseResult::Invalid;
            }

            std::copy_n(data.begin() + MN_BASE_PAYLOAD_SIZE, sizeof(Crypto::PublicKey), payload.verifierKey.data);
            payload.hasVerifierKey = true;

            const size_t healthyOffset = MN_BASE_PAYLOAD_SIZE + sizeof(Crypto::PublicKey);
            if (data[healthyOffset] > 1)
            {
                error = "Attest health flag must be 0 or 1";
                return MasternodeTxParseResult::Invalid;
            }
            payload.healthy = data[healthyOffset] == 1;

            std::copy_n(
                data.begin() + MN_ATTEST_UNSIGNED_PAYLOAD_SIZE,
                sizeof(Crypto::Signature),
                payload.signature.data);
            payload.hasSignature = true;
            payload.unsignedPayload.assign(data.begin(), data.begin() + MN_ATTEST_UNSIGNED_PAYLOAD_SIZE);
        }
        else if (type == MasternodeTxType::UpdateEndpoint)
        {
            if (data.size() != MN_UPDATE_ENDPOINT_PAYLOAD_SIZE)
            {
                error = "UpdateEndpoint payload has invalid size";
                return MasternodeTxParseResult::Invalid;
            }

            std::copy_n(
                data.begin() + MN_BASE_PAYLOAD_SIZE,
                sizeof(Crypto::Hash),
                payload.newEndpointCommitment.data);
            payload.hasNewEndpointCommitment = true;
            std::copy_n(
                data.begin() + MN_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE,
                sizeof(Crypto::Signature),
                payload.signature.data);
            payload.hasSignature = true;
            payload.unsignedPayload.assign(data.begin(), data.begin() + MN_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE);
        }
        else if (data.size() != MN_ACTION_PAYLOAD_SIZE)
        {
            error = "Masternode payload has invalid size";
            return MasternodeTxParseResult::Invalid;
        }
        else
        {
            std::copy_n(data.begin() + MN_BASE_PAYLOAD_SIZE, sizeof(Crypto::Signature), payload.signature.data);
            payload.hasSignature = true;
            payload.unsignedPayload.assign(data.begin(), data.begin() + MN_BASE_PAYLOAD_SIZE);
        }

        return MasternodeTxParseResult::Valid;
    }

    bool getMasternodeTxSigningHash(const MasternodeTxPayload &payload, Crypto::Hash &signingHash)
    {
        if (!payload.hasSignature || payload.unsignedPayload.empty())
        {
            return false;
        }

        signingHash = Crypto::cn_fast_hash(payload.unsignedPayload.data(), payload.unsignedPayload.size());
        return true;
    }

} // namespace CryptoNote
