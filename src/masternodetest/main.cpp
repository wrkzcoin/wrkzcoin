// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/* masternodetest — self-contained checks for the masternode consensus building blocks:
 *   - MN01 payload layouts (build <-> parse round trips, exact sizes, malformed input)
 *   - MasternodeStateTracker (lifecycle, spend-lock, anti-replay counters, fairness, JSON)
 *   - reward output derivation (deterministic coinbase key, wallet-side detectability)
 *   - ChainLockManager / InstantSendManager (assembly, equivocation, expiry, pruning)
 *   - MasternodeQuorum (determinism)
 *
 * Build:  cmake --build build --target masternodetest
 * Run:    ./build/src/masternodetest      (exit code 0 == all checks passed)
 */

#include <CryptoNote.h>
#include <CryptoTypes.h>
#include <common/StringTools.h>
#include <config/Constants.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/crypto.h>
#include <crypto/hash.h>
#include <crypto/random.h>
#include <cryptonotecore/ChainLockManager.h>
#include <cryptonotecore/InstantSendManager.h>
#include <cryptonotecore/MasternodeQuorum.h>
#include <cryptonotecore/MasternodeReward.h>
#include <cryptonotecore/MasternodeStateTracker.h>
#include <cryptonotecore/MasternodeTx.h>
#include <json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace CryptoNote;
using Tracker = MasternodeStateTracker;
using Status = MasternodeStateTracker::Status;

/* ------------------------------------------------------------------------------------------ */
/* Tiny test harness                                                                           */
/* ------------------------------------------------------------------------------------------ */

namespace
{
    int g_checks = 0;
    int g_failures = 0;
    std::string g_currentTest;

    void check(bool condition, const char *expression, const char *file, int line)
    {
        ++g_checks;
        if (!condition)
        {
            ++g_failures;
            std::cout << "  FAIL [" << g_currentTest << "] " << expression << "  (" << file << ":" << line << ")"
                      << std::endl;
        }
    }

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

    void runTest(const char *name, const std::function<void()> &body)
    {
        g_currentTest = name;
        const int failuresBefore = g_failures;
        try
        {
            body();
        }
        catch (const std::exception &e)
        {
            ++g_failures;
            std::cout << "  EXCEPTION [" << name << "] " << e.what() << std::endl;
        }
        std::cout << (g_failures == failuresBefore ? "  ok   " : "  FAIL ") << name << std::endl;
    }

    /* ---- helpers ------------------------------------------------------------------------- */

    Crypto::Hash randomHash()
    {
        Crypto::Hash h;
        Random::randomBytes(sizeof(h.data), h.data);
        return h;
    }

    Crypto::KeyImage randomKeyImage()
    {
        /* A real key image must be a valid curve point for the consensus checks that care, but the
         * tracker / managers treat it as an opaque 32-byte identifier, so random bytes are fine. */
        Crypto::KeyImage ki;
        Random::randomBytes(sizeof(ki.data), ki.data);
        return ki;
    }

    struct KeyPairT
    {
        Crypto::PublicKey pub;
        Crypto::SecretKey sec;
    };

    KeyPairT makeKeys()
    {
        KeyPairT k;
        Crypto::generate_keys(k.pub, k.sec);
        return k;
    }

    Crypto::Signature signPayload(const std::vector<uint8_t> &unsignedPayload, const KeyPairT &keys)
    {
        const Crypto::Hash hash = Crypto::cn_fast_hash(unsignedPayload.data(), unsignedPayload.size());
        Crypto::Signature sig;
        Crypto::generate_signature(hash, keys.pub, keys.sec, sig);
        return sig;
    }

    void appendVarint(std::vector<uint8_t> &out, size_t value)
    {
        while (value >= 0x80)
        {
            out.push_back(static_cast<uint8_t>((value & 0x7f) | 0x80));
            value >>= 7;
        }
        out.push_back(static_cast<uint8_t>(value));
    }

    /* Wrap an MN01 payload into a transaction extra exactly like the wallet / signer do:
     *   [0x02][varint(nonce size)][0x7f][varint(payload size)][payload] */
    Transaction makeTransactionWithPayload(const std::vector<uint8_t> &payload)
    {
        std::vector<uint8_t> nonce;
        nonce.push_back(Constants::TX_EXTRA_ARBITRARY_DATA_IDENTIFIER);
        appendVarint(nonce, payload.size());
        nonce.insert(nonce.end(), payload.begin(), payload.end());

        std::vector<uint8_t> extra;
        extra.push_back(Constants::TX_EXTRA_NONCE_IDENTIFIER);
        appendVarint(extra, nonce.size());
        extra.insert(extra.end(), nonce.begin(), nonce.end());

        Transaction tx;
        tx.version = CURRENT_TRANSACTION_VERSION;
        tx.unlockTime = 0;
        tx.extra = extra;
        return tx;
    }

    std::vector<uint8_t> withSignature(const std::vector<uint8_t> &unsignedPayload, const Crypto::Signature &sig)
    {
        std::vector<uint8_t> full = unsignedPayload;
        full.insert(full.end(), sig.data, sig.data + sizeof(sig.data));
        return full;
    }

    MasternodeTxParseResult parse(const std::vector<uint8_t> &payload, MasternodeTxPayload &out)
    {
        std::string error;
        return parseMasternodeTxPayload(makeTransactionWithPayload(payload), out, error);
    }

    bool bytesEqual(const uint8_t *a, const uint8_t *b, size_t n)
    {
        return std::memcmp(a, b, n) == 0;
    }

    struct RegisteredMn
    {
        Crypto::Hash id;
        KeyPairT payout;
        KeyPairT view;
        KeyPairT signing;
        Crypto::KeyImage collateralKeyImage;
        Crypto::PublicKey collateralOutputKey;
        Crypto::Hash endpoint;
        Crypto::Hash token;
    };

    RegisteredMn registerRandom(Tracker &tracker, uint64_t collateral = parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT)
    {
        RegisteredMn mn;
        mn.id = randomHash();
        mn.payout = makeKeys();
        mn.view = makeKeys();
        mn.signing = makeKeys();
        mn.collateralKeyImage = randomKeyImage();
        mn.collateralOutputKey = makeKeys().pub;
        mn.endpoint = randomHash();
        mn.token = randomHash();
        tracker.registerMasternode(
            mn.id,
            mn.payout.pub,
            mn.view.pub,
            collateral >= parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT,
            collateral,
            mn.token,
            1000,
            collateral,
            42,
            mn.collateralKeyImage,
            mn.collateralOutputKey,
            mn.endpoint,
            true,
            mn.signing.pub);
        return mn;
    }

