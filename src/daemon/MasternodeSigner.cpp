// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeSigner.h"

#include <algorithm>
#include <cstdio>
#include <common/StringTools.h>
#include <config/Constants.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/crypto.h>
#include <cryptonotecore/ChainLockManager.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/InstantSendManager.h>
#include <cryptonotecore/MasternodeQuorum.h>
#include <cryptonotecore/MasternodeTx.h>
#include <cryptonoteprotocol/CryptoNoteProtocolDefinitions.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandlerCommon.h>
#include <p2p/LevinProtocol.h>
#include <serialization/SerializationTools.h>

bool parseMnSigningKey(
    const std::string &hexKey,
    Crypto::SecretKey &privateKey,
    Crypto::PublicKey &publicKey)
{
    if (hexKey.size() != sizeof(Crypto::SecretKey) * 2)
    {
        return false;
    }
    if (!Common::podFromHex(hexKey, privateKey))
    {
        return false;
    }
    // Derive public key from private key (fails for a non-canonical / out-of-range scalar).
    return Crypto::secret_key_to_public_key(privateKey, publicKey);
}

MasternodeSigner::MasternodeSigner(
    CryptoNote::Core &core,
    CryptoNote::ICryptoNoteProtocolHandler &protocol,
    const Crypto::SecretKey &signingPrivateKey,
    const Crypto::PublicKey &signingPublicKey,
    const Crypto::Hash &masternodeId,
    const Crypto::SecretKey &operatorPrivateKey,
    const Crypto::PublicKey &operatorPublicKey,
    bool hasOperatorKey):
    m_core(core),
    m_protocol(protocol),
    m_signingPrivateKey(signingPrivateKey),
    m_signingPublicKey(signingPublicKey),
    m_masternodeId(masternodeId),
    m_operatorPrivateKey(operatorPrivateKey),
    m_operatorPublicKey(operatorPublicKey),
    m_hasOperatorKey(hasOperatorKey)
{
}

MasternodeSigner::~MasternodeSigner()
{
    stop();
}

void MasternodeSigner::start()
{
    m_running = true;
    m_clThread = std::thread(&MasternodeSigner::chainLockLoop, this);
    m_isThread = std::thread(&MasternodeSigner::instantSendLoop, this);
    if (m_hasOperatorKey)
    {
        m_heartbeatThread = std::thread(&MasternodeSigner::heartbeatLoop, this);
    }
}

void MasternodeSigner::stop()
{
    m_running = false;
    m_blockCv.notify_all();
    m_txCv.notify_all();
    {
        std::lock_guard<std::mutex> lock(m_heartbeatMutex);
        m_heartbeatNewBlock = true;
    }
    m_heartbeatCv.notify_all();
    if (m_clThread.joinable())
    {
        m_clThread.join();
    }
    if (m_isThread.joinable())
    {
        m_isThread.join();
    }
    if (m_heartbeatThread.joinable())
    {
        m_heartbeatThread.join();
    }
}

void MasternodeSigner::onNewBlock(uint32_t height, const Crypto::Hash &blockHash)
{
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        m_pendingBlocks.push_back({height, blockHash});
    }
    m_blockCv.notify_one();

    m_latestBlockHeight.store(height);
    {
        std::lock_guard<std::mutex> lock(m_heartbeatMutex);
        m_heartbeatNewBlock = true;
    }
    m_heartbeatCv.notify_one();
}

void MasternodeSigner::onNewTransaction(const Crypto::Hash &txHash)
{
    {
        std::lock_guard<std::mutex> lock(m_txMutex);
        m_pendingTxs.push_back(txHash);
    }
    m_txCv.notify_one();
}

