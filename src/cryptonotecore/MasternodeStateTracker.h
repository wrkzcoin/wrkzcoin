// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include "json.hpp"
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

namespace CryptoNote
{
    class MasternodeStateTracker
    {
      public:
        enum class Status : uint8_t
        {
            Registered = 0,
            Active = 1,
            Inactive = 2,
            Penalized = 3,
            Revoked = 4
        };

        struct RewardDistribution
        {
            bool hasMasternodeWinner = false;
            std::optional<Crypto::Hash> masternodeWinner;
            uint64_t masternodeReward = 0;
            uint64_t powReward = 0;
        };

        struct Snapshot
        {
            Crypto::Hash masternodeId;
            Status status = Status::Registered;
            bool bonded = false;
            uint64_t bondAmount = 0;
            uint64_t collateralAmount = 0;
            uint32_t collateralGlobalOutputIndex = 0;
            bool hasCollateral = false;
            bool hasEndpointCommitment = false;
            Crypto::Hash endpointCommitment = Crypto::Hash {{0}};
            uint64_t healthPercent = 0;
            bool spendLocked = false;
            uint32_t lastPaidHeight = 0;
            uint64_t rewardInFairnessWindow = 0;
        };

        void registerMasternode(
            const Crypto::Hash &masternodeId,
            const Crypto::PublicKey &payoutKey,
            bool bonded,
            uint64_t bondAmount,
            const Crypto::Hash &registrationTokenId,
            uint32_t registrationExpiresAtHeight,
            uint64_t collateralAmount,
            uint32_t collateralGlobalOutputIndex,
            const Crypto::KeyImage &collateralKeyImage,
            const Crypto::PublicKey &collateralOutputKey,
            const Crypto::Hash &endpointCommitment);

        void activateMasternode(const Crypto::Hash &masternodeId);

        void deactivateMasternode(const Crypto::Hash &masternodeId, uint32_t height);

        void penalizeMasternode(const Crypto::Hash &masternodeId, uint32_t height);

        void revokeMasternode(const Crypto::Hash &masternodeId, uint32_t height);

        Status getStatus(const Crypto::Hash &masternodeId) const;

        void recordHealthSample(const Crypto::Hash &masternodeId, uint32_t height, bool healthy);

        void recordAttestationSample(
            const Crypto::Hash &masternodeId,
            uint32_t height,
            const Crypto::PublicKey &verifierKey,
            bool healthy);

        uint64_t getHealthPercent(const Crypto::Hash &masternodeId, uint32_t currentHeight) const;

        bool meetsHealthThreshold(const Crypto::Hash &masternodeId, uint32_t currentHeight) const;

        bool meetsAttestationThreshold(const Crypto::Hash &masternodeId, uint32_t currentHeight) const;

        void markDeactivated(const Crypto::Hash &masternodeId, uint32_t height);

        void clearDeactivation(const Crypto::Hash &masternodeId);

        bool isSpendLocked(const Crypto::Hash &masternodeId, uint32_t currentHeight) const;

        void recordReward(const Crypto::Hash &masternodeId, uint32_t height, uint64_t amount);

        uint64_t getRewardAmountInFairnessWindow(const Crypto::Hash &masternodeId, uint32_t currentHeight) const;

        std::vector<Crypto::Hash>
            filterRewardEligible(const std::vector<Crypto::Hash> &candidateIds, uint32_t currentHeight) const;

        std::optional<Crypto::Hash>
            selectFairRewardWinner(const std::vector<Crypto::Hash> &candidateIds, uint32_t currentHeight) const;

        RewardDistribution calculateRewardDistribution(
            uint64_t totalReward,
            uint32_t currentHeight,
            const std::vector<Crypto::Hash> &candidateIds,
            uint64_t masternodePercent) const;

        std::vector<Crypto::Hash> getTrackedMasternodeIds() const;

        void clear();

        std::vector<Snapshot> getSnapshots(uint32_t currentHeight, size_t offset, size_t limit) const;

        bool hasMasternode(const Crypto::Hash &masternodeId) const;

        bool getPayoutKey(const Crypto::Hash &masternodeId, Crypto::PublicKey &payoutKey) const;

        bool hasUsedRegistrationToken(const Crypto::Hash &tokenId) const;

        bool hasCollateralKeyImage(const Crypto::KeyImage &keyImage, uint32_t currentHeight) const;

        bool hasCollateralOutpoint(uint64_t amount, uint32_t globalOutputIndex, uint32_t currentHeight) const;

        bool isCollateralKeyImageSpendLocked(const Crypto::KeyImage &keyImage, uint32_t currentHeight) const;

        bool hasEndpointCommitment(const Crypto::Hash &commitment, uint32_t currentHeight) const;

        bool isBonded(const Crypto::Hash &masternodeId) const;

        bool hasCollateralBinding(const Crypto::Hash &masternodeId) const;

        bool canAcceptHeartbeat(const Crypto::Hash &masternodeId, uint32_t height) const;

        bool canAcceptAttestation(
            const Crypto::Hash &masternodeId,
            const Crypto::PublicKey &verifierKey,
            uint32_t height) const;

        bool canAcceptEndpointUpdate(const Crypto::Hash &masternodeId, uint32_t height) const;

        void updateEndpointCommitment(
            const Crypto::Hash &masternodeId,
            const Crypto::Hash &newCommitment,
            uint32_t height);

        nlohmann::json toJson() const;

        bool fromJson(const nlohmann::json &json);

      private:
        struct HealthSample
        {
            uint32_t height;
            bool healthy;
        };

        struct AttestationSample
        {
            uint32_t height;
            Crypto::PublicKey verifierKey;
            bool healthy;
        };

        struct RewardSample
        {
            uint32_t height;
            uint64_t amount;
        };

        struct State
        {
            std::deque<HealthSample> healthSamples;
            std::deque<AttestationSample> attestationSamples;
            std::deque<RewardSample> rewardSamples;
            uint32_t lastPaidHeight = 0;
            bool hasBeenPaid = false;
            std::optional<uint32_t> deactivationHeight;
            Status status = Status::Registered;
            bool bonded = false;
            uint64_t bondAmount = 0;
            std::optional<Crypto::Hash> registrationTokenId;
            uint32_t registrationExpiresAtHeight = 0;
            uint64_t collateralAmount = 0;
            uint32_t collateralGlobalOutputIndex = 0;
            std::optional<Crypto::KeyImage> collateralKeyImage;
            std::optional<Crypto::PublicKey> collateralOutputKey;
            std::optional<Crypto::Hash> endpointCommitment;
            std::optional<uint32_t> lastEndpointUpdateHeight;
            Crypto::PublicKey payoutKey = Crypto::PublicKey {{0}};
        };

        static bool hashLess(const Crypto::Hash &lhs, const Crypto::Hash &rhs);

        static uint32_t calculateWindowStart(uint32_t currentHeight, uint64_t window);

        std::unordered_map<Crypto::Hash, State> m_states;
    };

} // namespace CryptoNote
