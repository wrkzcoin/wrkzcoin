// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace CryptoNote
{
    class Core;
    class ICryptoNoteProtocolHandler;
}

// MasternodeSigner runs two background threads:
//   - A ChainLock signing thread: woken on every new block tip, signs and broadcasts
//     NOTIFY_CHAINLOCK_VOTE if this node's registered MN is in the CL quorum.
//   - An InstantSend signing thread: woken on every new mempool TX, signs and broadcasts
//     NOTIFY_INSTANTSEND_VOTE if this node's MN is in the IS quorum for that TX.
//
// It requires a signing private key configured via --mn-signing-key.
// The corresponding public key must have been registered on-chain in the MN Register tx.
class MasternodeSigner
{
  public:
    MasternodeSigner(
        CryptoNote::Core &core,
        CryptoNote::ICryptoNoteProtocolHandler &protocol,
        const Crypto::SecretKey &signingPrivateKey,
        const Crypto::PublicKey &signingPublicKey,
        const Crypto::Hash &masternodeId);

    ~MasternodeSigner();

    // Start background threads.
    void start();

    // Signal threads to stop and join.
    void stop();

    // Called by the daemon when a new block is added to the main chain.
    void onNewBlock(uint32_t height, const Crypto::Hash &blockHash);

    // Called by the daemon when a new transaction arrives in the pool.
    void onNewTransaction(const Crypto::Hash &txHash);

  private:
    void chainLockLoop();
    void instantSendLoop();

    CryptoNote::Core &m_core;
    CryptoNote::ICryptoNoteProtocolHandler &m_protocol;
    Crypto::SecretKey m_signingPrivateKey;
    Crypto::PublicKey m_signingPublicKey;
    Crypto::Hash m_masternodeId;

    std::atomic<bool> m_running {false};

    // ChainLock work queue: pending (height, blockHash) pairs.
    struct BlockWork
    {
        uint32_t height;
        Crypto::Hash blockHash;
    };
    std::vector<BlockWork> m_pendingBlocks;
    std::mutex m_blockMutex;
    std::condition_variable m_blockCv;

    // InstantSend work queue: pending txHashes.
    std::vector<Crypto::Hash> m_pendingTxs;
    std::mutex m_txMutex;
    std::condition_variable m_txCv;

    std::thread m_clThread;
    std::thread m_isThread;
};

// Parse a hex-encoded signing key and return the keypair.
// Returns false if the hex is invalid or not 64 bytes (private key size).
bool parseMnSigningKey(
    const std::string &hexKey,
    Crypto::SecretKey &privateKey,
    Crypto::PublicKey &publicKey);
