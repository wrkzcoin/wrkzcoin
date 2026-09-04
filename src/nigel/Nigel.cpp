// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

////////////////////////
#include <nigel/Nigel.h>

#include "httplib.h"
////////////////////////

#include <common/CryptoNoteTools.h>
#include <common/IpcSocket.h>
#include <common/PlatformCaCerts.h>
#include <config/CryptoNoteConfig.h>
#include <cryptonotecore/CachedBlock.h>
#include <cryptonotecore/Core.h>
#include <CryptoNote.h>
#include <errors/Errors.h>
#include <sstream>
#include <utilities/Utilities.h>
#include <version.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <cstdlib>
#endif
#include <cstdio>

using json = nlohmann::json;

////////////////////////////////
/*   Inline helper methods    */
////////////////////////////////

inline std::string daemonBaseUrl(const std::string &host, const uint16_t port, const bool ssl)
{
    const uint16_t defaultPort = ssl ? 443 : 80;
    const std::string portStr = (port == defaultPort) ? "" : (":" + std::to_string(port));

    // Support a base-path prefix in the host string, e.g. "example.com/daemon".
    // This lets a web wallet use a same-origin reverse proxy to avoid mixed content.
    std::string hostname = host;
    std::string basePath;
    const auto slashPos = host.find('/');
    if (slashPos != std::string::npos)
    {
        hostname = host.substr(0, slashPos);
        basePath = host.substr(slashPos);  // includes leading '/'
    }

    const bool needsIpv6Brackets = hostname.find(':') != std::string::npos && (hostname.empty() || hostname.front() != '[');
    const std::string formattedHost = needsIpv6Brackets ? ("[" + hostname + "]") : hostname;
    return std::string(ssl ? "https://" : "http://") + formattedHost + portStr + basePath;
}

/* Nigel stores its request headers without the httplib type so that Nigel.h
   does not need the full httplib.h; build the real Headers at the call site. */
inline httplib::Headers toHeaders(const std::vector<std::pair<std::string, std::string>> &headers)
{
    return httplib::Headers(headers.begin(), headers.end());
}

inline std::shared_ptr<httplib::Client> getClient(
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const std::chrono::seconds timeout)
{
    const bool useSsl =
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        daemonSSL;
#else
        false;
#endif

    /* A local socket is addressed by path, so it cannot go through the URL
       constructor - that would try to parse the path as a scheme and host.
       There is no TLS on a Unix socket either; the kernel already restricts
       who can open it. */
    const bool useIpc = Utilities::isIpcDaemonAddress(daemonHost);

    auto client = useIpc
        ? std::make_shared<httplib::Client>(Utilities::ipcDaemonPath(daemonHost), 80)
        : std::make_shared<httplib::Client>(daemonBaseUrl(daemonHost, daemonPort, useSsl));

    if (useIpc)
    {
        Common::Ipc::configureClient(*client);
    }
    else if (useSsl)
    {
        /* Android's trust store is not on any path cpp-httplib probes, and the
           miss is silent - see PlatformCaCerts.h. Without this an https daemon
           is unreachable from the mobile wallet while http works fine. */
        Common::applySystemCaCertificates(*client);
    }

    client->set_connection_timeout(timeout);

    /* Reaching a node and waiting for it to answer are different problems. An
     * unreachable node should fail fast, but a node that has accepted a large
     * sync request is going to take a while to build the response and there is
     * nothing to gain by hanging up on it partway - we would pay for the
     * request again, and it would build the answer either way. */
    const auto responseTimeout =
        std::max(timeout, std::chrono::seconds(WalletConfig::daemonResponseTimeoutSeconds));

    client->set_read_timeout(responseTimeout);
    client->set_write_timeout(responseTimeout);

    /* Syncing a wallet is tens of thousands of sequential requests to the same
     * daemon. Without this every one of them pays for a fresh TCP handshake,
     * and a fresh TLS handshake on top of that when talking to an SSL node. */
    client->set_keep_alive(true);

    /* Nagle delays the last partial segment of a write while an ack is
     * outstanding, which on a strict request/response loop like this one turns
     * into a per-request stall. */
    client->set_tcp_nodelay(true);

    return client;
}

#if defined(__EMSCRIPTEN__)
inline std::string daemonUrl(const std::string &host, const uint16_t port, const bool ssl, const std::string &path)
{
    return daemonBaseUrl(host, port, ssl) + path;
}