    /* Make `mn` reward-eligible at `height`: Active + 100% heartbeat health + enough attestations. */
    void makeEligible(Tracker &tracker, const RegisteredMn &mn, uint32_t height)
    {
        tracker.activateMasternode(mn.id);
        for (uint32_t i = 0; i < 20; ++i)
        {
            const uint32_t h = height - 100 + i * 5;
            tracker.recordHealthSample(mn.id, h, true, h);
        }
        const KeyPairT verifier = makeKeys();
        for (uint64_t i = 0; i < parameters::MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW; ++i)
        {
            const uint32_t h = static_cast<uint32_t>(height - 2000 + i * 60);
            tracker.recordAttestationSample(mn.id, h, verifier.pub, true, h);
        }
    }
} // namespace

/* ------------------------------------------------------------------------------------------ */
/* 1. Payload layouts                                                                          */
/* ------------------------------------------------------------------------------------------ */

static void testPayloadSizes()
{
    CHECK(MASTERNODE_PAYLOAD_HEADER_SIZE == 37);
    CHECK(MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE == 277);
    CHECK(MASTERNODE_REGISTER_PAYLOAD_SIZE == 405);
    CHECK(MASTERNODE_HEARTBEAT_PAYLOAD_SIZE == 106);
    CHECK(MASTERNODE_ATTEST_PAYLOAD_SIZE == 138);
    CHECK(MASTERNODE_ACTION_PAYLOAD_SIZE == 105);
    CHECK(MASTERNODE_UPDATE_ENDPOINT_PAYLOAD_SIZE == 137);
    /* The zero-input heartbeat exemption relies on the payload fitting one varint byte. */
    CHECK(MASTERNODE_HEARTBEAT_PAYLOAD_SIZE < 128);
}

static void testRegisterRoundTrip()
{
    const KeyPairT payout = makeKeys();
    const KeyPairT view = makeKeys();
    const KeyPairT signing = makeKeys();
    const KeyPairT collateral = makeKeys();

    MasternodeRegisterFields f;
    f.masternodeId = randomHash();
    f.payoutKey = payout.pub;
    f.payoutViewKey = view.pub;
    f.registrationTokenId = randomHash();
    f.registrationExpiresAtHeight = 5001000;
    f.collateralAmount = parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT;
    f.collateralGlobalOutputIndex = 123456;
    f.collateralKeyImage = randomKeyImage();
    f.collateralOutputKey = collateral.pub;
    f.endpointCommitment = randomHash();
    f.signingKey = signing.pub;

    const auto unsignedPayload = buildMasternodeRegisterUnsignedPayload(f);
    CHECK(unsignedPayload.size() == MASTERNODE_REGISTER_UNSIGNED_PAYLOAD_SIZE);

    const Crypto::Signature payoutSig = signPayload(unsignedPayload, payout);
    const Crypto::Signature collateralProof = signPayload(unsignedPayload, collateral); // any 64 bytes for layout
    std::vector<uint8_t> full = withSignature(unsignedPayload, payoutSig);
    full.insert(full.end(), collateralProof.data, collateralProof.data + sizeof(collateralProof.data));
    CHECK(full.size() == MASTERNODE_REGISTER_PAYLOAD_SIZE);

    MasternodeTxPayload p;
    CHECK(parse(full, p) == MasternodeTxParseResult::Valid);
    CHECK(p.type == MasternodeTxType::Register);
    CHECK(p.masternodeId == f.masternodeId);
    CHECK(p.hasPayoutKey && p.payoutKey == f.payoutKey);
    CHECK(p.hasPayoutViewKey && p.payoutViewKey == f.payoutViewKey);
    CHECK(p.hasRegistrationToken && p.registrationTokenId == f.registrationTokenId);
    CHECK(p.registrationExpiresAtHeight == f.registrationExpiresAtHeight);
    CHECK(p.hasCollateral && p.collateralAmount == f.collateralAmount);
    CHECK(p.collateralGlobalOutputIndex == f.collateralGlobalOutputIndex);
    CHECK(p.collateralKeyImage == f.collateralKeyImage);
    CHECK(p.collateralOutputKey == f.collateralOutputKey);
    CHECK(p.hasEndpointCommitment && p.endpointCommitment == f.endpointCommitment);
    CHECK(p.hasSigningKey && p.signingKey == f.signingKey);
    CHECK(!p.hasHeight);
    CHECK(p.hasSignature && bytesEqual(p.signature.data, payoutSig.data, sizeof(payoutSig.data)));
    CHECK(p.hasCollateralSignature && bytesEqual(p.collateralSignature.data, collateralProof.data, 64));
    CHECK(p.unsignedPayload == unsignedPayload);

    /* The signing hash commits to the whole unsigned portion (payout signature verifies). */
    Crypto::Hash signingHash;
    CHECK(getMasternodeTxSigningHash(p, signingHash));
    CHECK(Crypto::check_signature(signingHash, payout.pub, p.signature));

    /* Old (v2, no payout view key) registration is rejected outright. */
    std::vector<uint8_t> v2 = unsignedPayload;
    v2.resize(v2.size() - sizeof(Crypto::PublicKey));
    v2.insert(v2.end(), payoutSig.data, payoutSig.data + 64);
    v2.insert(v2.end(), collateralProof.data, collateralProof.data + 64);
    MasternodeTxPayload rejected;
    CHECK(parse(v2, rejected) == MasternodeTxParseResult::Invalid);
}

