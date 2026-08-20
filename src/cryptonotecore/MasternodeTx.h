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
        Attest = 7,
        UpdateEndpoint = 8
    };

    enum class MasternodeTxParseResult : uint8_t
    {
        NotFound = 0,
        Valid = 1,
        Invalid = 2
    };

    /* Wire layout of MN01 payloads (all integers little-endian):
     *
     *   header           = "MN01" | type(1) | masternodeId(32)                       (37 bytes)
     *
     *   Register (v3)    = header | payoutKey(32) | tokenId(32) | tokenExpiry(4)
     *                      | collateralAmount(8) | collateralGlobalIndex(4)
     *                      | collateralKeyImage(32) | collateralOutputKey(32)
     *                      | endpointCommitment(32) | signingKey(32) | payoutViewKey(32)
     *                      ‖ payoutSignature(64) | collateralProof(64)                  (405 bytes)
     *
     *   Heartbeat        = header | height(4) | healthy(1) ‖ signature(64)            (106 bytes)
     *   Attest           = header | height(4) | verifierKey(32) | healthy(1) ‖ sig(64) (138 bytes)
     *   Activate/Deactivate/Penalize/Revoke
     *                    = header | height(4) ‖ signature(64)                         (105 bytes)
     *   UpdateEndpoint   = header | height(4) | newEndpointCommitment(32) ‖ sig(64)   (137 bytes)
     *
     * `height` is the block height the payload was produced at (nextBlockHeight as seen by the
     * signer). Consensus only accepts payloads that are recent and strictly newer than the last
     * accepted payload of the same kind, which makes every signed payload single-use. */
    constexpr size_t MASTERNODE_PAYLOAD_HEADER_SIZE = 4 + 1 + sizeof(Crypto::Hash);
    constexpr size_t MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE =
        MASTERNODE_PAYLOAD_HEADER_SIZE + sizeof(Crypto::PublicKey) /* payoutKey */
        + sizeof(Crypto::Hash) /* tokenId */ + sizeof(uint32_t) /* expiry */
        + sizeof(uint64_t) /* collateralAmount */ + sizeof(uint32_t) /* globalIndex */
        + sizeof(Crypto::KeyImage) + sizeof(Crypto::PublicKey) /* collateralOutputKey */
        + sizeof(Crypto::Hash) /* endpointCommitment */ + sizeof(Crypto::PublicKey) /* signingKey */
        + sizeof(Crypto::PublicKey) /* payoutViewKey */;
    constexpr size_t MASTERNODE_REGISTER_PAYLOAD_SIZE =
        MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE + 2 * sizeof(Crypto::Signature);
    constexpr size_t MASTERNODE_HEARTBEAT_UNSIGNED_PAYLOAD_SIZE = MASTERNODE_PAYLOAD_HEADER_SIZE + sizeof(uint32_t) + 1;
    constexpr size_t MASTERNODE_HEARTBEAT_PAYLOAD_SIZE =
        MASTERNODE_HEARTBEAT_UNSIGNED_PAYLOAD_SIZE + sizeof(Crypto::Signature);
    constexpr size_t MASTERNODE_ATTEST_UNSIGNED_PAYLOAD_SIZE =
        MASTERNODE_PAYLOAD_HEADER_SIZE + sizeof(uint32_t) + sizeof(Crypto::PublicKey) + 1;
    constexpr size_t MASTERNODE_ATTEST_PAYLOAD_SIZE = MASTERNODE_ATTEST_UNSIGNED_PAYLOAD_SIZE + sizeof(Crypto::Signature);
    constexpr size_t MASTERNODE_ACTION_UNSIGNED_PAYLOAD_SIZE = MASTERNODE_PAYLOAD_HEADER_SIZE + sizeof(uint32_t);
    constexpr size_t MASTERNODE_ACTION_PAYLOAD_SIZE = MASTERNODE_ACTION_UNSIGNED_PAYLOAD_SIZE + sizeof(Crypto::Signature);
    constexpr size_t MASTERNODE_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE =
        MASTERNODE_PAYLOAD_HEADER_SIZE + sizeof(uint32_t) + sizeof(Crypto::Hash);
    constexpr size_t MASTERNODE_UPDATE_ENDPOINT_PAYLOAD_SIZE =
        MASTERNODE_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE + sizeof(Crypto::Signature);

    struct MasternodeTxPayload
    {
        MasternodeTxType type;
        Crypto::Hash masternodeId;
        /* Creation height carried by every non-Register payload. */
        bool hasHeight = false;
        uint32_t height = 0;
        bool healthy = true;
        bool hasPayoutKey = false;
        Crypto::PublicKey payoutKey = Crypto::PublicKey {{0}};
        bool hasPayoutViewKey = false;
        Crypto::PublicKey payoutViewKey = Crypto::PublicKey {{0}};
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
        bool hasNewEndpointCommitment = false;
        Crypto::Hash newEndpointCommitment = Crypto::Hash {{0}};
        bool hasSigningKey = false;
        Crypto::PublicKey signingKey = Crypto::PublicKey {{0}};
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

    /* Payload builders (unsigned portion). Shared by the wallet, the daemon signer and tests so that
     * the wire layout lives in exactly one place. Append the 64-byte signature to obtain the full
     * MN01 payload. */
    std::vector<uint8_t> buildMasternodePayloadHeader(MasternodeTxType type, const Crypto::Hash &masternodeId);

    std::vector<uint8_t>
        buildMasternodeActionUnsignedPayload(MasternodeTxType type, const Crypto::Hash &masternodeId, uint32_t height);

    std::vector<uint8_t>
        buildMasternodeHeartbeatUnsignedPayload(const Crypto::Hash &masternodeId, uint32_t height, bool healthy);

    std::vector<uint8_t> buildMasternodeAttestUnsignedPayload(
        const Crypto::Hash &masternodeId,
        uint32_t height,
        const Crypto::PublicKey &verifierKey,
        bool healthy);

    std::vector<uint8_t> buildMasternodeUpdateEndpointUnsignedPayload(
        const Crypto::Hash &masternodeId,
        uint32_t height,
        const Crypto::Hash &newEndpointCommitment);

    struct MasternodeRegisterFields
    {
        Crypto::Hash masternodeId;
        Crypto::PublicKey payoutKey;
        Crypto::PublicKey payoutViewKey;
        Crypto::Hash registrationTokenId;
        uint32_t registrationExpiresAtHeight = 0;
        uint64_t collateralAmount = 0;
        uint32_t collateralGlobalOutputIndex = 0;
        Crypto::KeyImage collateralKeyImage;
        Crypto::PublicKey collateralOutputKey;
        Crypto::Hash endpointCommitment;
        Crypto::PublicKey signingKey;
    };

    std::vector<uint8_t> buildMasternodeRegisterUnsignedPayload(const MasternodeRegisterFields &fields);

} // namespace CryptoNote
