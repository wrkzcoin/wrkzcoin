// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeStateTracker.h"

#include <algorithm>
#include <common/StringTools.h>
#include <config/CryptoNoteConfig.h>

namespace CryptoNote
{
    namespace
    {
        using parameters::MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS;
        using parameters::MASTERNODE_FAIRNESS_WINDOW_BLOCKS;
        using parameters::MASTERNODE_ATTESTATION_WINDOW_BLOCKS;
        using parameters::MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW;
        using parameters::MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT;
        using parameters::MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION;
        using parameters::MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER;
        using parameters::MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL;
        using parameters::MASTERNODE_HEALTH_WINDOW_BLOCKS;
        using parameters::MASTERNODE_MIN_HEALTH_PERCENT;
        using parameters::MASTERNODE_ENDPOINT_UPDATE_COOLDOWN_BLOCKS;

        bool isRewardCandidateStatus(const MasternodeStateTracker::Status status)
        {
            return status == MasternodeStateTracker::Status::Active;
        }
    }

    void MasternodeStateTracker::registerMasternode(
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
        const Crypto::Hash &endpointCommitment,
        bool hasSigningKey,
        const Crypto::PublicKey &signingKey)
    {
        State &state = m_states[masternodeId];
        state.status = Status::Registered;
        state.payoutKey = payoutKey;
        state.bonded = bonded;
        state.bondAmount = bondAmount;
        state.registrationTokenId = registrationTokenId;
        state.registrationExpiresAtHeight = registrationExpiresAtHeight;
        state.collateralAmount = collateralAmount;
        state.collateralGlobalOutputIndex = collateralGlobalOutputIndex;
        state.collateralKeyImage = collateralKeyImage;
        state.collateralOutputKey = collateralOutputKey;
        state.endpointCommitment = endpointCommitment;
        state.hasSigningKey = hasSigningKey;
        if (hasSigningKey)
        {
            state.signingKey = signingKey;
        }
    }

