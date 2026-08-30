// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "WalletTypes.h"
#include "httplib_fwd.h"
#include "json.hpp"

#include <atomic>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>
#include <logger/Logger.h>
#include <memory>
#include <mutex>
#include <optional>
#include <rpc/CoreRpcServerCommandsDefinitions.h>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/* What one wallet sync request came back with. */
struct WalletSyncResponse
{
    bool success = false;

    std::vector<WalletTypes::WalletBlockInfo> blocks;

    std::optional<WalletTypes::TopBlock> topBlock;

    /* The highest height the daemon looked at, which is not the same as the
       highest block it sent - the ones in between held nothing worth sending.
       Zero when the daemon did not say, either because it predates the field
       or because it could not vouch for a whole window. A caller must then
       advance only as far as the last block it actually received. */
    uint64_t scannedToHeight = 0;
};

/* What one random-outputs request came back with. */
struct RandomOutsResponse
{
    /* The daemon answered and `outs` is usable. It may still hold fewer outputs
       per amount than were asked for - the caller decides whether that is
       enough for the ring it wants to build. */
    bool success = false;

    /* The daemon answered, but told us it cannot supply decoys at these
       denominations. Kept apart from a transport failure on purpose: the funds
       are fine and the node is fine, only the requested ring size is out of
       reach, and a caller that conflates the two reports the wrong cause and
       cannot retry smaller. Only set by daemons old enough to reject the whole
       request rather than return what they have. */
    bool notEnoughOutputs = false;

    /* The daemon's own explanation, when notEnoughOutputs is set. */
    std::string error;

    std::vector<CryptoNote::RandomOuts> outs;
};

class Nigel
{
  public:
    /////////////////////////
    /* Public Constructors */
    /////////////////////////

    Nigel(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL);

    Nigel(
        const std::string daemonHost,
        const uint16_t daemonPort,
        const bool daemonSSL,
        const std::chrono::seconds timeout);

    ~Nigel();

    /////////////////////////////
    /* Public member functions */
    /////////////////////////////

    void init(bool startBackgroundThread = true);

    /* Manually refresh daemon info (block count, peers, fees).
       Called by syncStep() in single-threaded WASM mode instead of background thread. */
    void refreshInfo();

    void swapNode(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL);

    void decreaseRequestedBlockCount();

    void resetRequestedBlockCount();

    /* Whether the last sync request came back rate limited (HTTP 429) */
    bool lastRequestWasRateLimited() const;

    /* How many blocks we are currently asking for per request */
    uint64_t requestedBlockCount() const;

    /* Returns whether we've received info from the daemon at some point */
    bool isOnline() const;

    uint64_t localDaemonBlockCount() const;

    uint64_t networkBlockCount() const;

    uint64_t peerCount() const;

    uint64_t hashrate() const;

    std::tuple<uint64_t, std::string> nodeFee() const;

    std::tuple<std::string, uint16_t, bool> nodeAddress() const;

    /* Shuts the sockets of any request currently in flight, so a thread
       blocked waiting for a daemon to answer errors out now rather than when
       the read timeout expires. Stopping a wallet has to join those threads,
       and without this it would have to wait out the timeout of whatever
       request happened to be running.

       Safe to call from another thread while requests are running - that is
       the only thing httplib guarantees here. The clients recover on their
       next use, so this is not a teardown. */
    void abortInFlightRequests();

    /* Whether the daemon lets us bound a request to a height window we chose
       and tells us how far it looked, which is what makes fetching several
       windows at once safe rather than a guess about where each ends. */
    bool daemonSupportsHeightRange() const;

    WalletSyncResponse getWalletSyncData(
        const std::vector<Crypto::Hash> blockHashCheckpoints,
        const uint64_t startHeight,
        const uint64_t startTimestamp,
        const bool skipCoinbaseTransactions);

    /* Fetches the half open height window [startHeight, endHeight). Uses its
       own connection, indexed by slot, so several of these can be in flight at
       once - one httplib client is one socket and cannot be shared.

       Unlike getWalletSyncData this never retries or falls back; a failure is
       reported so the caller can go back to the sequential path, which is the
       one that can recover from a fork. */
    WalletSyncResponse getWalletSyncDataRange(
        const size_t slot,
        const uint64_t startHeight,
        const uint64_t endHeight,
        const bool skipCoinbaseTransactions);

    /* Returns a bool on success or not */
    bool getTransactionsStatus(
        const std::unordered_set<Crypto::Hash> transactionHashes,
        std::unordered_set<Crypto::Hash> &transactionsInPool,
        std::unordered_set<Crypto::Hash> &transactionsInBlock,
        std::unordered_set<Crypto::Hash> &transactionsUnknown) const;

    RandomOutsResponse
        getRandomOutsByAmounts(const std::vector<uint64_t> amounts, const uint64_t requestedOuts) const;

