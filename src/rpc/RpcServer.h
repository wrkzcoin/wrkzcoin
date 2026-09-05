// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "httplib_fwd.h"
#include "JsonHelper.h"
#include "json_fwd.hpp"

#include <cryptonotecore/Core.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandlerCommon.h>
#include <errors/Errors.h>
#include <p2p/NetNode.h>

enum class RpcMode
{
    Standard = 0,
    Explorer = 1,
};

/* Everything that decides the bytes of a wallet sync response. The start index
   is the one the core resolved from the caller's checkpoints, not the
   checkpoints themselves - two wallets at the same height send different
   checkpoint tails but want the identical answer, and keying on the resolved
   index is what lets them share it. */
struct WalletSyncCacheKey
{
    uint64_t startIndex = 0;

    uint64_t blockCount = 0;

    uint64_t endHeight = 0;

    bool skipCoinbaseTransactions = false;

    bool skipInputKeyOffsets = false;

    bool skipEmptyBlocks = false;

    bool base64 = false;

    bool operator==(const WalletSyncCacheKey &other) const
    {
        return startIndex == other.startIndex && blockCount == other.blockCount && endHeight == other.endHeight
               && skipCoinbaseTransactions == other.skipCoinbaseTransactions
               && skipInputKeyOffsets == other.skipInputKeyOffsets && skipEmptyBlocks == other.skipEmptyBlocks
               && base64 == other.base64;
    }
};

struct WalletSyncCacheKeyHash
{
    std::size_t operator()(const WalletSyncCacheKey &key) const
    {
        std::size_t seed = std::hash<uint64_t>()(key.startIndex);

        const auto mix = [&seed](const std::size_t value) {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        };

        mix(std::hash<uint64_t>()(key.blockCount));

        mix(std::hash<uint64_t>()(key.endHeight));

        mix(static_cast<std::size_t>(key.skipCoinbaseTransactions) | (static_cast<std::size_t>(key.skipInputKeyOffsets) << 1)
            | (static_cast<std::size_t>(key.skipEmptyBlocks) << 2) | (static_cast<std::size_t>(key.base64) << 3));

        return seed;
    }
};

class RpcServer
{
  public:

    ////////////////////////////////
    /* Constructors / Destructors */
    ////////////////////////////////
    RpcServer(
        const uint16_t bindPort,
        const std::string rpcBindIp,
        const std::string rpcBindIpv6Address,
        const bool rpcUseIpv6,
        const std::string corsHeader,
        const std::string rpcAccessToken,
        const uint32_t rpcReadTimeout,
        const uint32_t rpcWriteTimeout,
        const uint64_t rpcMaxRequestBodyBytes,
        const uint32_t rpcMaxRequestsPerMinute,
        const uint32_t rpcMaxGlobalIndexesRange,
        const uint32_t rpcMaxBlockCount,
        const uint64_t rpcSyncCacheBytes,
        const bool rpcTrustProxy,
        const std::string rpcIpcPath,
        const uint32_t rpcIpcMode,
        const std::string rpcIpcGroup,
        const bool rpcIpcRequireToken,
        const RpcMode rpcMode,
        const std::shared_ptr<CryptoNote::Core> core,
        const std::shared_ptr<CryptoNote::NodeServer> p2p,
        const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> syncManager);

    ~RpcServer();

    /////////////////////////////
    /* Public member functions */
    /////////////////////////////

    /* Starts the server. */
    void start();

    /* Stops the server. */
    void stop();

    /* Which content encoding this build can compress responses with, or
       "none". Decided at compile time by which of httplib's compression
       backends were available, so it is the same answer for every request
       and every server this process runs. */
    static const char *compressionAlgorithm();

    /* Gets the IP/port combo the server is running on */
    std::tuple<std::string, uint16_t> getConnectionInfo();

    /* The local socket RPC is being served on, or empty if IPC is disabled or
       failed to bind. Only meaningful after start(). */
    std::string getIpcPath() const;

