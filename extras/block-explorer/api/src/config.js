"use strict";

function getInt(name, fallback) {
    const raw = process.env[name];
    if (raw === undefined || raw === "") {
        return fallback;
    }
    const parsed = Number.parseInt(raw, 10);
    if (Number.isNaN(parsed)) {
        return fallback;
    }
    return parsed;
}

function getBool(name, fallback) {
    const raw = process.env[name];
    if (raw === undefined || raw === "") {
        return fallback;
    }
    return raw === "true" || raw === "1";
}

function trimSlash(url) {
    if (!url) {
        return "";
    }
    return url.endsWith("/") ? url.slice(0, -1) : url;
}

const config = {
    nodeEnv: process.env.NODE_ENV || "development",
    host: process.env.HOST || "127.0.0.1",
    port: getInt("PORT", 8080),
    corsOrigin: process.env.CORS_ORIGIN || "*",
    daemonRpcUrl: trimSlash(process.env.DAEMON_RPC_URL || "http://127.0.0.1:11898"),
    daemonRpcToken: process.env.DAEMON_RPC_TOKEN || "",
    requestTimeoutMs: getInt("REQUEST_TIMEOUT_MS", 10000),
    apiMaxRpm: getInt("API_MAX_RPM", 300),
    cacheTtlInfoMs: getInt("CACHE_TTL_INFO_MS", 3000),
    cacheTtlBlockMs: getInt("CACHE_TTL_BLOCK_MS", 15000),
    cacheTtlTxMs: getInt("CACHE_TTL_TX_MS", 15000),
    sqliteDbPath: process.env.SQLITE_DB_PATH || "./data/explorer-index.db",
    indexerEnabled: getBool("INDEXER_ENABLED", true),
    indexPaymentIds: getBool("INDEX_PAYMENT_IDS", true),
    indexerBatchSize: getInt("INDEXER_BATCH_SIZE", 100),
    indexerPollIntervalMs: getInt("INDEXER_POLL_INTERVAL_MS", 15000),
    remoteDaemonRpcUrl: trimSlash(process.env.REMOTE_DAEMON_RPC_URL || ""),
    remoteDaemonRpcToken: process.env.REMOTE_DAEMON_RPC_TOKEN || "",
    adminApiKey: process.env.ADMIN_API_KEY || ""
};

module.exports = config;
