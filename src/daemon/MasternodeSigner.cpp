// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeSigner.h"

#include <algorithm>
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
    // Derive public key from private key.
    Crypto::secret_key_to_public_key(privateKey, publicKey);
    return true;
}

MasternodeSigner::MasternodeSigner(
    CryptoNote::Core &core,
    CryptoNote::ICryptoNoteProtocolHandler &protocol,
    const Crypto::SecretKey &signingPrivateKey,
    const Crypto::PublicKey &signingPublicKey,
    const Crypto::Hash &masternodeId,
    const Crypto::SecretKey &payoutPrivateKey,
    const Crypto::PublicKey &payoutPublicKey,
    bool hasPayoutKey):
    m_core(core),
    m_protocol(protocol),
    m_signingPrivateKey(signingPrivateKey),
    m_signingPublicKey(signingPublicKey),
    m_masternodeId(masternodeId),
    m_payoutPrivateKey(payoutPrivateKey),
    m_payoutPublicKey(payoutPublicKey),
    m_hasPayoutKey(hasPayoutKey)
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
    if (m_hasPayoutKey)
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
            try
            {
                const uint32_t height = item.height;
                const Crypto::Hash &blockHash = item.blockHash;

                // Check if already locked.
                if (m_core.hasChainLock(height))
                {
                    continue;
                }

                // Get active set and compute quorum for this block.
                const auto activeSet = m_core.getActiveMasternodeSet(height);
                const auto quorum = CryptoNote::MasternodeQuorum::selectQuorum(
                    activeSet,
                    blockHash,
                    CryptoNote::parameters::CHAINLOCK_QUORUM_SIZE);

                if (!CryptoNote::MasternodeQuorum::isInQuorum(m_masternodeId, quorum))
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
    // MN_BASE_PAYLOAD_SIZE = 4 (MN01) + 1 (type) + 32 (mnId) = 37
    // MN_HEARTBEAT_PAYLOAD_SIZE = 37 + 1 (flag) + 64 (sig) = 102
    constexpr size_t MN_BASE_PAYLOAD_SIZE = 37;
    constexpr size_t MN_HEARTBEAT_PAYLOAD_SIZE = MN_BASE_PAYLOAD_SIZE + 1 + sizeof(Crypto::Signature);
    // Nonce contains: 0x7f (arb-data tag) + varint(102) = 0x66 + 102 bytes payload = 104 bytes
    constexpr uint8_t NONCE_SIZE = static_cast<uint8_t>(1 + 1 + MN_HEARTBEAT_PAYLOAD_SIZE); // 104

    uint32_t lastHeartbeatHeight = 0;

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
        if (height < lastHeartbeatHeight + CryptoNote::parameters::MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL)
        {
            continue;
        }

        try
        {
            // Check the masternode is currently Active.
            const auto activeSet = m_core.getActiveMasternodeSet(height);
            const bool isActive = std::find(activeSet.begin(), activeSet.end(), m_masternodeId) != activeSet.end();
            if (!isActive)
            {
                continue;
            }

            // Build unsigned heartbeat payload: MN01 | 0x06 | masternodeId (32) | healthy (1)
            std::vector<uint8_t> unsignedPayload;
            unsignedPayload.reserve(MN_BASE_PAYLOAD_SIZE + 1);
            unsignedPayload.push_back('M');
            unsignedPayload.push_back('N');
            unsignedPayload.push_back('0');
            unsignedPayload.push_back('1');
            unsignedPayload.push_back(static_cast<uint8_t>(CryptoNote::MasternodeTxType::Heartbeat));
            unsignedPayload.insert(
                unsignedPayload.end(), m_masternodeId.data, m_masternodeId.data + sizeof(m_masternodeId.data));
            unsignedPayload.push_back(0x01); // healthy = true

            // Sign with payout private key.
            const Crypto::Hash sigHash = Crypto::cn_fast_hash(unsignedPayload.data(), unsignedPayload.size());
            Crypto::Signature sig;
            Crypto::generate_signature(sigHash, m_payoutPublicKey, m_payoutPrivateKey, sig);

            // Full MN01 heartbeat payload (102 bytes).
            std::vector<uint8_t> mnPayload = unsignedPayload;
            mnPayload.insert(mnPayload.end(), sig.data, sig.data + sizeof(sig.data));

            // Build the transaction extra field:
            //   [0x02][varint(104)][0x7f][varint(102)][102 bytes payload]
            // Payload (102) and nonce (104) both fit in one varint byte (< 128).
            std::vector<uint8_t> extra;
            extra.push_back(Constants::TX_EXTRA_NONCE_IDENTIFIER);  // 0x02
            extra.push_back(NONCE_SIZE);                             // 104 = 0x68
            extra.push_back(Constants::TX_EXTRA_ARBITRARY_DATA_IDENTIFIER); // 0x7f
            extra.push_back(static_cast<uint8_t>(MN_HEARTBEAT_PAYLOAD_SIZE)); // 102 = 0x66
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
            if (ok)
            {
                lastHeartbeatHeight = height;
            }
            // Suppress pool-already-exists failures (expected after restart).
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
                const uint32_t height = m_core.getTopBlockIndex();
                const auto activeSet = m_core.getActiveMasternodeSet(height);

                // Qualify: need at least INSTANTSEND_QUORUM_SIZE active MNs.
                if (activeSet.size() < CryptoNote::parameters::INSTANTSEND_QUORUM_SIZE)
                {
                    continue;
                }

                const auto quorum = CryptoNote::MasternodeQuorum::selectQuorum(
                    activeSet,
                    txHash,
                    CryptoNote::parameters::INSTANTSEND_QUORUM_SIZE);

                if (!CryptoNote::MasternodeQuorum::isInQuorum(m_masternodeId, quorum))
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