    bool MasternodeStateTracker::getSigningKey(const Crypto::Hash &masternodeId, Crypto::PublicKey &signingKey) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end() || !it->second.hasSigningKey)
        {
            return false;
        }
        signingKey = it->second.signingKey;
        return true;
    }

    void MasternodeStateTracker::activateMasternode(const Crypto::Hash &masternodeId)
    {
        State &state = m_states[masternodeId];
        state.status = Status::Active;
        state.deactivationHeight.reset();
    }

    void MasternodeStateTracker::deactivateMasternode(const Crypto::Hash &masternodeId, uint32_t height)
    {
        State &state = m_states[masternodeId];
        state.status = Status::Inactive;
        state.deactivationHeight = height;
    }

    void MasternodeStateTracker::penalizeMasternode(const Crypto::Hash &masternodeId, uint32_t height)
    {
        State &state = m_states[masternodeId];
        state.status = Status::Penalized;
        state.deactivationHeight = height;
    }

    void MasternodeStateTracker::revokeMasternode(const Crypto::Hash &masternodeId, uint32_t height)
    {
        State &state = m_states[masternodeId];
        state.status = Status::Revoked;
        state.deactivationHeight = height;
    }

    MasternodeStateTracker::Status MasternodeStateTracker::getStatus(const Crypto::Hash &masternodeId) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return Status::Registered;
        }

        return it->second.status;
    }

    void MasternodeStateTracker::recordHealthSample(const Crypto::Hash &masternodeId, uint32_t height, bool healthy)
    {
        State &state = m_states[masternodeId];
        state.healthSamples.push_back({height, healthy});

        const uint32_t windowStart = calculateWindowStart(height, MASTERNODE_HEALTH_WINDOW_BLOCKS);
        while (!state.healthSamples.empty() && state.healthSamples.front().height < windowStart)
        {
            state.healthSamples.pop_front();
        }
    }

    void MasternodeStateTracker::recordAttestationSample(
        const Crypto::Hash &masternodeId,
        uint32_t height,
        const Crypto::PublicKey &verifierKey,
        bool healthy)
    {
        State &state = m_states[masternodeId];
        state.attestationSamples.push_back({height, verifierKey, healthy});

        const uint32_t windowStart = calculateWindowStart(height, MASTERNODE_ATTESTATION_WINDOW_BLOCKS);
        while (!state.attestationSamples.empty() && state.attestationSamples.front().height < windowStart)
        {
            state.attestationSamples.pop_front();
        }
    }

    uint64_t MasternodeStateTracker::getHealthPercent(const Crypto::Hash &masternodeId, uint32_t currentHeight) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return 0;
        }

        const uint32_t windowStart = calculateWindowStart(currentHeight, MASTERNODE_HEALTH_WINDOW_BLOCKS);
        uint64_t totalSamples = 0;
        uint64_t healthySamples = 0;

        for (const auto &sample : it->second.healthSamples)
        {
            if (sample.height < windowStart)
            {
                continue;
            }

            ++totalSamples;
            if (sample.healthy)
            {
                ++healthySamples;
            }
        }

        if (totalSamples == 0)
        {
            return 0;
        }

        return (healthySamples * 100) / totalSamples;
    }

    bool MasternodeStateTracker::meetsHealthThreshold(const Crypto::Hash &masternodeId, uint32_t currentHeight) const
    {
        return getHealthPercent(masternodeId, currentHeight) >= MASTERNODE_MIN_HEALTH_PERCENT;
    }

    bool MasternodeStateTracker::meetsAttestationThreshold(const Crypto::Hash &masternodeId, uint32_t currentHeight) const
    {
        if (!MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION)
        {
            return true;
        }

        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return false;
        }

        const uint32_t windowStart = calculateWindowStart(currentHeight, MASTERNODE_ATTESTATION_WINDOW_BLOCKS);
        uint64_t totalSamples = 0;
        uint64_t healthySamples = 0;

        for (const auto &sample : it->second.attestationSamples)
        {
            if (sample.height < windowStart)
            {
                continue;
            }

            ++totalSamples;
            if (sample.healthy)
            {
                ++healthySamples;
            }
        }

        if (totalSamples < MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW)
        {
            return false;
        }

        if (totalSamples == 0)
        {
            return false;
        }

        return (healthySamples * 100) / totalSamples >= MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT;
    }

    void MasternodeStateTracker::markDeactivated(const Crypto::Hash &masternodeId, uint32_t height)
    {
        m_states[masternodeId].deactivationHeight = height;
    }

    void MasternodeStateTracker::clearDeactivation(const Crypto::Hash &masternodeId)
    {
        auto it = m_states.find(masternodeId);
        if (it != m_states.end())
        {
            it->second.deactivationHeight.reset();
        }
    }

    bool MasternodeStateTracker::isSpendLocked(const Crypto::Hash &masternodeId, uint32_t currentHeight) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end() || !it->second.deactivationHeight.has_value())
        {
            return false;
        }

        const uint64_t lockEndsAt =
            static_cast<uint64_t>(*it->second.deactivationHeight) + MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS;

        return static_cast<uint64_t>(currentHeight) < lockEndsAt;
    }

    void MasternodeStateTracker::recordReward(const Crypto::Hash &masternodeId, uint32_t height, uint64_t amount)
    {
        State &state = m_states[masternodeId];
        state.rewardSamples.push_back({height, amount});
        state.lastPaidHeight = height;
        state.hasBeenPaid = true;

        const uint32_t windowStart = calculateWindowStart(height, MASTERNODE_FAIRNESS_WINDOW_BLOCKS);
        while (!state.rewardSamples.empty() && state.rewardSamples.front().height < windowStart)
        {
            state.rewardSamples.pop_front();
        }
    }

    uint64_t
        MasternodeStateTracker::getRewardAmountInFairnessWindow(const Crypto::Hash &masternodeId, uint32_t currentHeight)
            const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return 0;
        }

        const uint32_t windowStart = calculateWindowStart(currentHeight, MASTERNODE_FAIRNESS_WINDOW_BLOCKS);
        uint64_t total = 0;
        for (const auto &sample : it->second.rewardSamples)
        {
            if (sample.height < windowStart)
            {
                continue;
            }
            total += sample.amount;
        }

        return total;
    }

    std::vector<Crypto::Hash>
        MasternodeStateTracker::filterRewardEligible(const std::vector<Crypto::Hash> &candidateIds, uint32_t currentHeight)
            const
    {
        std::vector<Crypto::Hash> eligible;
        eligible.reserve(candidateIds.size());

        for (const auto &candidateId : candidateIds)
        {
            const auto stateIt = m_states.find(candidateId);
            if (stateIt == m_states.end())
            {
                continue;
            }

            if (!isRewardCandidateStatus(stateIt->second.status))
            {
                continue;
            }

            if (!stateIt->second.bonded)
            {
                continue;
            }

            if (isSpendLocked(candidateId, currentHeight))
            {
                continue;
            }

            if (!meetsHealthThreshold(candidateId, currentHeight))
            {
                continue;
            }

            if (!meetsAttestationThreshold(candidateId, currentHeight))
            {
                continue;
            }

            eligible.push_back(candidateId);
        }

        return eligible;
    }

    std::optional<Crypto::Hash>
        MasternodeStateTracker::selectFairRewardWinner(const std::vector<Crypto::Hash> &candidateIds, uint32_t currentHeight)
            const
    {
        const auto eligible = filterRewardEligible(candidateIds, currentHeight);
        if (eligible.empty())
        {
            return std::nullopt;
        }

        const auto winner = std::min_element(
            eligible.begin(),
            eligible.end(),
            [this, currentHeight](const Crypto::Hash &lhs, const Crypto::Hash &rhs) {
                const uint64_t lhsReward = getRewardAmountInFairnessWindow(lhs, currentHeight);
                const uint64_t rhsReward = getRewardAmountInFairnessWindow(rhs, currentHeight);
                if (lhsReward != rhsReward)
                {
                    return lhsReward < rhsReward;
                }

                const auto lhsIt = m_states.find(lhs);
                const auto rhsIt = m_states.find(rhs);
                const uint32_t lhsLastPaid =
                    (lhsIt == m_states.end() || !lhsIt->second.hasBeenPaid) ? 0 : lhsIt->second.lastPaidHeight;
                const uint32_t rhsLastPaid =
                    (rhsIt == m_states.end() || !rhsIt->second.hasBeenPaid) ? 0 : rhsIt->second.lastPaidHeight;
                if (lhsLastPaid != rhsLastPaid)
                {
                    return lhsLastPaid < rhsLastPaid;
                }

                return hashLess(lhs, rhs);
            });

        return winner == eligible.end() ? std::nullopt : std::optional<Crypto::Hash>(*winner);
    }

    MasternodeStateTracker::RewardDistribution MasternodeStateTracker::calculateRewardDistribution(
        uint64_t totalReward,
        uint32_t currentHeight,
        const std::vector<Crypto::Hash> &candidateIds,
        uint64_t masternodePercent) const
    {
        RewardDistribution result;

        if (masternodePercent > 100)
        {
            masternodePercent = 100;
        }

        const auto winner = selectFairRewardWinner(candidateIds, currentHeight);
        if (!winner.has_value())
        {
            result.powReward = totalReward;
            return result;
        }

        result.hasMasternodeWinner = true;
        result.masternodeWinner = winner;
        result.masternodeReward = (totalReward * masternodePercent) / 100;
        result.powReward = totalReward - result.masternodeReward;
        return result;
    }

    std::vector<Crypto::Hash> MasternodeStateTracker::getTrackedMasternodeIds() const
    {
        std::vector<Crypto::Hash> ids;
        ids.reserve(m_states.size());
        for (const auto &[id, _] : m_states)
        {
            ids.push_back(id);
        }

        std::sort(ids.begin(), ids.end(), [](const Crypto::Hash &lhs, const Crypto::Hash &rhs) {
            return hashLess(lhs, rhs);
        });

        return ids;
    }

    void MasternodeStateTracker::clear()
    {
        m_states.clear();
    }

    std::vector<MasternodeStateTracker::Snapshot>
        MasternodeStateTracker::getSnapshots(uint32_t currentHeight, size_t offset, size_t limit) const
    {
        std::vector<Crypto::Hash> ids = getTrackedMasternodeIds();
        if (offset >= ids.size())
        {
            return {};
        }

        const size_t count = std::min(limit, ids.size() - offset);
        std::vector<Snapshot> snapshots;
        snapshots.reserve(count);

        for (size_t i = offset; i < offset + count; ++i)
        {
            const auto &id = ids[i];
            const auto stateIt = m_states.find(id);
            if (stateIt == m_states.end())
            {
                continue;
            }

            Snapshot snapshot;
            snapshot.masternodeId = id;
            snapshot.status = stateIt->second.status;
            snapshot.bonded = stateIt->second.bonded;
            snapshot.bondAmount = stateIt->second.bondAmount;
            snapshot.collateralAmount = stateIt->second.collateralAmount;
            snapshot.collateralGlobalOutputIndex = stateIt->second.collateralGlobalOutputIndex;
            snapshot.hasCollateral = stateIt->second.collateralKeyImage.has_value();
            snapshot.hasEndpointCommitment = stateIt->second.endpointCommitment.has_value();
            if (stateIt->second.endpointCommitment.has_value())
            {
                snapshot.endpointCommitment = *stateIt->second.endpointCommitment;
            }
            snapshot.healthPercent = getHealthPercent(id, currentHeight);
            snapshot.spendLocked = isSpendLocked(id, currentHeight);
            snapshot.lastPaidHeight = stateIt->second.lastPaidHeight;
            snapshot.rewardInFairnessWindow = getRewardAmountInFairnessWindow(id, currentHeight);
            snapshot.hasSigningKey = stateIt->second.hasSigningKey;
            if (stateIt->second.hasSigningKey)
            {
                snapshot.signingKey = stateIt->second.signingKey;
            }
            snapshots.push_back(snapshot);
        }

        return snapshots;
    }

    bool MasternodeStateTracker::hasMasternode(const Crypto::Hash &masternodeId) const
    {
        return m_states.find(masternodeId) != m_states.end();
    }

    bool MasternodeStateTracker::getPayoutKey(const Crypto::Hash &masternodeId, Crypto::PublicKey &payoutKey) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return false;
        }

        payoutKey = it->second.payoutKey;
        return true;
    }

    bool MasternodeStateTracker::hasUsedRegistrationToken(const Crypto::Hash &tokenId) const
    {
        for (const auto &[_, state] : m_states)
        {
            if (state.registrationTokenId.has_value() && *state.registrationTokenId == tokenId)
            {
                return true;
            }
        }

        return false;
    }

    bool MasternodeStateTracker::hasCollateralKeyImage(const Crypto::KeyImage &keyImage, uint32_t currentHeight) const
    {
        for (const auto &[_, state] : m_states)
        {
            if (!state.collateralKeyImage.has_value() || *state.collateralKeyImage != keyImage)
            {
                continue;
            }

            if (state.status == Status::Revoked)
            {
                if (!state.deactivationHeight.has_value())
                {
                    continue;
                }

                if (
                    static_cast<uint64_t>(currentHeight)
                    >= static_cast<uint64_t>(*state.deactivationHeight) + MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS)
                {
                    continue;
                }
            }

            return true;
        }

        return false;
    }

    bool MasternodeStateTracker::hasCollateralOutpoint(
        uint64_t amount,
        uint32_t globalOutputIndex,
        uint32_t currentHeight) const
    {
        for (const auto &[_, state] : m_states)
        {
            if (state.collateralAmount != amount || state.collateralGlobalOutputIndex != globalOutputIndex)
            {
                continue;
            }

            if (state.status == Status::Revoked)
            {
                if (!state.deactivationHeight.has_value())
                {
                    continue;
                }

                if (
                    static_cast<uint64_t>(currentHeight)
                    >= static_cast<uint64_t>(*state.deactivationHeight) + MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS)
                {
                    continue;
                }
            }

            return true;
        }

        return false;
    }

    bool MasternodeStateTracker::isCollateralKeyImageSpendLocked(
        const Crypto::KeyImage &keyImage,
        uint32_t currentHeight) const
    {
        for (const auto &[_, state] : m_states)
        {
            if (!state.collateralKeyImage.has_value() || *state.collateralKeyImage != keyImage)
            {
                continue;
            }

            if (state.status == Status::Revoked)
            {
                if (!state.deactivationHeight.has_value())
                {
                    return false;
                }

                return static_cast<uint64_t>(currentHeight)
                    < static_cast<uint64_t>(*state.deactivationHeight) + MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS;
            }

            if (state.status == Status::Inactive || state.status == Status::Penalized)
            {
                if (!state.deactivationHeight.has_value())
                {
                    return true;
                }

                return static_cast<uint64_t>(currentHeight)
                    < static_cast<uint64_t>(*state.deactivationHeight) + MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS;
            }

            return true;
        }

        return false;
    }

    bool MasternodeStateTracker::hasEndpointCommitment(const Crypto::Hash &commitment, uint32_t currentHeight) const
    {
        for (const auto &[_, state] : m_states)
        {
            if (!state.endpointCommitment.has_value() || *state.endpointCommitment != commitment)
            {
                continue;
            }

            if (state.status == Status::Revoked)
            {
                if (!state.deactivationHeight.has_value())
                {
                    continue;
                }

                if (
                    static_cast<uint64_t>(currentHeight)
                    >= static_cast<uint64_t>(*state.deactivationHeight) + MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS)
                {
                    continue;
                }
            }

            return true;
        }

        return false;
    }

    bool MasternodeStateTracker::isBonded(const Crypto::Hash &masternodeId) const
    {
        const auto it = m_states.find(masternodeId);
        return it != m_states.end() && it->second.bonded;
    }

    bool MasternodeStateTracker::hasCollateralBinding(const Crypto::Hash &masternodeId) const
    {
        const auto it = m_states.find(masternodeId);
        return it != m_states.end() && it->second.collateralKeyImage.has_value() && it->second.collateralOutputKey.has_value();
    }

    bool MasternodeStateTracker::canAcceptHeartbeat(const Crypto::Hash &masternodeId, uint32_t height) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return false;
        }

        if (it->second.healthSamples.empty())
        {
            return true;
        }

        const uint32_t lastHeight = it->second.healthSamples.back().height;
        if (height <= lastHeight)
        {
            return false;
        }

        const uint64_t delta = static_cast<uint64_t>(height) - lastHeight;
        return delta >= MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL;
    }

    bool MasternodeStateTracker::canAcceptAttestation(
        const Crypto::Hash &masternodeId,
        const Crypto::PublicKey &verifierKey,
        uint32_t height) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return false;
        }

        const uint32_t windowStart = calculateWindowStart(height, MASTERNODE_ATTESTATION_WINDOW_BLOCKS);
        for (auto sampleIt = it->second.attestationSamples.rbegin(); sampleIt != it->second.attestationSamples.rend(); ++sampleIt)
        {
            if (sampleIt->height < windowStart)
            {
                break;
            }

            if (sampleIt->verifierKey != verifierKey)
            {
                continue;
            }

            if (height <= sampleIt->height)
            {
                return false;
            }

            const uint64_t delta = static_cast<uint64_t>(height) - sampleIt->height;
            return delta >= MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER;
        }

        return true;
    }

    bool MasternodeStateTracker::canAcceptEndpointUpdate(const Crypto::Hash &masternodeId, uint32_t height) const
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return false;
        }

        if (!it->second.lastEndpointUpdateHeight.has_value())
        {
            return true;
        }

        const uint32_t last = *it->second.lastEndpointUpdateHeight;
        if (height <= last)
        {
            return false;
        }

        const uint64_t delta = static_cast<uint64_t>(height) - last;
        return delta >= MASTERNODE_ENDPOINT_UPDATE_COOLDOWN_BLOCKS;
    }

    void MasternodeStateTracker::updateEndpointCommitment(
        const Crypto::Hash &masternodeId,
        const Crypto::Hash &newCommitment,
        uint32_t height)
    {
        const auto it = m_states.find(masternodeId);
        if (it == m_states.end())
        {
            return;
        }

        it->second.endpointCommitment = newCommitment;
        it->second.lastEndpointUpdateHeight = height;
    }

    nlohmann::json MasternodeStateTracker::toJson() const
    {
        nlohmann::json root = nlohmann::json::array();

        for (const auto &[id, state] : m_states)
        {
            nlohmann::json item;
            item["id"] = Common::podToHex(id);
            item["status"] = static_cast<uint8_t>(state.status);
            item["last_paid_height"] = state.lastPaidHeight;
            item["has_been_paid"] = state.hasBeenPaid;
            item["payout_key"] = Common::podToHex(state.payoutKey);
            item["bonded"] = state.bonded;
            item["bond_amount"] = state.bondAmount;
            item["registration_token_id"] =
                state.registrationTokenId.has_value() ? nlohmann::json(Common::podToHex(*state.registrationTokenId))
                                                      : nlohmann::json(nullptr);
            item["registration_expires_at_height"] = state.registrationExpiresAtHeight;
            item["collateral_amount"] = state.collateralAmount;
            item["collateral_global_output_index"] = state.collateralGlobalOutputIndex;
            item["collateral_key_image"] =
                state.collateralKeyImage.has_value() ? nlohmann::json(Common::podToHex(*state.collateralKeyImage))
                                                     : nlohmann::json(nullptr);
            item["collateral_output_key"] =
                state.collateralOutputKey.has_value() ? nlohmann::json(Common::podToHex(*state.collateralOutputKey))
                                                      : nlohmann::json(nullptr);
            item["endpoint_commitment"] =
                state.endpointCommitment.has_value() ? nlohmann::json(Common::podToHex(*state.endpointCommitment))
                                                     : nlohmann::json(nullptr);
            item["last_endpoint_update_height"] =
                state.lastEndpointUpdateHeight.has_value() ? nlohmann::json(*state.lastEndpointUpdateHeight)
                                                           : nlohmann::json(nullptr);
            item["deactivation_height"] =
                state.deactivationHeight.has_value() ? nlohmann::json(*state.deactivationHeight) : nlohmann::json(nullptr);
            item["signing_key"] =
                state.hasSigningKey ? nlohmann::json(Common::podToHex(state.signingKey)) : nlohmann::json(nullptr);

            nlohmann::json health = nlohmann::json::array();
            for (const auto &sample : state.healthSamples)
            {
                health.push_back({{"h", sample.height}, {"ok", sample.healthy}});
            }
            item["health"] = std::move(health);

            nlohmann::json rewards = nlohmann::json::array();
            for (const auto &sample : state.rewardSamples)
            {
                rewards.push_back({{"h", sample.height}, {"a", sample.amount}});
            }
            item["rewards"] = std::move(rewards);

            nlohmann::json attestations = nlohmann::json::array();
            for (const auto &sample : state.attestationSamples)
            {
                attestations.push_back(
                    {{"h", sample.height}, {"v", Common::podToHex(sample.verifierKey)}, {"ok", sample.healthy}});
            }
            item["attestations"] = std::move(attestations);

            root.push_back(std::move(item));
        }

        return root;
    }

    bool MasternodeStateTracker::fromJson(const nlohmann::json &json)
    {
        if (!json.is_array())
        {
            return false;
        }

        std::unordered_map<Crypto::Hash, State> loaded;

        for (const auto &item : json)
        {
            if (
                !item.is_object() || !item.contains("id") || !item.contains("status") || !item.contains("payout_key")
                || !item.contains("bonded") || !item.contains("bond_amount") || !item.contains("registration_expires_at_height"))
            {
                return false;
            }

            Crypto::Hash id;
            if (!Common::podFromHex(item.at("id").get<std::string>(), id))
            {
                return false;
            }

            State state;
            state.status = static_cast<Status>(item.at("status").get<uint8_t>());
            state.lastPaidHeight = item.value("last_paid_height", 0u);
            state.hasBeenPaid = item.value("has_been_paid", false);
            state.bonded = item.at("bonded").get<bool>();
            state.bondAmount = item.at("bond_amount").get<uint64_t>();
            state.registrationExpiresAtHeight = item.at("registration_expires_at_height").get<uint32_t>();
            state.collateralAmount = item.value("collateral_amount", 0ULL);
            state.collateralGlobalOutputIndex = item.value("collateral_global_output_index", 0U);

            if (item.contains("registration_token_id") && !item.at("registration_token_id").is_null())
            {
                Crypto::Hash tokenId;
                if (!Common::podFromHex(item.at("registration_token_id").get<std::string>(), tokenId))
                {
                    return false;
                }
                state.registrationTokenId = tokenId;
            }

            if (item.contains("collateral_key_image") && !item.at("collateral_key_image").is_null())
            {
                Crypto::KeyImage keyImage;
                if (!Common::podFromHex(item.at("collateral_key_image").get<std::string>(), keyImage))
                {
                    return false;
                }
                state.collateralKeyImage = keyImage;
            }

            if (item.contains("collateral_output_key") && !item.at("collateral_output_key").is_null())
            {
                Crypto::PublicKey outputKey;
                if (!Common::podFromHex(item.at("collateral_output_key").get<std::string>(), outputKey))
                {
                    return false;
                }
                state.collateralOutputKey = outputKey;
            }

            if (item.contains("endpoint_commitment") && !item.at("endpoint_commitment").is_null())
            {
                Crypto::Hash commitment;
                if (!Common::podFromHex(item.at("endpoint_commitment").get<std::string>(), commitment))
                {
                    return false;
                }
                state.endpointCommitment = commitment;
            }

            if (!Common::podFromHex(item.at("payout_key").get<std::string>(), state.payoutKey))
            {
                return false;
            }

            if (item.contains("last_endpoint_update_height") && !item.at("last_endpoint_update_height").is_null())
            {
                state.lastEndpointUpdateHeight = item.at("last_endpoint_update_height").get<uint32_t>();
            }

            if (item.contains("deactivation_height") && !item.at("deactivation_height").is_null())
            {
                state.deactivationHeight = item.at("deactivation_height").get<uint32_t>();
            }

            if (item.contains("signing_key") && !item.at("signing_key").is_null())
            {
                Crypto::PublicKey sk;
                if (!Common::podFromHex(item.at("signing_key").get<std::string>(), sk))
                {
                    return false;
                }
                state.hasSigningKey = true;
                state.signingKey = sk;
            }

            if (item.contains("health"))
            {
                for (const auto &sample : item.at("health"))
                {
                    state.healthSamples.push_back({sample.at("h").get<uint32_t>(), sample.at("ok").get<bool>()});
                }
            }

            if (item.contains("rewards"))
            {
                for (const auto &sample : item.at("rewards"))
                {
                    state.rewardSamples.push_back({sample.at("h").get<uint32_t>(), sample.at("a").get<uint64_t>()});
                }
            }

            if (item.contains("attestations"))
            {
                for (const auto &sample : item.at("attestations"))
                {
                    AttestationSample att;
                    att.height = sample.at("h").get<uint32_t>();
                    if (!Common::podFromHex(sample.at("v").get<std::string>(), att.verifierKey))
                    {
                        return false;
                    }
                    att.healthy = sample.at("ok").get<bool>();
                    state.attestationSamples.push_back(att);
                }
            }

            loaded.emplace(id, std::move(state));
        }

        m_states = std::move(loaded);
        return true;
    }

    bool MasternodeStateTracker::hashLess(const Crypto::Hash &lhs, const Crypto::Hash &rhs)
    {
        return std::lexicographical_compare(std::begin(lhs.data), std::end(lhs.data), std::begin(rhs.data), std::end(rhs.data));
    }

    uint32_t MasternodeStateTracker::calculateWindowStart(uint32_t currentHeight, uint64_t window)
    {
        if (window <= 1)
        {
            return currentHeight;
        }

        const uint64_t distance = window - 1;
        if (static_cast<uint64_t>(currentHeight) <= distance)
        {
            return 0;
        }

        return static_cast<uint32_t>(static_cast<uint64_t>(currentHeight) - distance);
    }

} // namespace CryptoNote