void MasternodeSigner::chainLockLoop()
{
    // Track block hashes signed this session to avoid redundant re-broadcasts
    // when relay fails mid-operation. Signatures are deterministic so re-signing
    // is safe, but re-broadcasting the same vote is unnecessary network noise.
    std::vector<Crypto::Hash> alreadySigned;

    while (m_running)
    {
        std::vector<BlockWork> work;
        {
            std::unique_lock<std::mutex> lock(m_blockMutex);
            m_blockCv.wait(lock, [this] { return !m_running || !m_pendingBlocks.empty(); });
            work.swap(m_pendingBlocks);
        }

        if (!m_running)
        {
            break;
        }

        for (const auto &item : work)
        {
            // Skip if we already successfully relayed a vote for this block hash.
            if (std::find(alreadySigned.begin(), alreadySigned.end(), item.blockHash) != alreadySigned.end())
            {
                continue;
            }

            try
            {
                const uint32_t height = item.height;
                const Crypto::Hash &blockHash = item.blockHash;

                // Never vote while catching up: blocks pushed during initial sync are historical
                // and the rest of the network has long moved on (and the vote window rejects them).
                if (!m_protocol.isSynchronized())
                {
                    continue;
                }

                // Check if already locked.
                if (m_core.hasChainLock(height))
                {
                    alreadySigned.push_back(blockHash);
                    continue;
                }

                // Quorum for this height (seeded by block height-1, computed by Core so that the
                // signer and the validators always agree).
                const auto quorum = m_core.getChainLockQuorum(height);
                if (!quorum.has_value())
                {
                    continue;
                }

                // Need a full quorum of active MNs before ChainLocks are meaningful.
                if (quorum->size() < CryptoNote::parameters::CHAINLOCK_QUORUM_SIZE)
                {
                    continue;
                }

                if (!CryptoNote::MasternodeQuorum::isInQuorum(m_masternodeId, *quorum))
                {
                    continue; // not in quorum for this block
                }

                // Build and sign the vote.
                const auto preimage = CryptoNote::ChainLockManager::buildVotePreimage(height, blockHash);
                const Crypto::Hash sigHash = Crypto::cn_fast_hash(preimage.data(), preimage.size());

                CryptoNote::NOTIFY_CHAINLOCK_VOTE::request req;
                req.height = height;
                req.blockHash = blockHash;
                req.masternodeId = m_masternodeId;
                req.signingKey = m_signingPublicKey;
                Crypto::generate_signature(sigHash, m_signingPublicKey, m_signingPrivateKey, req.signature);

                // Feed into our own core (so we count our own vote).
                CryptoNote::ChainLockVote vote;
                vote.height = req.height;
                vote.blockHash = req.blockHash;
                vote.masternodeId = req.masternodeId;
                vote.signingKey = req.signingKey;
                vote.signature = req.signature;
                m_core.addChainLockVote(vote);

                // Broadcast to network.
                m_protocol.relayChainLockVote(req);

                // Mark as relayed — no need to re-broadcast for this block hash.
                alreadySigned.push_back(blockHash);
            }
            catch (...)
            {
                // Never crash the signer thread.
            }
        }
    }
}

void MasternodeSigner::heartbeatLoop()
{
    // Heartbeat payload layout / size comes from MasternodeTx.h (single source of truth).
    constexpr size_t MN_HEARTBEAT_PAYLOAD_SIZE = CryptoNote::MASTERNODE_HEARTBEAT_PAYLOAD_SIZE;
    // Nonce contains: 0x7f (arb-data tag) + varint(payload size) + payload bytes.
    static_assert(MN_HEARTBEAT_PAYLOAD_SIZE < 128, "heartbeat payload size must fit in a 1-byte varint");
    constexpr uint8_t NONCE_SIZE = static_cast<uint8_t>(1 + 1 + MN_HEARTBEAT_PAYLOAD_SIZE);
    static_assert(NONCE_SIZE < 128, "heartbeat nonce size must fit in a 1-byte varint");

    // Height of the last heartbeat *attempt* (accepted or not). Advancing it on every attempt keeps
    // a persistently-rejected heartbeat from being retried (and logged) on every single block.
    uint32_t lastHeartbeatAttemptHeight = 0;

    while (m_running)
    {
        {
            std::unique_lock<std::mutex> lock(m_heartbeatMutex);
            m_heartbeatCv.wait(lock, [this] { return !m_running || m_heartbeatNewBlock; });
            m_heartbeatNewBlock = false;
        }

        if (!m_running)
        {
            break;
        }

        const uint32_t height = m_latestBlockHeight.load();

        // Only submit every MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL blocks.
        if (height < lastHeartbeatAttemptHeight + CryptoNote::parameters::MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL)
        {
            continue;
        }

        try
        {
            // Don't heartbeat while syncing: the payload height would be stale by the time we catch up.
            if (!m_protocol.isSynchronized())
            {
                continue;
            }

            // Check the masternode is currently Active.
            const auto activeSet = m_core.getActiveMasternodeSet(height);
            const bool isActive = std::find(activeSet.begin(), activeSet.end(), m_masternodeId) != activeSet.end();
            if (!isActive)
            {
                continue;
            }

            lastHeartbeatAttemptHeight = height;

            // Build unsigned heartbeat payload: MN01 | 0x06 | masternodeId (32) | height (4) | healthy (1).
            // The creation height (next block height as we see it) makes the payload single-use.
            const uint32_t payloadHeight = height + 1;
            const std::vector<uint8_t> unsignedPayload =
                CryptoNote::buildMasternodeHeartbeatUnsignedPayload(m_masternodeId, payloadHeight, true);

            // Sign with the operator private key.
            const Crypto::Hash sigHash = Crypto::cn_fast_hash(unsignedPayload.data(), unsignedPayload.size());
            Crypto::Signature sig;
            Crypto::generate_signature(sigHash, m_operatorPublicKey, m_operatorPrivateKey, sig);

            // Full MN01 heartbeat payload.
            std::vector<uint8_t> mnPayload = unsignedPayload;
            mnPayload.insert(mnPayload.end(), sig.data, sig.data + sizeof(sig.data));
            if (mnPayload.size() != MN_HEARTBEAT_PAYLOAD_SIZE)
            {
                fprintf(stderr, "[mn-heartbeat] internal error: unexpected heartbeat payload size %zu\n", mnPayload.size());
                continue;
            }

            // Build the transaction extra field:
            //   [0x02][varint(nonce size)][0x7f][varint(payload size)][payload bytes]
            std::vector<uint8_t> extra;
            extra.push_back(Constants::TX_EXTRA_NONCE_IDENTIFIER);  // 0x02
            extra.push_back(NONCE_SIZE);
            extra.push_back(Constants::TX_EXTRA_ARBITRARY_DATA_IDENTIFIER); // 0x7f
            extra.push_back(static_cast<uint8_t>(MN_HEARTBEAT_PAYLOAD_SIZE));
            extra.insert(extra.end(), mnPayload.begin(), mnPayload.end());

            // Build the zero-input, zero-output transaction.
            CryptoNote::Transaction tx;
            tx.version = CryptoNote::CURRENT_TRANSACTION_VERSION;
            tx.unlockTime = 0;
            tx.inputs.clear();
            tx.outputs.clear();
            tx.extra = extra;

            const auto txBinary = CryptoNote::toBinaryArray(tx);
            const auto [ok, errMsg] = m_core.addTransactionToPool(txBinary);
            if (!ok && errMsg.find("already") == std::string::npos)
            {
                // Log unexpected rejections (suppress the expected "already in pool" on restart).
                fprintf(stderr,
                    "[mn-heartbeat] WARNING: heartbeat TX rejected at height %u: %s\n",
                    static_cast<unsigned>(height),
                    errMsg.c_str());
            }
        }
        catch (...)
        {
            // Never crash the heartbeat thread.
        }
    }
}

