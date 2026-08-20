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

        void appendUint32LE(std::vector<uint8_t> &out, uint32_t value)
        {
            for (size_t i = 0; i < sizeof(uint32_t); ++i)
            {
                out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
            }
        }

        void appendUint64LE(std::vector<uint8_t> &out, uint64_t value)
        {
            for (size_t i = 0; i < sizeof(uint64_t); ++i)
            {
                out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
            }
        }

        template<typename Pod> void appendPod(std::vector<uint8_t> &out, const Pod &pod)
        {
            out.insert(out.end(), pod.data, pod.data + sizeof(pod.data));
        }

        template<typename Pod> void readPod(const std::vector<uint8_t> &data, size_t offset, Pod &pod)
        {
            std::copy_n(data.begin() + offset, sizeof(pod.data), pod.data);
        }
    }

    std::vector<uint8_t> buildMasternodePayloadHeader(MasternodeTxType type, const Crypto::Hash &masternodeId)
    {
        std::vector<uint8_t> out;
        out.reserve(MASTERNODE_PAYLOAD_HEADER_SIZE);
        out.push_back(MN_MAGIC_0);
        out.push_back(MN_MAGIC_1);
        out.push_back(MN_MAGIC_2);
        out.push_back(MN_MAGIC_3);
        out.push_back(static_cast<uint8_t>(type));
        appendPod(out, masternodeId);
        return out;
    }

    std::vector<uint8_t>
        buildMasternodeActionUnsignedPayload(MasternodeTxType type, const Crypto::Hash &masternodeId, uint32_t height)
    {
        auto out = buildMasternodePayloadHeader(type, masternodeId);
        appendUint32LE(out, height);
        return out;
    }

    std::vector<uint8_t>
        buildMasternodeHeartbeatUnsignedPayload(const Crypto::Hash &masternodeId, uint32_t height, bool healthy)
    {
        auto out = buildMasternodeActionUnsignedPayload(MasternodeTxType::Heartbeat, masternodeId, height);
        out.push_back(healthy ? 1 : 0);
        return out;
    }

    std::vector<uint8_t> buildMasternodeAttestUnsignedPayload(
        const Crypto::Hash &masternodeId,
        uint32_t height,
        const Crypto::PublicKey &verifierKey,
        bool healthy)
    {
        auto out = buildMasternodeActionUnsignedPayload(MasternodeTxType::Attest, masternodeId, height);
        appendPod(out, verifierKey);
        out.push_back(healthy ? 1 : 0);
        return out;
    }

    std::vector<uint8_t> buildMasternodeUpdateEndpointUnsignedPayload(
        const Crypto::Hash &masternodeId,
        uint32_t height,
        const Crypto::Hash &newEndpointCommitment)
    {
        auto out = buildMasternodeActionUnsignedPayload(MasternodeTxType::UpdateEndpoint, masternodeId, height);
        appendPod(out, newEndpointCommitment);
        return out;
    }

    std::vector<uint8_t> buildMasternodeRegisterUnsignedPayload(const MasternodeRegisterFields &fields)
    {
        auto out = buildMasternodePayloadHeader(MasternodeTxType::Register, fields.masternodeId);
        out.reserve(MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE);
        appendPod(out, fields.payoutKey);
        appendPod(out, fields.registrationTokenId);
        appendUint32LE(out, fields.registrationExpiresAtHeight);
        appendUint64LE(out, fields.collateralAmount);
        appendUint32LE(out, fields.collateralGlobalOutputIndex);
        appendPod(out, fields.collateralKeyImage);
        appendPod(out, fields.collateralOutputKey);
        appendPod(out, fields.endpointCommitment);
        appendPod(out, fields.signingKey);
        appendPod(out, fields.payoutViewKey);
        return out;
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

        if (data.size() < MASTERNODE_PAYLOAD_HEADER_SIZE)
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

        payload = MasternodeTxPayload {};
        payload.type = type;
        readPod(data, 5, payload.masternodeId);

        if (type == MasternodeTxType::Register)
        {
            if (data.size() != MASTERNODE_REGISTER_PAYLOAD_SIZE)
            {
                error = "Register payload has invalid size";
                return MasternodeTxParseResult::Invalid;
            }

            size_t offset = MASTERNODE_PAYLOAD_HEADER_SIZE;
            readPod(data, offset, payload.payoutKey);
            payload.hasPayoutKey = true;
            offset += sizeof(Crypto::PublicKey);

            readPod(data, offset, payload.registrationTokenId);
            offset += sizeof(Crypto::Hash);
            payload.registrationExpiresAtHeight = readUint32LE(data, offset);
            offset += sizeof(uint32_t);
            payload.hasRegistrationToken = true;

            payload.collateralAmount = readUint64LE(data, offset);
            offset += sizeof(uint64_t);
            payload.collateralGlobalOutputIndex = readUint32LE(data, offset);
            offset += sizeof(uint32_t);
            readPod(data, offset, payload.collateralKeyImage);
            offset += sizeof(Crypto::KeyImage);
            readPod(data, offset, payload.collateralOutputKey);
            offset += sizeof(Crypto::PublicKey);
            payload.hasCollateral = true;

            readPod(data, offset, payload.endpointCommitment);
            offset += sizeof(Crypto::Hash);
            payload.hasEndpointCommitment = true;

            readPod(data, offset, payload.signingKey);
            offset += sizeof(Crypto::PublicKey);
            payload.hasSigningKey = true;

            readPod(data, offset, payload.payoutViewKey);
            offset += sizeof(Crypto::PublicKey);
            payload.hasPayoutViewKey = true;

            if (offset != MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE)
            {
                error = "Register payload layout mismatch";
                return MasternodeTxParseResult::Invalid;
            }

            readPod(data, offset, payload.signature);
            payload.hasSignature = true;
            offset += sizeof(Crypto::Signature);
            readPod(data, offset, payload.collateralSignature);
            payload.hasCollateralSignature = true;

            payload.unsignedPayload.assign(data.begin(), data.begin() + MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE);
            return MasternodeTxParseResult::Valid;
        }

        /* Every other payload type carries the creation height right after the header. */
        size_t unsignedSize = 0;
        size_t expectedSize = 0;
        switch (type)
        {
            case MasternodeTxType::Heartbeat:
                unsignedSize = MASTERNODE_HEARTBEAT_UNSIGNED_PAYLOAD_SIZE;
                expectedSize = MASTERNODE_HEARTBEAT_PAYLOAD_SIZE;
                break;
            case MasternodeTxType::Attest:
                unsignedSize = MASTERNODE_ATTEST_UNSIGNED_PAYLOAD_SIZE;
                expectedSize = MASTERNODE_ATTEST_PAYLOAD_SIZE;
                break;
            case MasternodeTxType::UpdateEndpoint:
                unsignedSize = MASTERNODE_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE;
                expectedSize = MASTERNODE_UPDATE_ENDPOINT_PAYLOAD_SIZE;
                break;
            default:
                unsignedSize = MASTERNODE_ACTION_UNSIGNED_PAYLOAD_SIZE;
                expectedSize = MASTERNODE_ACTION_PAYLOAD_SIZE;
                break;
        }

        if (data.size() != expectedSize)
        {
            error = "Masternode payload has invalid size";
            return MasternodeTxParseResult::Invalid;
        }

        size_t offset = MASTERNODE_PAYLOAD_HEADER_SIZE;
        payload.height = readUint32LE(data, offset);
        payload.hasHeight = true;
        offset += sizeof(uint32_t);

        if (type == MasternodeTxType::Heartbeat)
        {
            if (data[offset] > 1)
            {
                error = "Heartbeat health flag must be 0 or 1";
                return MasternodeTxParseResult::Invalid;
            }
            payload.healthy = data[offset] == 1;
            offset += 1;
        }
        else if (type == MasternodeTxType::Attest)
        {
            readPod(data, offset, payload.verifierKey);
            payload.hasVerifierKey = true;
            offset += sizeof(Crypto::PublicKey);

            if (data[offset] > 1)
            {
                error = "Attest health flag must be 0 or 1";
                return MasternodeTxParseResult::Invalid;
            }
            payload.healthy = data[offset] == 1;
            offset += 1;
        }
        else if (type == MasternodeTxType::UpdateEndpoint)
        {
            readPod(data, offset, payload.newEndpointCommitment);
            payload.hasNewEndpointCommitment = true;
            offset += sizeof(Crypto::Hash);
        }

        if (offset != unsignedSize)
        {
            error = "Masternode payload layout mismatch";
            return MasternodeTxParseResult::Invalid;
        }

        readPod(data, unsignedSize, payload.signature);
        payload.hasSignature = true;
        payload.unsignedPayload.assign(data.begin(), data.begin() + unsignedSize);

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
