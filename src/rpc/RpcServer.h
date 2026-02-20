// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "httplib.h"
#include "JsonHelper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <cryptonotecore/Core.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandlerCommon.h>
#include <errors/Errors.h>
#include <p2p/NetNode.h>

enum class RpcMode
{
    Standard = 0,
    Explorer = 1,
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
        const std::string corsHeader,
        const std::string rpcAccessToken,
        const uint32_t rpcReadTimeout,
        const uint32_t rpcWriteTimeout,
        const uint64_t rpcMaxRequestBodyBytes,
        const uint32_t rpcMaxRequestsPerMinute,
        const uint32_t rpcMaxGlobalIndexesRange,
        const uint32_t rpcMaxBlockCount,
        const bool rpcTrustProxy,
        const std::string feeAddress,
        const uint64_t feeAmount,
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

    /* Gets the IP/port combo the server is running on */
    std::tuple<std::string, uint16_t> getConnectionInfo();

  private:
    //////////////////////////////
    /* Private member functions */
    //////////////////////////////

    /* Starts listening for requests on the server */
    void listen();

    std::optional<rapidjson::Document> getJsonBody(
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
        std::function<std::tuple<Error, uint16_t>(
            const httplib::Request &req,
            httplib::Response &res,
            const rapidjson::Document &body)> handler);

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
        info(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        fee(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        height(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        peers(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    ///////////////////
    /* POST REQUESTS */
    ///////////////////

    std::tuple<Error, uint16_t>
        sendTransaction(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRandomOuts(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getWalletSyncData(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getGlobalIndexes(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        queryBlocksLite(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getTransactionsStatus(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getPoolChanges(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        queryBlocksDetailed(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    /* Deprecated. Use getGlobalIndexes instead. */
    std::tuple<Error, uint16_t>
        getGlobalIndexesDeprecated(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRawBlocks(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    ///////////////////////
    /* JSON RPC REQUESTS */
    ///////////////////////

    std::tuple<Error, uint16_t>
        getBlockTemplate(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        submitBlock(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockCount(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockHashForHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getLastBlockHeader(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockHeaderByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockHeaderByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlocksByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockDetailsByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getTransactionDetailsByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getTransactionsInPool(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    /* Our server instance */
    httplib::Server m_server;

    /* The server host */
    const std::string m_host;

    /* The server port */
    const uint16_t m_port;

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

    /* The thread running the server */
    std::thread m_serverThread;

    /* The address to return from the /fee endpoint */
    const std::string m_feeAddress;

    /* The amount to return from the /fee endpoint */
    const uint64_t m_feeAmount;

    /* RPC methods that are enabled */
    const RpcMode m_rpcMode;

    /* A pointer to our CryptoNoteCore instance */
    const std::shared_ptr<CryptoNote::Core> m_core;

    /* A pointer to our P2P stack */
    const std::shared_ptr<CryptoNote::NodeServer> m_p2p;

    const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> m_syncManager;

    std::mutex m_rateLimitMutex;
    std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> m_rateLimitByIp;
};