    /* Runs one daemon console command line and returns what it printed. The
       daemon installs this once its command handler exists, which is after
       start() because the handler needs to know which listeners came up;
       until then the route answers 503. Only ever reachable over the IPC
       socket - see setupRoutes. */
    using ConsoleExecutor = std::function<std::string(const std::string &commandLine)>;

    void setConsoleExecutor(ConsoleExecutor executor);

  private:
    //////////////////////////////
    /* Private member functions */
    //////////////////////////////

    /* Starts listening for requests on the server (IPv4) */
    void listen();

    /* Starts listening for requests on the server (IPv6) */
    void listenIpv6();

    /* Serves the already bound local IPC socket */
    void listenIpc();

    /* Registers all HTTP routes on the given server instance. isIpc marks the
       local socket listener, whose callers are vouched for by the kernel
       rather than by an address or a token. */
    void setupRoutes(httplib::Server &srv, const bool isIpc);

    std::optional<nlohmann::json> getJsonBody(
        const httplib::Request &req,
        httplib::Response &res,
        const bool bodyRequired);

    /* Handles stuff like parsing json and then forwards onto the handler */
    void middleware(
        const httplib::Request &req,
        httplib::Response &res,
        const RpcMode routePermissions,
        const bool bodyRequired,
        const bool syncRequired,
        const bool isIpc,
        std::function<std::tuple<Error, uint16_t>(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body)> handler);

    void failRequest(uint16_t statusCode, std::string body, httplib::Response &res);

    void failJsonRpcRequest(
        const int64_t errorCode,
        const std::string errorMessage,
        httplib::Response &res);

    std::string getClientIp(const httplib::Request &req) const;

    bool isRateLimited(const std::string &clientIp);

    /////////////////////
    /* OPTION REQUESTS */
    /////////////////////

    void handleOptions(const httplib::Request &req, httplib::Response &res) const;

    //////////////////
    /* GET REQUESTS */
    //////////////////