void MasternodeSigner::instantSendLoop()
{
    while (m_running)
    {
        std::vector<Crypto::Hash> work;
        {
            std::unique_lock<std::mutex> lock(m_txMutex);
            m_txCv.wait(lock, [this] { return !m_running || !m_pendingTxs.empty(); });
            work.swap(m_pendingTxs);
        }

        if (!m_running)
        {
            break;
        }

        for (const auto &txHash : work)
        {
            try
            {
                if (!m_protocol.isSynchronized())
                {
                    continue;
                }

                // Quorum for the current cycle (seeded by a block hash, computed by Core).
                const auto quorum = m_core.getInstantSendQuorum();
                if (!quorum.has_value())
                {
                    continue;
                }

                // Qualify: need a full quorum of active MNs.
                if (quorum->size() < CryptoNote::parameters::INSTANTSEND_QUORUM_SIZE)
                {
                    continue;
                }

                if (!CryptoNote::MasternodeQuorum::isInQuorum(m_masternodeId, *quorum))
                {
                    continue;
                }

                // Already locked?
                const auto [found, txData] = m_core.getPoolTransaction(txHash);
                if (!found)
                {
                    continue; // TX left the pool
                }

                // Check input count qualification.
                const auto rawTx = CryptoNote::Core::getRawTransaction(
                    std::vector<uint8_t>(txData.begin(), txData.end()));
                if (rawTx.keyInputs.size() > CryptoNote::parameters::INSTANTSEND_MAX_INPUTS)
                {
                    continue;
                }

                // Build and sign the vote.
                const auto preimage = CryptoNote::InstantSendManager::buildVotePreimage(txHash);
                const Crypto::Hash sigHash = Crypto::cn_fast_hash(preimage.data(), preimage.size());

                CryptoNote::NOTIFY_INSTANTSEND_VOTE::request req;
                req.txHash = txHash;
                req.masternodeId = m_masternodeId;
                req.signingKey = m_signingPublicKey;
                Crypto::generate_signature(sigHash, m_signingPublicKey, m_signingPrivateKey, req.signature);

                // Feed into our own core.
                CryptoNote::InstantSendVote vote;
                vote.txHash = req.txHash;
                vote.masternodeId = req.masternodeId;
                vote.signingKey = req.signingKey;
                vote.signature = req.signature;
                m_core.addInstantSendVote(vote);

                // Broadcast to network.
                m_protocol.relayInstantSendVote(req);
            }
            catch (...)
            {
                // Never crash the signer thread.
            }
        }
    }
}
