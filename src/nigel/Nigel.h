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
#include <optional>
#include <rpc/CoreRpcServerCommandsDefinitions.h>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

    std::tuple<bool, std::vector<WalletTypes::WalletBlockInfo>, std::optional<WalletTypes::TopBlock>> getWalletSyncData(
        const std::vector<Crypto::Hash> blockHashCheckpoints,
        const uint64_t startHeight,
        const uint64_t startTimestamp,
        const bool skipCoinbaseTransactions);

    /* Returns a bool on success or not */
    bool getTransactionsStatus(
        const std::unordered_set<Crypto::Hash> transactionHashes,
        std::unordered_set<Crypto::Hash> &transactionsInPool,
        std::unordered_set<Crypto::Hash> &transactionsInBlock,
        std::unordered_set<Crypto::Hash> &transactionsUnknown) const;

    std::tuple<bool, std::vector<CryptoNote::RandomOuts>>
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