static void testSignedPayloadRoundTrips()
{
    const KeyPairT keys = makeKeys();
    const Crypto::Hash id = randomHash();

    /* Heartbeat */
    {
        const auto u = buildMasternodeHeartbeatUnsignedPayload(id, 5000123, true);
        CHECK(u.size() == MASTERNODE_HEARTBEAT_UNSIGNED_PAYLOAD_SIZE);
        const auto full = withSignature(u, signPayload(u, keys));
        CHECK(full.size() == MASTERNODE_HEARTBEAT_PAYLOAD_SIZE);
        MasternodeTxPayload p;
        CHECK(parse(full, p) == MasternodeTxParseResult::Valid);
        CHECK(p.type == MasternodeTxType::Heartbeat);
        CHECK(p.masternodeId == id);
        CHECK(p.hasHeight && p.height == 5000123);
        CHECK(p.healthy);
        CHECK(p.unsignedPayload == u);

        const auto uUnhealthy = buildMasternodeHeartbeatUnsignedPayload(id, 7, false);
        MasternodeTxPayload q;
        CHECK(parse(withSignature(uUnhealthy, signPayload(uUnhealthy, keys)), q) == MasternodeTxParseResult::Valid);
        CHECK(!q.healthy && q.height == 7);

        /* health flag must be 0/1 */
        auto bad = full;
        bad[MASTERNODE_PAYLOAD_HEADER_SIZE + 4] = 2;
        MasternodeTxPayload r;
        CHECK(parse(bad, r) == MasternodeTxParseResult::Invalid);
    }

    /* Attest */
    {
        const KeyPairT verifier = makeKeys();
        const auto u = buildMasternodeAttestUnsignedPayload(id, 5000200, verifier.pub, false);
        CHECK(u.size() == MASTERNODE_ATTEST_UNSIGNED_PAYLOAD_SIZE);
        const auto full = withSignature(u, signPayload(u, verifier));
        CHECK(full.size() == MASTERNODE_ATTEST_PAYLOAD_SIZE);
        MasternodeTxPayload p;
        CHECK(parse(full, p) == MasternodeTxParseResult::Valid);
        CHECK(p.type == MasternodeTxType::Attest);
        CHECK(p.hasHeight && p.height == 5000200);
        CHECK(p.hasVerifierKey && p.verifierKey == verifier.pub);
        CHECK(!p.healthy);
        Crypto::Hash h;
        CHECK(getMasternodeTxSigningHash(p, h) && Crypto::check_signature(h, verifier.pub, p.signature));
    }

    /* Activate / Deactivate / Penalize / Revoke */
    for (const auto type :
         {MasternodeTxType::Activate, MasternodeTxType::Deactivate, MasternodeTxType::Penalize, MasternodeTxType::Revoke})
    {
        const auto u = buildMasternodeActionUnsignedPayload(type, id, 5000300);
        CHECK(u.size() == MASTERNODE_ACTION_UNSIGNED_PAYLOAD_SIZE);
        const auto full = withSignature(u, signPayload(u, keys));
        CHECK(full.size() == MASTERNODE_ACTION_PAYLOAD_SIZE);
        MasternodeTxPayload p;
        CHECK(parse(full, p) == MasternodeTxParseResult::Valid);
        CHECK(p.type == type);
        CHECK(p.hasHeight && p.height == 5000300);
        CHECK(p.masternodeId == id);
    }

    /* UpdateEndpoint */
    {
        const Crypto::Hash newCommitment = randomHash();
        const auto u = buildMasternodeUpdateEndpointUnsignedPayload(id, 5000400, newCommitment);
        CHECK(u.size() == MASTERNODE_UPDATE_ENDPOINT_UNSIGNED_PAYLOAD_SIZE);
        const auto full = withSignature(u, signPayload(u, keys));
        CHECK(full.size() == MASTERNODE_UPDATE_ENDPOINT_PAYLOAD_SIZE);
        MasternodeTxPayload p;
        CHECK(parse(full, p) == MasternodeTxParseResult::Valid);
        CHECK(p.type == MasternodeTxType::UpdateEndpoint);
        CHECK(p.hasHeight && p.height == 5000400);
        CHECK(p.hasNewEndpointCommitment && p.newEndpointCommitment == newCommitment);
    }
}

static void testPayloadRejections()
{
    const KeyPairT keys = makeKeys();
    const Crypto::Hash id = randomHash();
    const auto u = buildMasternodeHeartbeatUnsignedPayload(id, 10, true);
    const auto full = withSignature(u, signPayload(u, keys));

    MasternodeTxPayload p;

    /* one byte short / one byte long */
    auto shortPayload = full;
    shortPayload.pop_back();
    CHECK(parse(shortPayload, p) == MasternodeTxParseResult::Invalid);
    auto longPayload = full;
    longPayload.push_back(0);
    CHECK(parse(longPayload, p) == MasternodeTxParseResult::Invalid);

    /* unknown type */
    auto unknownType = full;
    unknownType[4] = 0x42;
    CHECK(parse(unknownType, p) == MasternodeTxParseResult::Invalid);

    /* wrong magic => not a masternode payload at all */
    auto wrongMagic = full;
    wrongMagic[0] = 'X';
    CHECK(parse(wrongMagic, p) == MasternodeTxParseResult::NotFound);

    /* no extra data => not found */
    Transaction empty;
    std::string error;
    CHECK(parseMasternodeTxPayload(empty, p, error) == MasternodeTxParseResult::NotFound);

    /* header only (truncated) */
    std::vector<uint8_t> headerOnly(full.begin(), full.begin() + MASTERNODE_PAYLOAD_HEADER_SIZE);
    CHECK(parse(headerOnly, p) == MasternodeTxParseResult::Invalid);
}

/* ------------------------------------------------------------------------------------------ */
/* 2. MasternodeStateTracker                                                                   */
/* ------------------------------------------------------------------------------------------ */

static void testTrackerLifecycle()
{
    Tracker tracker;
    const RegisteredMn mn = registerRandom(tracker);

    CHECK(tracker.hasMasternode(mn.id));
    CHECK(tracker.getStatus(mn.id) == Status::Registered);
    CHECK(tracker.isBonded(mn.id));
    CHECK(tracker.hasCollateralBinding(mn.id));

    Crypto::PublicKey spend, view, signing;
    CHECK(tracker.getPayoutAddressKeys(mn.id, spend, view));
    CHECK(spend == mn.payout.pub && view == mn.view.pub);
    CHECK(tracker.getSigningKey(mn.id, signing) && signing == mn.signing.pub);

    /* Registered -> Active -> Inactive -> Active (re-activate) -> Penalized -> Revoked */
    tracker.activateMasternode(mn.id);
    CHECK(tracker.getStatus(mn.id) == Status::Active);
    tracker.deactivateMasternode(mn.id, 100);
    CHECK(tracker.getStatus(mn.id) == Status::Inactive);
    tracker.activateMasternode(mn.id);
    CHECK(tracker.getStatus(mn.id) == Status::Active);
    tracker.deactivateMasternode(mn.id, 200);
    tracker.penalizeMasternode(mn.id, 300);
    CHECK(tracker.getStatus(mn.id) == Status::Penalized);
    tracker.activateMasternode(mn.id); // not allowed from Penalized
    CHECK(tracker.getStatus(mn.id) == Status::Penalized);
    tracker.revokeMasternode(mn.id, 400);
    CHECK(tracker.getStatus(mn.id) == Status::Revoked);
    tracker.activateMasternode(mn.id); // never from Revoked
    CHECK(tracker.getStatus(mn.id) == Status::Revoked);

    /* Revoke straight from Registered (exit path for a registration that never activated). */
    const RegisteredMn fresh = registerRandom(tracker);
    tracker.revokeMasternode(fresh.id, 500);
    CHECK(tracker.getStatus(fresh.id) == Status::Revoked);

    /* Deactivate only from Active, Penalize only from Active/Inactive (tracker guards). */
    const RegisteredMn other = registerRandom(tracker);
    tracker.deactivateMasternode(other.id, 600);
    CHECK(tracker.getStatus(other.id) == Status::Registered);
    tracker.penalizeMasternode(other.id, 600);
    CHECK(tracker.getStatus(other.id) == Status::Registered);
}

