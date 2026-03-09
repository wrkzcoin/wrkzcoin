// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "MasternodeSigner.h"

#include <common/StringTools.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/crypto.h>
#include <cryptonotecore/ChainLockManager.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/InstantSendManager.h>
#include <cryptonotecore/MasternodeQuorum.h>
#include <cryptonoteprotocol/CryptoNoteProtocolDefinitions.h>
#include <cryptonoteprotocol/ICryptoNoteProtocolHandler.h>
#include <p2p/LevinProtocol.h>

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
    const Crypto::Hash &masternodeId):
    m_core(core),
    m_protocol(protocol),
    m_signingPrivateKey(signingPrivateKey),
    m_signingPublicKey(signingPublicKey),
    m_masternodeId(masternodeId)
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
}

void MasternodeSigner::stop()
{
    m_running = false;
    m_blockCv.notify_all();
    m_txCv.notify_all();
    if (m_clThread.joinable())
    {
        m_clThread.join();
    }
    if (m_isThread.joinable())
    {
        m_isThread.join();
    }
}

void MasternodeSigner::onNewBlock(uint32_t height, const Crypto::Hash &blockHash)
{
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        m_pendingBlocks.push_back({height, blockHash});
    }
    m_blockCv.notify_one();
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
                if (rawTx.inputs.size() > CryptoNote::parameters::INSTANTSEND_MAX_INPUTS)
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
