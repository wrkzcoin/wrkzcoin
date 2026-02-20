"use strict";

const { requireAdmin } = require("../lib/adminAuth");

async function healthRoutes(fastify) {
    fastify.get("/api/v1/health", async (request, _reply) => {
        const repair = request.query && (request.query.repair === "1" || request.query.repair === "true");
        const deep = request.query && (request.query.deep === "1" || request.query.deep === "true");
        const includeDaemon = deep || repair;

        try {
            const summary = await fastify.services.network.getSummary();
            const indexer = await fastify.services.indexer.getStatus({ includeDaemon });

            let consistency = null;
            let status = "ok";

            if (deep || repair) {
                if (repair) {
                    requireAdmin(request, fastify.config.adminApiKey);
                }
                consistency = await fastify.services.indexer.checkConsistency({ repair });
                status = consistency.consistent_with_local ? "ok" : "degraded";
            }

            return {
                success: true,
                data: {
                    status,
                    upstream_rpc: "ok",
                    network: {
                        height: summary.height,
                        network_height: summary.network_height,
                        synced: summary.synced
                    },
                    indexer,
                    consistency,
                    mode: deep || repair ? "deep" : "shallow"
                }
            };
        } catch (err) {
            return {
                success: true,
                data: {
                    status: "degraded",
                    upstream_rpc: "down",
                    error: err.message
                }
            };
        }
    });
}

module.exports = healthRoutes;