static void testTrackerAntiReplayCounters()
{
    Tracker tracker;
    const RegisteredMn mn = registerRandom(tracker);
    tracker.activateMasternode(mn.id);

    CHECK(tracker.getLastLifecycleHeight(mn.id) == 0);
    tracker.recordLifecycleHeight(mn.id, 50);
    tracker.recordLifecycleHeight(mn.id, 40); // never goes backwards
    CHECK(tracker.getLastLifecycleHeight(mn.id) == 50);

    CHECK(tracker.getLastHeartbeatPayloadHeight(mn.id) == 0);
    tracker.recordHealthSample(mn.id, 100, true, 101);
    CHECK(tracker.getLastHeartbeatPayloadHeight(mn.id) == 101);
    tracker.recordHealthSample(mn.id, 105, true, 99); // inclusion later but payload older: keep max
    CHECK(tracker.getLastHeartbeatPayloadHeight(mn.id) == 101);
    CHECK(!tracker.canAcceptHeartbeat(mn.id, 105 + parameters::MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL - 1));
    CHECK(tracker.canAcceptHeartbeat(mn.id, 105 + parameters::MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL));

    const KeyPairT v1 = makeKeys();
    const KeyPairT v2 = makeKeys();
    CHECK(tracker.getLastAttestationPayloadHeight(mn.id, v1.pub) == 0);
    tracker.recordAttestationSample(mn.id, 200, v1.pub, true, 201);
    tracker.recordAttestationSample(mn.id, 260, v2.pub, true, 259);
    CHECK(tracker.getLastAttestationPayloadHeight(mn.id, v1.pub) == 201);
    CHECK(tracker.getLastAttestationPayloadHeight(mn.id, v2.pub) == 259);
    CHECK(!tracker.canAcceptAttestation(mn.id, v1.pub, 200 + parameters::MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER - 1));
    CHECK(tracker.canAcceptAttestation(mn.id, v1.pub, 200 + parameters::MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER));

    /* Revoke + re-register keeps the counters (old signed payloads stay dead). */
    tracker.revokeMasternode(mn.id, 300);
    tracker.recordLifecycleHeight(mn.id, 301);
    tracker.registerMasternode(
        mn.id, mn.payout.pub, mn.view.pub, true, parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT, randomHash(), 9999,
        parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT, 7, randomKeyImage(), makeKeys().pub, randomHash(), true,
        mn.signing.pub);
    CHECK(tracker.getStatus(mn.id) == Status::Registered);
    CHECK(tracker.getLastLifecycleHeight(mn.id) == 301);
    CHECK(tracker.getLastHeartbeatPayloadHeight(mn.id) == 101);
    CHECK(tracker.getLastAttestationPayloadHeight(mn.id, v1.pub) == 201);
}

static void testTrackerSpendLock()
{
    Tracker tracker;
    const uint32_t lock = static_cast<uint32_t>(parameters::MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS);

    const RegisteredMn a = registerRandom(tracker);
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 10));
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 10'000'000)); // Registered: forever
    CHECK(tracker.hasCollateralKeyImage(a.collateralKeyImage, 10));
    CHECK(tracker.hasEndpointCommitment(a.endpoint, 10));
    CHECK(tracker.hasCollateralOutpoint(parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT, 42, 10));
    CHECK(tracker.hasUsedRegistrationToken(a.token));
    CHECK(!tracker.isCollateralKeyImageSpendLocked(randomKeyImage(), 10));

    tracker.activateMasternode(a.id);
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 10'000'000)); // Active: forever

    tracker.deactivateMasternode(a.id, 1000);
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 1000 + lock - 1));
    CHECK(!tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 1000 + lock));

    tracker.activateMasternode(a.id); // re-activation re-locks
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 1000 + lock + 5));

    tracker.revokeMasternode(a.id, 2000);
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 2000 + lock - 1));
    CHECK(!tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 2000 + lock));
    CHECK(tracker.hasCollateralKeyImage(a.collateralKeyImage, 2000 + lock - 1));
    CHECK(!tracker.hasCollateralKeyImage(a.collateralKeyImage, 2000 + lock));
    CHECK(!tracker.hasEndpointCommitment(a.endpoint, 2000 + lock));

    /* Two records sharing one key image: an expired Revoked one and a live Registered one.
     * Whatever the unordered_map iteration order, the key image must be reported locked. */
    const Crypto::Hash otherId = randomHash();
    tracker.registerMasternode(
        otherId, makeKeys().pub, makeKeys().pub, true, parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT, randomHash(),
        9999, parameters::MASTERNODE_COLLATERAL_LOCK_AMOUNT, 42, a.collateralKeyImage, a.collateralOutputKey,
        randomHash(), true, makeKeys().pub);
    CHECK(tracker.isCollateralKeyImageSpendLocked(a.collateralKeyImage, 2000 + lock + 10));
    CHECK(tracker.hasCollateralKeyImage(a.collateralKeyImage, 2000 + lock + 10));
}

static void testTrackerHealthAndEligibility()
{
    Tracker tracker;
    const uint32_t height = 5'300'000;

    const RegisteredMn mn = registerRandom(tracker);
    tracker.activateMasternode(mn.id);

    /* 20/20 healthy -> 100% */
    for (uint32_t i = 0; i < 20; ++i)
    {
        tracker.recordHealthSample(mn.id, height - 100 + i * 5, true, height - 100 + i * 5);
    }
    CHECK(tracker.getHealthPercent(mn.id, height) == 100);
    CHECK(tracker.meetsHealthThreshold(mn.id, height));

    /* Not eligible without attestations while MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION. */
    const std::vector<Crypto::Hash> candidates {mn.id};
    if (parameters::MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION)
    {
        CHECK(!tracker.meetsAttestationThreshold(mn.id, height));
        CHECK(tracker.filterRewardEligible(candidates, height).empty());
    }

    const KeyPairT verifier = makeKeys();
    for (uint64_t i = 0; i < parameters::MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW; ++i)
    {
        const uint32_t h = static_cast<uint32_t>(height - 2000 + i * 60);
        tracker.recordAttestationSample(mn.id, h, verifier.pub, true, h);
    }
    CHECK(tracker.meetsAttestationThreshold(mn.id, height));
    CHECK(tracker.filterRewardEligible(candidates, height).size() == 1);

    /* 2 unhealthy out of 22 -> 90% < 95% -> not eligible */
    tracker.recordHealthSample(mn.id, height - 2, false, height - 2);
    tracker.recordHealthSample(mn.id, height - 1, false, height - 1);
    CHECK(tracker.getHealthPercent(mn.id, height) == 90);
    CHECK(!tracker.meetsHealthThreshold(mn.id, height));
    CHECK(tracker.filterRewardEligible(candidates, height).empty());

    /* Samples age out of the window. */
    CHECK(tracker.getHealthPercent(mn.id, height + static_cast<uint32_t>(parameters::MASTERNODE_HEALTH_WINDOW_BLOCKS) + 10) == 0);

    /* Only Active nodes are candidates. */
    tracker.deactivateMasternode(mn.id, height);
    CHECK(tracker.filterRewardEligible(candidates, height).empty());
}