    std::tuple<Error, uint16_t>
        info(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        height(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        peers(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    ///////////////////
    /* POST REQUESTS */
    ///////////////////

    /* IPC only. Runs a daemon console command through the installed executor. */
    std::tuple<Error, uint16_t>
        console(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        sendTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getRandomOuts(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getWalletSyncData(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getGlobalIndexes(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        queryBlocksLite(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getTransactionsStatus(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getPoolChanges(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        queryBlocksDetailed(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Deprecated. Use getGlobalIndexes instead. */
    std::tuple<Error, uint16_t>
        getGlobalIndexesDeprecated(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getRawBlocks(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    ///////////////////////
    /* JSON RPC REQUESTS */
    ///////////////////////

    std::tuple<Error, uint16_t>
        getBlockTemplate(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        submitBlock(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getBlockCount(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getBlockHashForHeight(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getLastBlockHeader(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getBlockHeaderByHash(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getBlockHeaderByHeight(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getBlocksByHeight(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getBlockDetailsByHash(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getTransactionDetailsByHash(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        getTransactionsInPool(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Looks up the transactions carrying a given payment ID. Only long
       plaintext payment IDs can be found this way - the index is built from
       them alone, and a short payment ID is encrypted to its receiver so the
       same one looks different in every transaction it appears in. */
    std::tuple<Error, uint16_t>
        getTransactionHashesByPaymentId(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    /* Our IPv4 server instance */
    std::unique_ptr<httplib::Server> m_server;

    /* Our IPv6 server instance (only used when m_ipv6Host is non-empty) */
    std::unique_ptr<httplib::Server> m_ipv6Server;

    /* Our local IPC server instance (only used when m_ipcPath is non-empty) */
    std::unique_ptr<httplib::Server> m_ipcServer;

    /* The server host (IPv4) */
    const std::string m_host;

    /* The server port */
    const uint16_t m_port;

    /* The IPv6 bind address (empty = IPv6 disabled) */
    const std::string m_ipv6Host;

    /* The header to use with 'Access-Control-Allow-Origin'. If empty string,
     * header is not added. */
    const std::string m_corsHeader;

    const std::string m_rpcAccessToken;

    const uint32_t m_rpcReadTimeout;

    const uint32_t m_rpcWriteTimeout;

    const uint64_t m_rpcMaxRequestBodyBytes;

    const uint32_t m_rpcMaxRequestsPerMinute;

    const uint32_t m_rpcMaxGlobalIndexesRange;

    const uint32_t m_rpcMaxBlockCount;

    const bool m_rpcTrustProxy;

    /* The local socket to accept RPC on (empty = IPC disabled) */
    const std::string m_ipcPath;

    /* Permission bits the socket file is created with */
    const uint32_t m_ipcMode;

    /* Optional group to hand the socket file to, for a 0660 setup */
    const std::string m_ipcGroup;

    /* Whether --rpc-access-token is still demanded of IPC callers. Off by
       default: the socket's mode already decides who may connect, and the
       kernel enforces it where a shared secret only asks politely. */
    const bool m_ipcRequireToken;

    /* The thread running the IPv4 server */
    std::thread m_serverThread;

    /* The thread running the IPv6 server (only used when m_ipv6Host is non-empty) */
    std::thread m_ipv6Thread;

    /* The thread running the IPC server (only used when m_ipcPath is non-empty) */
    std::thread m_ipcThread;

    /* Set once the IPC socket is bound, so shutdown knows to unlink it */
    bool m_ipcBound = false;

    /* Installed after start() from the daemon thread while the listeners are
       already serving, so every reader takes a copy under the lock. */
    std::mutex m_consoleExecutorMutex;

    ConsoleExecutor m_consoleExecutor;

    /* RPC methods that are enabled */
    const RpcMode m_rpcMode;

    /* A pointer to our CryptoNoteCore instance */
    const std::shared_ptr<CryptoNote::Core> m_core;

    /* A pointer to our P2P stack */
    const std::shared_ptr<CryptoNote::NodeServer> m_p2p;

    const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> m_syncManager;

    std::mutex m_rateLimitMutex;
    std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> m_rateLimitByIp;

    /* The window m_rateLimitByIp holds counts for. When the window rolls over
       the map is cleared rather than left to accumulate an entry per address
       seen since the node started. */
    uint64_t m_rateLimitWindowStart = 0;

    ////////////////////////////
    /* WALLET SYNC BODY CACHE */
    ////////////////////////////

    /* Every wallet syncing past a given height asks for the same range with
       the same flags, and building one response is thousands of database
       reads, a full reassembly and the encoding of the result. Keeping the
       finished bodies means a node serving many wallets does that work once
       per range rather than once per wallet. */
    std::mutex m_syncCacheMutex;

    /* Most recently used at the front. Bodies are large and variable, so the
       cache is bounded by their total size rather than a count. */
    std::list<std::pair<WalletSyncCacheKey, std::string>> m_syncCacheEntries;

    std::unordered_map<
        WalletSyncCacheKey,
        std::list<std::pair<WalletSyncCacheKey, std::string>>::iterator,
        WalletSyncCacheKeyHash>
        m_syncCacheIndex;

    uint64_t m_syncCacheBytes = 0;

    const uint64_t m_syncCacheMaxBytes;

    /* The highest top block index seen while serving. A drop means the chain
       reorganised, at which point anything cached may describe blocks that are
       no longer on the main chain. */
    uint64_t m_syncCacheTopBlockIndex = 0;

    std::optional<std::string> lookupSyncCache(const WalletSyncCacheKey &key);

    void storeSyncCache(
        const WalletSyncCacheKey &key,
        const std::string &body,
        const std::vector<WalletTypes::WalletBlockInfo> &blocks,
        const uint64_t topBlockIndex);

    void discardSyncCacheOnReorg(const uint64_t topBlockIndex);
};