// Synchronous HTTP helper using JavaScript's synchronous XMLHttpRequest API.
//
// emscripten_fetch(EMSCRIPTEN_FETCH_SYNCHRONOUS) deadlocks on the WASM main
// thread because ccall() blocks the JS event loop: the XHR onload callback
// can never fire while WASM is executing synchronously.  pthreads avoid this
// because each thread has its own event loop, but wallet_wasm_request runs on
// the WASM main thread and needs to make HTTP calls (e.g. getRandomOutsByAmounts
// during prepareBasicTransfer).
//
// Synchronous XMLHttpRequest (xhr.open(..., false)) blocks the calling thread
// at the network level without the event loop, so it works correctly from any
// Web Worker context — including both the WASM main thread and pthreads.
// It is deprecated only on the browser's UI thread (freeze risk), not in workers.
//
// Returns a malloc'd body buffer that the caller must free(), or nullptr on
// network failure.  *out_status and *out_body_len are always set.
EM_JS(char*, wrkzSyncXhr, (const char* url, const char* method,
                            const char* body, int body_len,
                            int* out_status, int* out_body_len), {
    // In Emscripten pthreads builds with ALLOW_MEMORY_GROWTH, the module-level
    // HEAPU8 / HEAP32 typed-array views in a pthread worker can be stale: they
    // were created against the *original* SharedArrayBuffer length and are not
    // updated until the worker's event-loop processes the broadcast message sent
    // by the main thread after a memory.grow().  Because our C++ sync loop runs
    // without yielding to the browser event loop, those views stay stale for the
    // lifetime of the worker.  _malloc() may return a pointer beyond the stale
    // view's length, making HEAPU8.set() / HEAP32[] a silent no-op and leaving
    // the response buffer full of zeros.
    //
    // Fix: snapshot fresh views directly from the underlying SharedArrayBuffer
    // (.buffer always reflects the grown length) before every memory access.
    var h8  = new Uint8Array(HEAPU8.buffer);
    var h32 = new Int32Array(HEAP32.buffer);

    var urlStr = UTF8ToString(url);
    var methodStr = UTF8ToString(method);
    var status = 0;
    var responseText = '';

    // Auto-upgrade http:// to https:// when the page itself is served over
    // HTTPS.  Browsers block mixed content (HTTP XHR from HTTPS pages).
    if (typeof self !== 'undefined' && self.location && self.location.protocol === 'https:') {
        if (urlStr.startsWith('http://')) {
            urlStr = 'https://' + urlStr.substring(7);
        }
    }

    function doRequest(withJsonContentType) {
        try {
            var xhr = new XMLHttpRequest();
            xhr.open(methodStr, urlStr, false /* synchronous */);
            if (withJsonContentType && body_len > 0) {
                xhr.setRequestHeader('Content-Type', 'application/json');
                xhr.setRequestHeader('Accept', 'application/json');
            }
            if (body_len > 0) {
                xhr.send(new TextDecoder('utf-8').decode(h8.subarray(body, body + body_len)));
            } else {
                xhr.send(null);
            }
            return {
                status: xhr.status || 0,
                body: xhr.responseText || ''
            };
        } catch (requestErr) {
            return {
                status: 0,
                body: ''
            };
        }
    }

    try {
        var methodUpper = methodStr.toUpperCase();
        if (methodUpper === 'POST' && body_len > 0) {
            var first = doRequest(true);
            status = first.status;
            responseText = first.body;

            if (status === 0 || status === 400 || status === 415) {
                var second = doRequest(false);
                status = second.status;
                responseText = second.body;
            }
        } else {
            var single = doRequest(false);
            status = single.status;
            responseText = single.body;
        }
    } catch (err) {
        status = 0;
        responseText = '';
    }
    h32[out_status >> 2] = status;
    var encoded = (new TextEncoder()).encode(responseText);
    var len = encoded.length;
    h32[out_body_len >> 2] = len;
    if (len === 0) { return 0; }
    var ptr = _malloc(len + 1);
    if (!ptr) { return 0; }
    h8.set(encoded, ptr);
    h8[ptr + len] = 0;
    return ptr;
});

inline httplib::Result emscriptenRequestJson(
    const std::string &url,
    const char *method,
    const std::string *jsonBody)
{
    const char *bodyPtr = jsonBody ? jsonBody->c_str() : nullptr;
    const int bodyLen = jsonBody ? static_cast<int>(jsonBody->size()) : 0;
    int status = 0;
    int respLen = 0;

    char *respBuf = wrkzSyncXhr(url.c_str(), method, bodyPtr, bodyLen, &status, &respLen);

    if (status == 0)
    {
        if (respBuf)
        {
            free(respBuf);
        }
        return {nullptr, httplib::Error::Connection};
    }

    auto response = std::make_unique<httplib::Response>();
    response->status = status;
    if (respBuf && respLen > 0)
    {
        response->body.assign(respBuf, static_cast<size_t>(respLen));
    }
    if (respBuf)
    {
        free(respBuf);
    }
    return {std::move(response), httplib::Error::Success};
}
#endif

