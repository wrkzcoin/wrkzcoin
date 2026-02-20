"use strict";

const Fastify = require("fastify");
const fastifyCors = require("@fastify/cors");
const fastifyRateLimit = require("@fastify/rate-limit");

const config = require("./config");
const { HttpError } = require("./lib/httpErrors");
const TtlCache = require("./lib/ttlCache");
const DaemonRpcClient = require("./lib/daemonRpc");
const SqliteStore = require("./lib/sqliteStore");

const SearchService = require("./services/searchService");
const BlockService = require("./services/blockService");
const TxService = require("./services/txService");
const NetworkService = require("./services/networkService");
const IndexerService = require("./services/indexerService");

const healthRoutes = require("./routes/health");
const searchRoutes = require("./routes/search");
const blockRoutes = require("./routes/block");
const txRoutes = require("./routes/tx");
const networkRoutes = require("./routes/network");
const indexerRoutes = require("./routes/indexer");

function buildApp() {
    const app = Fastify({
        logger: true
    });

    const rpcClient = new DaemonRpcClient({
        baseUrl: config.daemonRpcUrl,
        token: config.daemonRpcToken,
        timeoutMs: config.requestTimeoutMs
    });
    const remoteRpcClient = config.remoteDaemonRpcUrl
        ? new DaemonRpcClient({
            baseUrl: config.remoteDaemonRpcUrl,
            token: config.remoteDaemonRpcToken,
            timeoutMs: config.requestTimeoutMs
        })
        : null;

    const cache = new TtlCache();
    const store = new SqliteStore(config.sqliteDbPath);

    const blockService = new BlockService({
        rpcClient,
        cache,
        cacheTtlMs: config.cacheTtlBlockMs
    });

    const txService = new TxService({
        rpcClient,
        cache,
        cacheTtlMs: config.cacheTtlTxMs,
        store
    });

    const networkService = new NetworkService({
        rpcClient,
        cache,
        cacheTtlMs: config.cacheTtlInfoMs
    });

    const indexerService = new IndexerService({
        store,
        localRpcClient: rpcClient,
        remoteRpcClient,
        enabled: config.indexerEnabled,
        indexPaymentIds: config.indexPaymentIds,
        batchSize: config.indexerBatchSize,
        pollIntervalMs: config.indexerPollIntervalMs,
        logger: app.log
    });

    const searchService = new SearchService({
        blockService,
        txService,
        indexerService
    });

    app.decorate("services", {
        search: searchService,
        block: blockService,
        tx: txService,
        network: networkService,
        indexer: indexerService
    });
    app.decorate("config", config);

    app.register(fastifyCors, {
        origin: config.corsOrigin
    });

    app.register(fastifyRateLimit, {
        max: config.apiMaxRpm,
        timeWindow: "1 minute"
    });

    app.register(healthRoutes);
    app.register(searchRoutes);
    app.register(blockRoutes);
    app.register(txRoutes);
    app.register(networkRoutes);
    app.register(indexerRoutes);

    app.addHook("onReady", async () => {
        await app.services.indexer.start();
    });

    app.addHook("onClose", async () => {
        app.services.indexer.stop();
    });

    app.setNotFoundHandler((_request, reply) => {
        reply.code(404).send({
            success: false,
            error: {
                code: "NOT_FOUND",
                message: "Route not found",
                details: null
            }
        });
    });

    app.setErrorHandler((error, _request, reply) => {
        if (error instanceof HttpError) {
            reply.code(error.statusCode).send({
                success: false,
                error: {
                    code: error.code,
                    message: error.message,
                    details: error.details
                }
            });
            return;
        }

        if (error.statusCode === 429) {
            reply.code(429).send({
                success: false,
                error: {
                    code: "RATE_LIMITED",
                    message: "Too many requests",
                    details: null
                }
            });
            return;
        }

        reply.code(500).send({
            success: false,
            error: {
                code: "INTERNAL_ERROR",
                message: "Unexpected server error",
                details: null
            }
        });
    });

    return app;
}

async function start() {
    const app = buildApp();

    try {
        await app.listen({ host: config.host, port: config.port });
    } catch (err) {
        app.log.error(err);
        process.exit(1);
    }
}

if (require.main === module) {
    start();
}

module.exports = { buildApp };