    /* {success, connectionError, errorMessage} */
    std::tuple<bool, bool, std::string> sendTransaction(const CryptoNote::Transaction tx) const;

    std::tuple<bool, std::unordered_map<Crypto::Hash, std::vector<uint64_t>>>
        getGlobalIndexesForRange(const uint64_t startHeight, const uint64_t endHeight) const;

  private:
    //////////////////////////////
    /* Private member functions */
    //////////////////////////////

    void stop();

    void backgroundRefresh();

    bool getDaemonInfo();

    /* Validates the HTTP result, parses the JSON body and (optionally) checks
       the "status":"OK" field. Lives in Nigel.cpp so this header does not need
       the full httplib.h. */
    std::optional<nlohmann::json>
        parseJSONResponse(const httplib::Result &res, const std::string &failMessage, const bool verifyStatus) const;

    template<typename F>
    auto tryParseJSONResponse(
        const httplib::Result &res,
        const std::string &failMessage,
        const F parseFunc,
        const bool verifyStatus = true) const -> std::optional<decltype(parseFunc(nlohmann::json()))>
    {
        const auto j = parseJSONResponse(res, failMessage, verifyStatus);

        if (!j)
        {
            return std::nullopt;
        }

        try
        {
            return parseFunc(*j);
        }
        catch (const nlohmann::json::exception &e)
        {
            Logger::logger.log(
                failMessage + ": " + std::string(e.what()),
                Logger::INFO,
                { Logger::SYNC, Logger::DAEMON }
            );

            return std::nullopt;
        }
    }

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    /* Stores our http client (Don't really care about it launching threads
       and making our functions non const) */
    std::shared_ptr<httplib::Client> m_nodeClient = nullptr;

    /* One connection per parallel range fetch slot. An httplib client owns a
       single socket, so sharing one between concurrent requests would
       interleave two conversations down the same pipe. Created on first use so
       a wallet that never fetches in parallel never opens them. */
    std::vector<std::shared_ptr<httplib::Client>> m_rangeClients;

    std::mutex m_rangeClientMutex;

    std::shared_ptr<httplib::Client> rangeClient(const size_t slot);

    /* Stores the HTTP headers included in all Nigel requests */
    std::vector<std::pair<std::string, std::string>> m_requestHeaders;

    /* Runs a background refresh on height, hashrate, etc */
    std::thread m_backgroundThread;

    /* If we should stop the background thread */
    std::atomic<bool> m_shouldStop = false;

    /* Stores how many blocks we'll try to sync */
    std::atomic<uint64_t> m_blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;

    /* The largest blockCount this daemon has been observed to accept. Starts
       optimistic and is lowered if the daemon rejects a request as exceeding
       its --rpc-max-block-count. */
    std::atomic<uint64_t> m_maxBlockCount = WalletConfig::maxBlocksPerSyncRequest;

    /* Set when the last sync request was rejected with HTTP 429. Shrinking the
       batch in response to rate limiting is counterproductive - it takes more
       requests to move the same blocks - so the downloader waits instead. */
    std::atomic<bool> m_lastRequestRateLimited = false;

    /* The amount of blocks the daemon we're connected to has */
    std::atomic<uint64_t> m_localDaemonBlockCount = 0;

    /* The amount of blocks the network has */
    std::atomic<uint64_t> m_networkBlockCount = 0;

    /* The amount of peers we're connected to */
    std::atomic<uint64_t> m_peerCount = 0;

    /* The hashrate (based on the last local block the daemon has synced) */
    std::atomic<uint64_t> m_lastKnownHashrate = 0;

    /* Whether the daemon is a blockchain cache API
       see: https://github.com/TurtlePay/blockchain-cache-api */
    std::atomic<bool> m_isBlockchainCache = false;

    /* Optional sync behaviours, each switched on only once the daemon has
       named it in /info. Left off for a daemon that advertises nothing, which
       is how every release before these existed behaves. */
    std::atomic<bool> m_daemonSkipsEmptyBlocks = false;

    std::atomic<bool> m_daemonSpeaksBase64 = false;

    std::atomic<bool> m_daemonSupportsHeightRange = false;

    /* The compression warning is worth saying once, not every ten seconds for
       as long as the wallet is open. */
    std::atomic<bool> m_warnedAboutCompression = false;

    /* The address to send the node fee to (May be "") */
    std::string m_nodeFeeAddress;

    /* The fee the node charges */
    uint64_t m_nodeFeeAmount = 0;

    /* The timeout on requests */
    std::chrono::seconds m_timeout;

    /* The daemon hostname */
    std::string m_daemonHost;

    /* The daemon port */
    uint16_t m_daemonPort;

    /* If the daemon is SSL */
    bool m_daemonSSL = false;

    /* Whether we should use /getrawblocks instead of /getwalletsyncdata */
    bool m_useRawBlocks = false;
};