static void testTrackerFairness()
{
    Tracker tracker;
    const uint32_t height = 5'300'000;

    RegisteredMn a = registerRandom(tracker);
    RegisteredMn b = registerRandom(tracker);
    RegisteredMn c = registerRandom(tracker);
    makeEligible(tracker, a, height);
    makeEligible(tracker, b, height);
    makeEligible(tracker, c, height);

    const std::vector<Crypto::Hash> candidates {a.id, b.id, c.id};
    CHECK(tracker.filterRewardEligible(candidates, height).size() == 3);

    const uint64_t totalReward = 1'000'003; // not a multiple of 100
    auto d1 = tracker.calculateRewardDistribution(totalReward, height, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(d1.hasMasternodeWinner && d1.masternodeWinner.has_value());
    CHECK(d1.masternodeReward == (totalReward * parameters::MASTERNODE_REWARD_PERCENT) / 100);
    CHECK(d1.masternodeReward + d1.powReward == totalReward);

    /* Never-paid nodes win first, tie broken by smallest id. */
    std::vector<Crypto::Hash> sorted = candidates;
    std::sort(sorted.begin(), sorted.end(), [](const Crypto::Hash &l, const Crypto::Hash &r) {
        return std::lexicographical_compare(std::begin(l.data), std::end(l.data), std::begin(r.data), std::end(r.data));
    });
    CHECK(*d1.masternodeWinner == sorted[0]);

    /* Deterministic: same state, same answer. */
    auto d1again = tracker.calculateRewardDistribution(totalReward, height, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(d1again.masternodeWinner == d1.masternodeWinner);

    /* After paying the winner, the next winner is a different (still never-paid) node. */
    tracker.recordReward(*d1.masternodeWinner, height, d1.masternodeReward);
    auto d2 = tracker.calculateRewardDistribution(totalReward, height + 1, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(d2.hasMasternodeWinner && *d2.masternodeWinner != *d1.masternodeWinner);
    tracker.recordReward(*d2.masternodeWinner, height + 1, d2.masternodeReward);
    auto d3 = tracker.calculateRewardDistribution(totalReward, height + 2, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(d3.hasMasternodeWinner && *d3.masternodeWinner != *d1.masternodeWinner && *d3.masternodeWinner != *d2.masternodeWinner);
    tracker.recordReward(*d3.masternodeWinner, height + 2, d3.masternodeReward);

    /* Everyone paid once: lowest cumulative reward wins, i.e. the rotation continues with the first one. */
    auto d4 = tracker.calculateRewardDistribution(totalReward, height + 3, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(d4.hasMasternodeWinner && *d4.masternodeWinner == *d1.masternodeWinner);
    CHECK(tracker.getRewardAmountInFairnessWindow(*d1.masternodeWinner, height + 3) == d1.masternodeReward);

    /* Large totals do not overflow the split. */
    const uint64_t huge = std::numeric_limits<uint64_t>::max() - 7;
    auto dHuge = tracker.calculateRewardDistribution(huge, height + 4, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(dHuge.hasMasternodeWinner);
    CHECK(dHuge.masternodeReward == (huge / 100) * parameters::MASTERNODE_REWARD_PERCENT
                                       + ((huge % 100) * parameters::MASTERNODE_REWARD_PERCENT) / 100);
    CHECK(dHuge.masternodeReward + dHuge.powReward == huge);

    /* A share that rounds to zero means no masternode output (miner keeps everything). */
    auto dZero = tracker.calculateRewardDistribution(1, height + 5, candidates, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(!dZero.hasMasternodeWinner && dZero.powReward == 1 && dZero.masternodeReward == 0);

    /* No candidates -> miner keeps everything. */
    auto dNone = tracker.calculateRewardDistribution(totalReward, height, {}, parameters::MASTERNODE_REWARD_PERCENT);
    CHECK(!dNone.hasMasternodeWinner && dNone.powReward == totalReward);
}

static void testTrackerJson()
{
    Tracker tracker;
    const uint32_t height = 5'300'000;
    RegisteredMn a = registerRandom(tracker);
    RegisteredMn b = registerRandom(tracker);
    makeEligible(tracker, a, height);
    tracker.recordLifecycleHeight(a.id, 77);
    tracker.recordReward(a.id, height, 12345);
    tracker.deactivateMasternode(a.id, height + 1);
    tracker.updateEndpointCommitment(b.id, randomHash(), height);

    const nlohmann::json j = tracker.toJson();
    CHECK(j.contains("version") && j.contains("masternodes"));

    /* The masternode array is emitted in unordered_map order, which may legitimately differ
     * between two trackers holding the same records — compare the sorted-by-id form. */
    const auto normalize = [](nlohmann::json json) {
        auto &arr = json["masternodes"];
        std::sort(arr.begin(), arr.end(), [](const nlohmann::json &l, const nlohmann::json &r) {
            return l.at("id").get<std::string>() < r.at("id").get<std::string>();
        });
        return json;
    };

    Tracker restored;
    CHECK(restored.fromJson(j));
    CHECK(normalize(restored.toJson()) == normalize(j));
    CHECK(restored.getStatus(a.id) == Status::Inactive);
    CHECK(restored.getLastLifecycleHeight(a.id) == 77);
    CHECK(restored.getLastHeartbeatPayloadHeight(a.id) == tracker.getLastHeartbeatPayloadHeight(a.id));
    CHECK(restored.getRewardAmountInFairnessWindow(a.id, height) == 12345);
    Crypto::PublicKey spend, view;
    CHECK(restored.getPayoutAddressKeys(a.id, spend, view) && view == a.view.pub);
    CHECK(restored.isCollateralKeyImageSpendLocked(a.collateralKeyImage, height + 2));

    /* Missing payout view key (pre-v3 snapshot) must be rejected. */
    nlohmann::json missingView = j;
    missingView["masternodes"][0].erase("payout_view_key");
    Tracker t2;
    CHECK(!t2.fromJson(missingView));

    /* Unknown status value must be rejected. */
    nlohmann::json badStatus = j;
    badStatus["masternodes"][0]["status"] = 9;
    Tracker t3;
    CHECK(!t3.fromJson(badStatus));

    /* Garbage must be rejected. */
    Tracker t4;
    CHECK(!t4.fromJson(nlohmann::json("not an object")));
}

/* ------------------------------------------------------------------------------------------ */
/* 3. Reward output derivation                                                                 */
/* ------------------------------------------------------------------------------------------ */

static void testRewardDerivation()
{
    const Crypto::Hash prev = randomHash();
    const Crypto::SecretKey r1 = MasternodeReward::deriveCoinbaseTxSecretKey(5'200'000, prev);
    const Crypto::SecretKey r1again = MasternodeReward::deriveCoinbaseTxSecretKey(5'200'000, prev);
    const Crypto::SecretKey r2 = MasternodeReward::deriveCoinbaseTxSecretKey(5'200'001, prev);
    const Crypto::SecretKey r3 = MasternodeReward::deriveCoinbaseTxSecretKey(5'200'000, randomHash());
    CHECK(bytesEqual(r1.data, r1again.data, 32));
    CHECK(!bytesEqual(r1.data, r2.data, 32));
    CHECK(!bytesEqual(r1.data, r3.data, 32));

    /* The derived scalar is a valid secret key. */
    Crypto::PublicKey R;
    CHECK(Crypto::secret_key_to_public_key(r1, R));

    /* Operator wallet: (a, A) view, (b, B) spend. Consensus derives P from (A, B, r); the wallet
     * must be able to recognise P from (R, a) and identify B, exactly like a normal output. */
    const KeyPairT view = makeKeys();
    const KeyPairT spend = makeKeys();
    Crypto::PublicKey P;
    CHECK(MasternodeReward::deriveRewardOutputKey(spend.pub, view.pub, r1, 1, P));

    Crypto::KeyDerivation walletDerivation;
    CHECK(Crypto::generate_key_derivation(R, view.sec, walletDerivation));
    Crypto::PublicKey walletExpected;
    CHECK(Crypto::derive_public_key(walletDerivation, 1, spend.pub, walletExpected));
    CHECK(walletExpected == P);
    Crypto::PublicKey underived;
    CHECK(Crypto::underive_public_key(walletDerivation, 1, P, underived));
    CHECK(underived == spend.pub);

    /* The wallet can spend it: the derived secret matches P. */
    Crypto::SecretKey x;
    Crypto::derive_secret_key(walletDerivation, 1, spend.sec, x);
    Crypto::PublicKey xPub;
    CHECK(Crypto::secret_key_to_public_key(x, xPub) && xPub == P);

    /* Different height => different one-time key (and therefore a different key image). */
    Crypto::PublicKey P2;
    CHECK(MasternodeReward::deriveRewardOutputKey(spend.pub, view.pub, r2, 1, P2));
    CHECK(!(P2 == P));

    /* Output index matters (index 0 is the miner's slot). */
    Crypto::PublicKey P0;
    CHECK(MasternodeReward::deriveRewardOutputKey(spend.pub, view.pub, r1, 0, P0));
    CHECK(!(P0 == P));

    /* Raw payout key is never the output key (the bug this design replaces). */
    CHECK(!(P == spend.pub));
}

/* ------------------------------------------------------------------------------------------ */
/* 4. ChainLockManager                                                                         */
/* ------------------------------------------------------------------------------------------ */

namespace
{
    ChainLockVote makeVote(uint32_t height, const Crypto::Hash &blockHash, const Crypto::Hash &mnId, const KeyPairT &signing)
    {
        ChainLockVote v;
        v.height = height;
        v.blockHash = blockHash;
        v.masternodeId = mnId;
        v.signingKey = signing.pub;
        const auto preimage = ChainLockManager::buildVotePreimage(height, blockHash);
        const Crypto::Hash h = Crypto::cn_fast_hash(preimage.data(), preimage.size());
        Crypto::generate_signature(h, signing.pub, signing.sec, v.signature);
        return v;
    }
} // namespace

static void testChainLockManager()
{
    ChainLockManager mgr;
    const uint64_t threshold = 3;
    const uint64_t maxPending = 10;
    const std::time_t now = 1'700'000'000;
    const uint32_t H = 5'000'010;
    const Crypto::Hash block = randomHash();
    const Crypto::Hash otherBlock = randomHash();

    std::vector<Crypto::Hash> ids;
    std::vector<KeyPairT> keys;
    for (int i = 0; i < 5; ++i)
    {
        ids.push_back(randomHash());
        keys.push_back(makeKeys());
    }

    CHECK(mgr.addVote(makeVote(H, block, ids[0], keys[0]), threshold, maxPending, now) == MasternodeVoteResult::Added);
    /* same vote again */
    CHECK(mgr.addVote(makeVote(H, block, ids[0], keys[0]), threshold, maxPending, now) == MasternodeVoteResult::Duplicate);
    /* same masternode, different hash at the same height = equivocation -> not added, not relayed */
    CHECK(mgr.addVote(makeVote(H, otherBlock, ids[0], keys[0]), threshold, maxPending, now) == MasternodeVoteResult::Duplicate);
    /* bad signature (signed with another key) */
    {
        ChainLockVote forged = makeVote(H, block, ids[1], keys[2]);
        forged.signingKey = keys[1].pub;
        CHECK(mgr.addVote(forged, threshold, maxPending, now) == MasternodeVoteResult::Rejected);
    }
    CHECK(mgr.addVote(makeVote(H, block, ids[1], keys[1]), threshold, maxPending, now) == MasternodeVoteResult::Added);
    CHECK(!mgr.hasLock(H));
    CHECK(mgr.addVote(makeVote(H, block, ids[2], keys[2]), threshold, maxPending, now) == MasternodeVoteResult::Assembled);
    CHECK(mgr.hasLock(H));
    CHECK(mgr.highestLockedHeight() == H);
    const auto lock = mgr.getLock(H);
    CHECK(lock.has_value() && lock->votes.size() == 3 && lock->blockHash == block);
    /* late vote for a locked height */
    CHECK(mgr.addVote(makeVote(H, block, ids[3], keys[3]), threshold, maxPending, now) == MasternodeVoteResult::Duplicate);

    /* conflict handling + pending expiry valve */
    CHECK(!mgr.isConflict(H, block, now, 3600));
    CHECK(mgr.isConflict(H, otherBlock, now, 3600));
    CHECK(mgr.isConflict(H, otherBlock, now + 3600, 3600));
    CHECK(!mgr.isConflict(H, otherBlock, now + 3601, 3600)); // pending too long: advisory only
    mgr.markLockSatisfied(H);                                // the block arrived: permanent
    CHECK(mgr.isConflict(H, otherBlock, now + 999'999, 3600));
    CHECK(!mgr.isConflict(H + 1, otherBlock, now, 3600)); // no lock there

    /* assembled lock ingestion: distinct voters, valid signatures */
    {
        const uint32_t H2 = H + 1;
        const Crypto::Hash b2 = randomHash();
        ChainLock cl;
        cl.height = H2;
        cl.blockHash = b2;
        cl.votes = {makeVote(H2, b2, ids[0], keys[0]), makeVote(H2, b2, ids[1], keys[1]), makeVote(H2, b2, ids[2], keys[2])};
        CHECK(mgr.addChainLock(cl, threshold, now));
        CHECK(mgr.hasLock(H2));
        CHECK(!mgr.addChainLock(cl, threshold, now)); // already locked

        ChainLock dup;
        dup.height = H2 + 1;
        dup.blockHash = b2;
        dup.votes = {makeVote(H2 + 1, b2, ids[0], keys[0]), makeVote(H2 + 1, b2, ids[0], keys[0]), makeVote(H2 + 1, b2, ids[1], keys[1])};
        CHECK(!mgr.addChainLock(dup, threshold, now)); // duplicate voter

        ChainLock few;
        few.height = H2 + 2;
        few.blockHash = b2;
        few.votes = {makeVote(H2 + 2, b2, ids[0], keys[0])};
        CHECK(!mgr.addChainLock(few, threshold, now)); // below threshold

        ChainLock mismatch;
        mismatch.height = H2 + 3;
        mismatch.blockHash = b2;
        mismatch.votes = {makeVote(H2 + 3, b2, ids[0], keys[0]), makeVote(H2 + 3, randomHash(), ids[1], keys[1]), makeVote(H2 + 3, b2, ids[2], keys[2])};
        CHECK(!mgr.addChainLock(mismatch, threshold, now)); // vote for a different hash
    }

    /* pending cap */
    {
        ChainLockManager small;
        const uint32_t H3 = 123;
        const Crypto::Hash b3 = randomHash();
        CHECK(small.addVote(makeVote(H3, b3, ids[0], keys[0]), 10, 2, now) == MasternodeVoteResult::Added);
        CHECK(small.addVote(makeVote(H3, b3, ids[1], keys[1]), 10, 2, now) == MasternodeVoteResult::Added);
        CHECK(small.addVote(makeVote(H3, b3, ids[2], keys[2]), 10, 2, now) == MasternodeVoteResult::Rejected);
    }

    /* persistence round trip (restored locks are "received now") */
    {
        const std::string json = mgr.toJson();
        ChainLockManager restored;
        CHECK(restored.fromJson(json, now + 5));
        CHECK(restored.hasLock(H) && restored.hasLock(H + 1));
        CHECK(restored.getLock(H)->votes.size() == 3);
        CHECK(restored.isConflict(H, otherBlock, now + 5, 3600));
        CHECK(!restored.isConflict(H, otherBlock, now + 5 + 3601, 3600));
        CHECK(!restored.fromJson("definitely not json", now));
    }

    /* pruning */
    mgr.pruneLocksBelow(H + 1);
    CHECK(!mgr.hasLock(H) && mgr.hasLock(H + 1));
    mgr.removeAbove(H);
    CHECK(!mgr.hasLock(H + 1));
    CHECK(mgr.highestLockedHeight() == 0);
}

/* ------------------------------------------------------------------------------------------ */
/* 5. InstantSendManager                                                                       */
/* ------------------------------------------------------------------------------------------ */

namespace
{
    InstantSendVote makeIsVote(const Crypto::Hash &txHash, const Crypto::Hash &mnId, const KeyPairT &signing)
    {
        InstantSendVote v;
        v.txHash = txHash;
        v.masternodeId = mnId;
        v.signingKey = signing.pub;
        const auto preimage = InstantSendManager::buildVotePreimage(txHash);
        const Crypto::Hash h = Crypto::cn_fast_hash(preimage.data(), preimage.size());
        Crypto::generate_signature(h, signing.pub, signing.sec, v.signature);
        return v;
    }
} // namespace

static void testInstantSendManager()
{
    InstantSendManager mgr;
    const uint64_t threshold = 2;
    const uint32_t height = 5'000'100;

    std::vector<Crypto::Hash> ids;
    std::vector<KeyPairT> keys;
    for (int i = 0; i < 4; ++i)
    {
        ids.push_back(randomHash());
        keys.push_back(makeKeys());
    }

    const Crypto::Hash txA = randomHash();
    const Crypto::Hash txB = randomHash();
    const std::vector<Crypto::KeyImage> kisA {randomKeyImage(), randomKeyImage()};
    const std::vector<Crypto::KeyImage> kisB {kisA[0], randomKeyImage()}; // B double-spends A's first input

    CHECK(mgr.addVote(makeIsVote(txA, ids[0], keys[0]), {}, threshold, height) == MasternodeVoteResult::Rejected); // no key images
    CHECK(mgr.addVote(makeIsVote(txA, ids[0], keys[0]), kisA, threshold, height) == MasternodeVoteResult::Added);
    CHECK(mgr.addVote(makeIsVote(txA, ids[0], keys[0]), kisA, threshold, height) == MasternodeVoteResult::Duplicate);
    {
        InstantSendVote forged = makeIsVote(txA, ids[1], keys[2]);
        forged.signingKey = keys[1].pub;
        CHECK(mgr.addVote(forged, kisA, threshold, height) == MasternodeVoteResult::Rejected);
    }
    CHECK(!mgr.isLocked(kisA[0]));
    CHECK(mgr.addVote(makeIsVote(txA, ids[1], keys[1]), kisA, threshold, height) == MasternodeVoteResult::Assembled);
    CHECK(mgr.isLocked(kisA[0]) && mgr.isLocked(kisA[1]));
    CHECK(!mgr.isConflict(kisA[0], txA));
    CHECK(mgr.isConflict(kisA[0], txB));
    const auto byTx = mgr.getLockByTxHash(txA);
    CHECK(byTx.has_value() && byTx->keyImages.size() == 2 && byTx->votes.size() == 2 && byTx->lockedAtHeight == height);
    CHECK(mgr.getLock(kisA[1]).has_value());
    CHECK(mgr.addVote(makeIsVote(txA, ids[2], keys[2]), kisA, threshold, height) == MasternodeVoteResult::Duplicate);

    /* A conflicting tx cannot gather votes while A is locked. */
    CHECK(mgr.addVote(makeIsVote(txB, ids[2], keys[2]), kisB, threshold, height) == MasternodeVoteResult::Rejected);

    /* Assembled lock ingestion. */
    {
        const Crypto::Hash txC = randomHash();
        InstantSendLock lock;
        lock.txHash = txC;
        lock.keyImages = {randomKeyImage()};
        lock.votes = {makeIsVote(txC, ids[0], keys[0]), makeIsVote(txC, ids[1], keys[1])};
        lock.lockedAtHeight = height;
        CHECK(mgr.addInstantSendLock(lock, threshold));
        CHECK(!mgr.addInstantSendLock(lock, threshold)); // already known -> not re-stored / re-relayed

        InstantSendLock conflicting = lock;
        conflicting.txHash = randomHash();
        conflicting.votes = {makeIsVote(conflicting.txHash, ids[0], keys[0]), makeIsVote(conflicting.txHash, ids[1], keys[1])};
        CHECK(!mgr.addInstantSendLock(conflicting, threshold)); // key image locked to another tx

        InstantSendLock empty = lock;
        empty.txHash = randomHash();
        empty.keyImages.clear();
        CHECK(!mgr.addInstantSendLock(empty, threshold));

        InstantSendLock dupVoter = lock;
        dupVoter.txHash = randomHash();
        dupVoter.keyImages = {randomKeyImage()};
        dupVoter.votes = {makeIsVote(dupVoter.txHash, ids[0], keys[0]), makeIsVote(dupVoter.txHash, ids[0], keys[0])};
        CHECK(!mgr.addInstantSendLock(dupVoter, threshold));
    }

    /* Chain is authoritative: a spent key image drops the whole lock covering it. */
    mgr.onKeyImageSpent(kisA[1]);
    CHECK(!mgr.isLocked(kisA[0]) && !mgr.isLocked(kisA[1]));
    CHECK(!mgr.getLockByTxHash(txA).has_value());

    /* Expiry. */
    {
        InstantSendManager m2;
        const Crypto::Hash txD = randomHash();
        const std::vector<Crypto::KeyImage> kisD {randomKeyImage()};
        CHECK(m2.addVote(makeIsVote(txD, ids[0], keys[0]), kisD, threshold, height) == MasternodeVoteResult::Added);
        CHECK(m2.addVote(makeIsVote(txD, ids[1], keys[1]), kisD, threshold, height) == MasternodeVoteResult::Assembled);
        m2.pruneExpired(height + static_cast<uint32_t>(parameters::INSTANTSEND_LOCK_EXPIRY_BLOCKS), parameters::INSTANTSEND_LOCK_EXPIRY_BLOCKS);
        CHECK(m2.isLocked(kisD[0]));
        m2.pruneExpired(height + static_cast<uint32_t>(parameters::INSTANTSEND_LOCK_EXPIRY_BLOCKS) + 1, parameters::INSTANTSEND_LOCK_EXPIRY_BLOCKS);
        CHECK(!m2.isLocked(kisD[0]));

        /* confirmation clears too */
        CHECK(m2.addVote(makeIsVote(txD, ids[0], keys[0]), kisD, threshold, height) == MasternodeVoteResult::Added);
        CHECK(m2.addVote(makeIsVote(txD, ids[1], keys[1]), kisD, threshold, height) == MasternodeVoteResult::Assembled);
        m2.onTxConfirmed(txD);
        CHECK(!m2.isLocked(kisD[0]));

        /* persistence round trip */
        CHECK(m2.addVote(makeIsVote(txD, ids[0], keys[0]), kisD, threshold, height) == MasternodeVoteResult::Added);
        CHECK(m2.addVote(makeIsVote(txD, ids[1], keys[1]), kisD, threshold, height) == MasternodeVoteResult::Assembled);
        InstantSendManager restored;
        CHECK(restored.fromJson(m2.toJson()));
        CHECK(restored.isLocked(kisD[0]) && restored.getLockByTxHash(txD).has_value());
        CHECK(!restored.fromJson("nope"));
    }
}

/* ------------------------------------------------------------------------------------------ */
/* 6. MasternodeQuorum                                                                         */
/* ------------------------------------------------------------------------------------------ */

static void testQuorumSelection()
{
    std::vector<Crypto::Hash> activeSet;
    for (int i = 0; i < 50; ++i)
    {
        activeSet.push_back(randomHash());
    }
    const Crypto::Hash seedA = randomHash();
    const Crypto::Hash seedB = randomHash();

    const auto qA = MasternodeQuorum::selectQuorum(activeSet, seedA, parameters::CHAINLOCK_QUORUM_SIZE);
    const auto qAagain = MasternodeQuorum::selectQuorum(activeSet, seedA, parameters::CHAINLOCK_QUORUM_SIZE);
    const auto qB = MasternodeQuorum::selectQuorum(activeSet, seedB, parameters::CHAINLOCK_QUORUM_SIZE);

    CHECK(qA.size() == parameters::CHAINLOCK_QUORUM_SIZE);
    CHECK(qA == qAagain);
    CHECK(qA != qB); // astronomically unlikely to be equal
    for (const auto &id : qA)
    {
        CHECK(MasternodeQuorum::isInQuorum(id, qA));
        CHECK(std::find(activeSet.begin(), activeSet.end(), id) != activeSet.end());
    }
    CHECK(!MasternodeQuorum::isInQuorum(randomHash(), qA));

    /* Input order does not matter. */
    std::vector<Crypto::Hash> shuffled(activeSet.rbegin(), activeSet.rend());
    CHECK(MasternodeQuorum::selectQuorum(shuffled, seedA, parameters::CHAINLOCK_QUORUM_SIZE) == qA);

    /* Smaller sets are capped at the set size; empty set -> empty quorum. */
    const std::vector<Crypto::Hash> five(activeSet.begin(), activeSet.begin() + 5);
    CHECK(MasternodeQuorum::selectQuorum(five, seedA, parameters::CHAINLOCK_QUORUM_SIZE).size() == 5);
    CHECK(MasternodeQuorum::selectQuorum({}, seedA, parameters::CHAINLOCK_QUORUM_SIZE).empty());
}

/* ------------------------------------------------------------------------------------------ */

int main()
{
    std::cout << "masternodetest" << std::endl;

    runTest("payload sizes", testPayloadSizes);
    runTest("register payload round trip", testRegisterRoundTrip);
    runTest("signed payload round trips", testSignedPayloadRoundTrips);
    runTest("payload rejections", testPayloadRejections);
    runTest("tracker lifecycle", testTrackerLifecycle);
    runTest("tracker anti-replay counters", testTrackerAntiReplayCounters);
    runTest("tracker spend lock", testTrackerSpendLock);
    runTest("tracker health and eligibility", testTrackerHealthAndEligibility);
    runTest("tracker fairness", testTrackerFairness);
    runTest("tracker json", testTrackerJson);
    runTest("reward derivation", testRewardDerivation);
    runTest("chainlock manager", testChainLockManager);
    runTest("instantsend manager", testInstantSendManager);
    runTest("quorum selection", testQuorumSelection);

    std::cout << std::endl
              << g_checks << " checks, " << g_failures << " failure(s)" << std::endl;

    return g_failures == 0 ? 0 : 1;
}