////////////////////////////////
/* Constructors / Destructors */
////////////////////////////////

std::optional<nlohmann::json>
    Nigel::parseJSONResponse(const httplib::Result &res, const std::string &failMessage, const bool verifyStatus) const
{
    if (res)
    {
        if (res->status == 200)
        {
            try
            {
                nlohmann::json j = nlohmann::json::parse(res->body);

                Logger::logger.log(
                    "Got response from daemon: " + j.dump(),
                    Logger::TRACE,
                    { Logger::SYNC, Logger::DAEMON }
                );

                if (verifyStatus)
                {
                    const std::string status = j.at("status").get<std::string>();

                    if (status != "OK")
                    {
                        Logger::logger.log(
                            failMessage + " - Expected status \"OK\", got " + status,
                            Logger::INFO,
                            { Logger::SYNC, Logger::DAEMON }
                        );

                        return std::nullopt;
                    }
                }

                return j;
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
        else
        {
            std::stringstream stream;

            stream << failMessage << " - got status code " << res->status;

            if (res->body != "")
            {
                stream << ", body: " << res->body;
            }

            Logger::logger.log(
                stream.str(),
                Logger::INFO,
                { Logger::SYNC, Logger::DAEMON }
            );

            return std::nullopt;
        }
    }
    else
    {
        Logger::logger.log(
            failMessage + " - failed to open socket or timed out.",
            Logger::INFO,
            { Logger::SYNC, Logger::DAEMON }
        );

        return std::nullopt;
    }
}

Nigel::Nigel(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL):
    Nigel(daemonHost, daemonPort, daemonSSL, std::chrono::seconds(10))
{
}

Nigel::Nigel(
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const std::chrono::seconds timeout):
    m_timeout(timeout),
    m_daemonHost(daemonHost),
    m_daemonPort(daemonPort),
    m_daemonSSL(daemonSSL)
{
    std::stringstream userAgent;
    userAgent << "Nigel/" << PROJECT_VERSION_LONG;

    m_requestHeaders = {{"User-Agent", userAgent.str()}};
    m_nodeClient = getClient(m_daemonHost, m_daemonPort, m_daemonSSL, m_timeout);
}

Nigel::~Nigel()
{
    stop();
}

//////////////////////
/* Member functions */
//////////////////////

void Nigel::swapNode(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL)
{
    stop();

    m_blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
    m_maxBlockCount = WalletConfig::maxBlocksPerSyncRequest;
    m_lastRequestRateLimited = false;
    m_localDaemonBlockCount = 0;
    m_networkBlockCount = 0;
    m_peerCount = 0;
    m_lastKnownHashrate = 0;
    m_isBlockchainCache = false;
    m_daemonSkipsEmptyBlocks = false;
    m_daemonSpeaksBase64 = false;
    m_daemonSupportsHeightRange = false;
    m_warnedAboutCompression = false;
    m_nodeFeeAddress = "";
    m_nodeFeeAmount = 0;
    m_useRawBlocks = false;

    m_daemonHost = daemonHost;
    m_daemonPort = daemonPort;
    m_daemonSSL = daemonSSL;

    m_nodeClient = getClient(m_daemonHost, m_daemonPort, m_daemonSSL, m_timeout);

    {
        /* These point at the node we just left. */
        std::lock_guard<std::mutex> lock(m_rangeClientMutex);
        m_rangeClients.clear();
    }

    init();
}

void Nigel::decreaseRequestedBlockCount()
{
    /* A rate limited request tells us nothing about how much data the daemon
       can produce in time - it never got that far. Halving the batch here
       would make things strictly worse, since serving the same range then
       costs twice as many rate limit slots. */
    if (m_lastRequestRateLimited)
    {
        return;
    }

    if (m_blockCount > 1)
    {
        m_blockCount = m_blockCount / 2;
    }
}

/* Called after a successful fetch. Rather than snapping back to the default,
   grow towards the negotiated ceiling - larger batches mean fewer round trips
   and fewer rate limit slots consumed for the same number of blocks. */
void Nigel::resetRequestedBlockCount()
{
    const uint64_t ceiling = m_maxBlockCount.load();

    uint64_t current = m_blockCount.load();

    if (current < CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)
    {
        current = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
    }
    else if (current < ceiling)
    {
        current = std::min(current * 2, ceiling);
    }

    m_blockCount = std::min(current, ceiling);
}

bool Nigel::lastRequestWasRateLimited() const
{
    return m_lastRequestRateLimited;
}

uint64_t Nigel::requestedBlockCount() const
{
    return m_blockCount;
}

WalletSyncResponse Nigel::getWalletSyncData(
    const std::vector<Crypto::Hash> blockHashCheckpoints,
    const uint64_t startHeight,
    const uint64_t startTimestamp,
    const bool skipCoinbaseTransactions)
{
    Logger::logger.log("Fetching blocks from the daemon", Logger::DEBUG, {Logger::SYNC, Logger::DAEMON});

    json j = {{"blockHashCheckpoints", blockHashCheckpoints},
              {"startHeight", startHeight},
              {"startTimestamp", startTimestamp},
              {"blockCount", m_blockCount.load()},
              {"skipCoinbaseTransactions", skipCoinbaseTransactions},
              /* Ring member offsets are only needed to build a spend, which we
                 do from get_random_outs - the sync path never reads them.
                 Daemons that don't know the flag send them anyway, and our
                 deserializer treats key_offsets as optional either way. */
              {"skipInputKeyOffsets", true}};

    /* A block holding nothing but a coinbase has nothing in it for us once we
       have said we don't want coinbases, and most blocks on a quiet chain are
       exactly that. Letting the daemon leave them out is what turns a batch
       that advances us a thousand heights into one that can advance us tens of
       thousands. Only asked for where the daemon has said it understands the
       flag - an older one would send a plain range and we would read the gaps
       we expected as a fork. */
    if (skipCoinbaseTransactions && m_daemonSkipsEmptyBlocks)
    {
        j["skipEmptyBlocks"] = true;
    }

    /* Almost the entire response body is fixed size keys and hashes, which
       cost 64 characters each as hex and 44 as base64. */
    if (m_daemonSpeaksBase64)
    {
        j["encoding"] = "base64";
    }

    const std::string endpoint = m_useRawBlocks ? "/getrawblocks" : "/getwalletsyncdata";

    if (!m_useRawBlocks)
    {
        Logger::logger.log("Using /getwalletsyncdata for wallet sync", Logger::DEBUG, {Logger::SYNC, Logger::DAEMON});
    }

    Logger::logger.log(
        "Sending " + endpoint + " request to daemon: " + j.dump(),
        Logger::TRACE,
        { Logger::SYNC, Logger::DAEMON }
    );

    const std::string requestBody = j.dump();
#if defined(__EMSCRIPTEN__)
    const auto res = emscriptenRequestJson(
        daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, endpoint),
        "POST",
        &requestBody);
#else
    const auto res = m_nodeClient->Post(endpoint, toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

    m_lastRequestRateLimited = false;

    /* The daemon's --rpc-max-block-count is lower than what we asked for.
       Remember its ceiling so we stop overshooting, and retry immediately at
       a size it will accept. Daemons old enough not to have the option clamp
       silently instead, which costs us nothing but a smaller batch. */
    if (res && res->status == 400 && m_blockCount > CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)
    {
        const uint64_t rejected = m_blockCount.load();

        const uint64_t reduced = std::max<uint64_t>(CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT, rejected / 2);

        m_maxBlockCount = reduced;
        m_blockCount = reduced;

        Logger::logger.log(
            "Daemon rejected a request for " + std::to_string(rejected) + " blocks. Reducing to "
                + std::to_string(reduced) + " blocks per request.",
            Logger::DEBUG,
            { Logger::SYNC, Logger::DAEMON }
        );

        return getWalletSyncData(blockHashCheckpoints, startHeight, startTimestamp, skipCoinbaseTransactions);
    }

    /* Rate limited. Flag it so the downloader waits rather than shrinking the
       batch, which would only consume more rate limit slots per block. */
    if (res && res->status == 429)
    {
        m_lastRequestRateLimited = true;

        Logger::logger.log(
            "Daemon rate limited our sync request. Backing off before retrying.",
            Logger::DEBUG,
            { Logger::SYNC, Logger::DAEMON }
        );

        return {};
    }

    /* Daemon doesn't support /getrawblocks, or pruned/raw-block path failed:
       fall back to /getwalletsyncdata reconstruction path. */
    if (res && m_useRawBlocks && (res->status == 404 || res->status == 500))
    {
        m_useRawBlocks = false;

        Logger::logger.log(
            "Falling back to /getwalletsyncdata endpoint after " + std::to_string(res->status)
                + " from /getrawblocks",
            Logger::WARNING,
            { Logger::SYNC, Logger::DAEMON }
        );

        return getWalletSyncData(
            blockHashCheckpoints,
            startHeight,
            startTimestamp,
            skipCoinbaseTransactions
        );
    }

    const auto parsedResponse = tryParseJSONResponse(
        res,
        "Failed to fetch blocks from daemon",
        [this, skipCoinbaseTransactions](const nlohmann::json j) {

        std::vector<WalletTypes::WalletBlockInfo> items;

        if (m_useRawBlocks)
        {
            const auto rawBlocks = j.at("items").get<std::vector<CryptoNote::RawBlock>>();

            for (const auto &rawBlock : rawBlocks)
            {
                CryptoNote::BlockTemplate block;

                fromBinaryArray(block, rawBlock.block);

                WalletTypes::WalletBlockInfo walletBlock;

                CryptoNote::CachedBlock cachedBlock(block);

                walletBlock.blockHeight = cachedBlock.getBlockIndex();
                walletBlock.blockHash = cachedBlock.getBlockHash();
                walletBlock.blockTimestamp = block.timestamp;

                if (!skipCoinbaseTransactions)
                {
                    walletBlock.coinbaseTransaction = CryptoNote::Core::getRawCoinbaseTransaction(block.baseTransaction);
                }

                for (const auto &transaction : rawBlock.transactions)
                {
                    walletBlock.transactions.push_back(CryptoNote::Core::getRawTransaction(transaction));
                }

                items.push_back(walletBlock);
            }
        }
        else
        {
            items = j.at("items").get<std::vector<WalletTypes::WalletBlockInfo>>();
        }

        std::optional<WalletTypes::TopBlock> topBlock;

        if (j.find("synced") != j.end() && j.find("topBlock") != j.end() && j.at("synced").get<bool>())
        {
            topBlock = j.at("topBlock").get<WalletTypes::TopBlock>();
        }

        uint64_t scannedToHeight = 0;

        if (j.find("scannedToHeight") != j.end())
        {
            scannedToHeight = j.at("scannedToHeight").get<uint64_t>();
        }

        return std::make_tuple(items, topBlock, scannedToHeight);
    });

    if (parsedResponse)
    {
        const auto [ items, topBlock, scannedToHeight ] = *parsedResponse;

        /* On pruned daemons, /getrawblocks may return no usable data for old
           heights even when not synced yet. Switch to reconstruction endpoint. */
        if (m_useRawBlocks && items.empty() && !topBlock.has_value())
        {
            m_useRawBlocks = false;

            Logger::logger.log(
                "No blocks returned from /getrawblocks during sync. Falling back to /getwalletsyncdata.",
                Logger::WARNING,
                { Logger::SYNC, Logger::DAEMON }
            );

            return getWalletSyncData(
                blockHashCheckpoints,
                startHeight,
                startTimestamp,
                skipCoinbaseTransactions
            );
        }

        return { true, items, topBlock, scannedToHeight };
    }

    return {};
}

void Nigel::abortInFlightRequests()
{
#if !defined(__EMSCRIPTEN__)
    if (m_nodeClient)
    {
        m_nodeClient->stop();
    }

    std::lock_guard<std::mutex> lock(m_rangeClientMutex);

    for (const auto &client : m_rangeClients)
    {
        if (client)
        {
            client->stop();
        }
    }
#endif
}

std::shared_ptr<httplib::Client> Nigel::rangeClient(const size_t slot)
{
    std::lock_guard<std::mutex> lock(m_rangeClientMutex);

    if (m_rangeClients.size() <= slot)
    {
        m_rangeClients.resize(slot + 1);
    }

    if (!m_rangeClients[slot])
    {
        m_rangeClients[slot] = getClient(m_daemonHost, m_daemonPort, m_daemonSSL, m_timeout);
    }

    return m_rangeClients[slot];
}

bool Nigel::daemonSupportsHeightRange() const
{
    return m_daemonSupportsHeightRange;
}

bool Nigel::daemonSkipsEmptyBlocks() const
{
    return m_daemonSkipsEmptyBlocks;
}

WalletSyncResponse Nigel::getWalletSyncDataRange(
    const size_t slot,
    const uint64_t startHeight,
    const uint64_t endHeight,
    const bool skipCoinbaseTransactions)
{
    if (!m_daemonSupportsHeightRange || endHeight <= startHeight)
    {
        return {};
    }

    /* No checkpoints: the window is addressed by height, which is the whole
       point - a caller can name several windows before it has seen any of the
       answers. Fork detection stays with the sequential path, which is why
       this is only used well below the tip. */
    json j = {{"blockHashCheckpoints", std::vector<Crypto::Hash>()},
              {"startHeight", startHeight},
              {"endHeight", endHeight},
              {"startTimestamp", 0},
              {"blockCount", m_blockCount.load()},
              {"skipCoinbaseTransactions", skipCoinbaseTransactions},
              {"skipInputKeyOffsets", true}};

    if (skipCoinbaseTransactions && m_daemonSkipsEmptyBlocks)
    {
        j["skipEmptyBlocks"] = true;
    }

    if (m_daemonSpeaksBase64)
    {
        j["encoding"] = "base64";
    }

    const std::string requestBody = j.dump();

#if defined(__EMSCRIPTEN__)
    const auto res = emscriptenRequestJson(
        daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/getwalletsyncdata"),
        "POST",
        &requestBody);
#else
    const auto res = rangeClient(slot)->Post(
        "/getwalletsyncdata", toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

    if (res && res->status == 429)
    {
        m_lastRequestRateLimited = true;

        return {};
    }

    const auto parsedResponse = tryParseJSONResponse(
        res, "Failed to fetch a block range from daemon", [](const nlohmann::json j) {

        auto items = j.at("items").get<std::vector<WalletTypes::WalletBlockInfo>>();

        uint64_t scannedToHeight = 0;

        if (j.find("scannedToHeight") != j.end())
        {
            scannedToHeight = j.at("scannedToHeight").get<uint64_t>();
        }

        return std::make_tuple(items, scannedToHeight);
    });

    if (!parsedResponse)
    {
        return {};
    }

    const auto [ items, scannedToHeight ] = *parsedResponse;

    return { true, items, std::nullopt, scannedToHeight };
}

void Nigel::stop()
{
    m_shouldStop = true;

    if (m_backgroundThread.joinable())
    {
        m_backgroundThread.join();
    }
}

void Nigel::refreshInfo()
{
    getDaemonInfo();
}

void Nigel::init(bool startBackgroundThread)
{
    m_shouldStop = false;

    /* Get initial daemon info before returning so the status is valid. */
    getDaemonInfo();

    /* Now launch the background thread to constantly update the heights etc.
       Skipped when startBackgroundThread=false (e.g. WASM single-threaded mode). */
    if (startBackgroundThread)
    {
        m_backgroundThread = std::thread(&Nigel::backgroundRefresh, this);
    }
}

bool Nigel::getDaemonInfo()
{
    Logger::logger.log("Updating daemon info", Logger::DEBUG, {Logger::SYNC, Logger::DAEMON});

    Logger::logger.log(
        "Sending /info request to daemon",
        Logger::TRACE,
        { Logger::SYNC, Logger::DAEMON }
    );

#if defined(__EMSCRIPTEN__)
    auto res = emscriptenRequestJson(
        daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/info"),
        "GET",
        nullptr);
#else
    auto res = m_nodeClient->Get("/info", toHeaders(m_requestHeaders));
#endif

    const auto parsedResponse = tryParseJSONResponse(res, "Failed to update daemon info", [this](const nlohmann::json j) {
        m_localDaemonBlockCount = j.at("height").get<uint64_t>();

        /* Height returned is one more than the current height - but we
           don't want to overflow is the height returned is zero */
        if (m_localDaemonBlockCount != 0)
        {
            m_localDaemonBlockCount--;
        }

        m_networkBlockCount = j.at("network_height").get<uint64_t>();

        /* Height returned is one more than the current height - but we
           don't want to overflow is the height returned is zero */
        if (m_networkBlockCount != 0)
        {
            m_networkBlockCount--;
        }

        m_peerCount =
            j.at("incoming_connections_count").get<uint64_t>() + j.at("outgoing_connections_count").get<uint64_t>();

        m_lastKnownHashrate = j.at("difficulty").get<uint64_t>() / CryptoNote::parameters::DIFFICULTY_TARGET;

        /* Look to see if the isCacheApi property exists in the response
           and if so, set the internal value to whatever it found */
        if (j.find("isCacheApi") != j.end())
        {
            m_isBlockchainCache = j.at("isCacheApi").get<bool>();
        }

        /* A lite daemon stores nothing below this height, so no wallet can find
           funds down there through it however far back it is told to scan. A
           daemon too old to advertise the field leaves this at zero, which reads
           as "serves everything" - which is what it is. See LITENODE.md. */
        if (j.find("lite_start_height") != j.end())
        {
            m_liteStartHeight = j.at("lite_start_height").get<uint64_t>();
        }
        else
        {
            m_liteStartHeight = 0;
        }

        /* Optional sync behaviours only get switched on once the daemon has
           named them. A daemon too old to advertise anything simply never
           enables them, and keeps serving what it always has. */
        bool skipEmptyBlocks = false;
        bool base64 = false;
        bool heightRange = false;

        if (j.find("sync_features") != j.end() && j.at("sync_features").is_array())
        {
            for (const auto &feature : j.at("sync_features"))
            {
                if (!feature.is_string())
                {
                    continue;
                }

                const auto name = feature.get<std::string>();

                if (name == CryptoNote::SyncFeatures::SKIP_EMPTY_BLOCKS)
                {
                    skipEmptyBlocks = true;
                }
                else if (name == CryptoNote::SyncFeatures::BASE64_ENCODING)
                {
                    base64 = true;
                }
                else if (name == CryptoNote::SyncFeatures::HEIGHT_RANGE)
                {
                    heightRange = true;
                }
            }
        }

        /* A blockchain cache API speaks its own dialect of these endpoints and
           does not implement our flags, so take it at its word only for the
           daemon. */
        m_daemonSkipsEmptyBlocks = skipEmptyBlocks && !m_isBlockchainCache;
        m_daemonSpeaksBase64 = base64 && !m_isBlockchainCache;
        m_daemonSupportsHeightRange = heightRange && !m_isBlockchainCache;

        if (j.find("compression") != j.end() && j.at("compression").is_string())
        {
            const auto compression = j.at("compression").get<std::string>();

            if (compression == "none" && !m_warnedAboutCompression.exchange(true))
            {
                Logger::logger.log(
                    "The daemon is not compressing its responses, so syncing will transfer several times more "
                    "data than it needs to. This is decided when the daemon is built - its operator can fix it "
                    "by installing zlib and rebuilding.",
                    Logger::WARNING,
                    { Logger::SYNC, Logger::DAEMON }
                );
            }
        }

        return true;
    });

    return parsedResponse.has_value();
}

void Nigel::backgroundRefresh()
{
    while (!m_shouldStop)
    {
        getDaemonInfo();

        Utilities::sleepUnlessStopping(std::chrono::seconds(10), m_shouldStop);
    }
}

bool Nigel::isOnline() const
{
    return m_localDaemonBlockCount != 0 || m_networkBlockCount != 0 || m_peerCount != 0 || m_lastKnownHashrate != 0;
}

uint64_t Nigel::localDaemonBlockCount() const
{
    return m_localDaemonBlockCount;
}

uint64_t Nigel::networkBlockCount() const
{
    return m_networkBlockCount;
}

uint64_t Nigel::liteStartHeight() const
{
    return m_liteStartHeight;
}

uint64_t Nigel::peerCount() const
{
    return m_peerCount;
}

uint64_t Nigel::hashrate() const
{
    return m_lastKnownHashrate;
}

std::tuple<uint64_t, std::string> Nigel::nodeFee() const
{
    return {m_nodeFeeAmount, m_nodeFeeAddress};
}

std::tuple<std::string, uint16_t, bool> Nigel::nodeAddress() const
{
    return {m_daemonHost, m_daemonPort, m_daemonSSL};
}

bool Nigel::getTransactionsStatus(
    const std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown) const
{
    json j = {{"transactionHashes", transactionHashes}};

    Logger::logger.log(
        "Sending /get_transactions_status request to daemon: " + j.dump(),
        Logger::TRACE,
        { Logger::SYNC, Logger::DAEMON }
    );

    const std::string requestBody = j.dump();
#if defined(__EMSCRIPTEN__)
    auto res = emscriptenRequestJson(
        daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/get_transactions_status"),
        "POST",
        &requestBody);
#else
    auto res = m_nodeClient->Post("/get_transactions_status", toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

    const auto parsedResponse = tryParseJSONResponse(res, "Failed to get transactions status", [&](const nlohmann::json j) {
        transactionsInPool = j.at("transactionsInPool").get<std::unordered_set<Crypto::Hash>>();
        transactionsInBlock = j.at("transactionsInBlock").get<std::unordered_set<Crypto::Hash>>();
        transactionsUnknown = j.at("transactionsUnknown").get<std::unordered_set<Crypto::Hash>>();

        return true;
    });

    return parsedResponse.has_value();
}

/* A daemon new enough to return whatever it found answers 200 with short
   per-amount lists, and the caller sizes the ring from those. An older one
   rejects the entire request instead, naming CANT_GET_FAKE_OUTPUTS in the body.
   That is a ring size out of reach, not a node out of reach - report it as such
   so the caller can retry smaller rather than telling the user their daemon is
   down. */
static bool isNotEnoughOutputsResponse(const httplib::Result &res, std::string &error)
{
    if (!res || res->status == 200)
    {
        return false;
    }

    try
    {
        const nlohmann::json j = nlohmann::json::parse(res->body);

        if (j.value("errorCode", 0) != static_cast<int>(CANT_GET_FAKE_OUTPUTS))
        {
            return false;
        }

        error = j.value("errorMessage", std::string("The daemon does not have enough outputs to mix with"));

        return true;
    }
    catch (const nlohmann::json::exception &)
    {
        return false;
    }
}

RandomOutsResponse
    Nigel::getRandomOutsByAmounts(const std::vector<uint64_t> amounts, const uint64_t requestedOuts) const
{
    RandomOutsResponse response;

    json j = {{"amounts", amounts}, {"outs_count", requestedOuts}};

    /* The blockchain cache doesn't call it outs_count
       it calls it mixin */
    if (m_isBlockchainCache)
    {
        j.erase("outs_count");
        j["mixin"] = requestedOuts;

        Logger::logger.log(
            "Sending /randomOutputs request to daemon: " + j.dump(),
            Logger::TRACE,
            { Logger::SYNC, Logger::DAEMON }
        );

        /* We also need to handle the request and response a bit
           differently so we'll do this here */
        const std::string requestBody = j.dump();
#if defined(__EMSCRIPTEN__)
        auto res = emscriptenRequestJson(
            daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/randomOutputs"),
            "POST",
            &requestBody);
#else
        auto res = m_nodeClient->Post("/randomOutputs", toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

        const auto parsedResponse = tryParseJSONResponse(res, "Failed to get random outs", [](const nlohmann::json j) {
            return j.get<std::vector<CryptoNote::RandomOuts>>();
        }, false);

        if (parsedResponse)
        {
            response.success = true;
            response.outs = *parsedResponse;

            return response;
        }

        response.notEnoughOutputs = isNotEnoughOutputsResponse(res, response.error);
    }
    else
    {
        Logger::logger.log(
            "Sending /getrandom_outs request to daemon: " + j.dump(),
            Logger::TRACE,
            { Logger::SYNC, Logger::DAEMON }
        );

        const std::string requestBody = j.dump();
#if defined(__EMSCRIPTEN__)
        auto res = emscriptenRequestJson(
            daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/getrandom_outs"),
            "POST",
            &requestBody);
#else
        auto res = m_nodeClient->Post("/getrandom_outs", toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

        const auto parsedResponse = tryParseJSONResponse(res, "Failed to get random outs", [](const nlohmann::json j) {
            return j.at("outs").get<std::vector<CryptoNote::RandomOuts>>();
        });

        if (parsedResponse)
        {
            response.success = true;
            response.outs = *parsedResponse;

            return response;
        }

        response.notEnoughOutputs = isNotEnoughOutputsResponse(res, response.error);
    }

    return response;
}

std::tuple<bool, bool, std::string> Nigel::sendTransaction(const CryptoNote::Transaction tx) const
{
    json j = {{"tx_as_hex", Common::toHex(CryptoNote::toBinaryArray(tx))}};

    Logger::logger.log(
        "Sending /sendrawtransaction request to daemon: " + j.dump(),
        Logger::TRACE,
        { Logger::SYNC, Logger::DAEMON }
    );

    const std::string requestBody = j.dump();
#if defined(__EMSCRIPTEN__)
    auto res = emscriptenRequestJson(
        daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/sendrawtransaction"),
        "POST",
        &requestBody);
#else
    auto res = m_nodeClient->Post("/sendrawtransaction", toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

    bool success = false;
    bool connectionError = true;
    std::string error;

    tryParseJSONResponse(res, "Failed to send transaction", [&](const nlohmann::json j) {
        connectionError = false;

        success = j.at("status").get<std::string>() == "OK";

        if (j.find("error") != j.end())
        {
            error = j.at("error").get<std::string>();
        }

        return true;
    }, false);

    return {success, connectionError, error};
}

std::tuple<bool, std::unordered_map<Crypto::Hash, std::vector<uint64_t>>>
    Nigel::getGlobalIndexesForRange(const uint64_t startHeight, const uint64_t endHeight) const
{
    /* Blockchain cache API does not support this method and we
       don't need it to because it returns the global indexes
       with the key outputs when we get the wallet sync data */
    if (m_isBlockchainCache)
    {
        return {false, {}};
    }

    json j = {{"startHeight", startHeight}, {"endHeight", endHeight}};

    Logger::logger.log(
        "Sending /get_global_indexes_for_range request to daemon: " + j.dump(),
        Logger::TRACE,
        { Logger::SYNC, Logger::DAEMON }
    );

    const std::string requestBody = j.dump();
#if defined(__EMSCRIPTEN__)
    auto res = emscriptenRequestJson(
        daemonUrl(m_daemonHost, m_daemonPort, m_daemonSSL, "/get_global_indexes_for_range"),
        "POST",
        &requestBody);
#else
    auto res = m_nodeClient->Post("/get_global_indexes_for_range", toHeaders(m_requestHeaders), requestBody, "application/json");
#endif

    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> result;

    const auto parsedResponse = tryParseJSONResponse(res, "Failed to get global indexes for range", [&result](const nlohmann::json j) {
        /* The daemon doesn't serialize the way nlohmann::json does, so
           we can't just .get<std::unordered_map ...> */
        nlohmann::json indexes = j.at("indexes");

        for (const auto &index : indexes)
        {
            result[index.at("key").get<Crypto::Hash>()] = index.at("value").get<std::vector<uint64_t>>();
        }

        return true;
    });

    return {parsedResponse.has_value(), result};
}

