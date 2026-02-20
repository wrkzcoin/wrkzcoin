"use strict";

const { requireAdmin } = require("../lib/adminAuth");

async function indexerRoutes(fastify) {
    fastify.get("/api/v1/index/status", async (request, _reply) => {
        const deep = request.query && (request.query.deep === "1" || request.query.deep === "true");
        const status = await fastify.services.indexer.getStatus({ includeDaemon: deep });
        return { success: true, data: status };
    });

    fastify.post("/api/v1/index/repair", async (request, _reply) => {
        requireAdmin(request, fastify.config.adminApiKey);
        const before = await fastify.services.indexer.getStatus();
        const consistency = await fastify.services.indexer.checkConsistency({ repair: true });
        const after = await fastify.services.indexer.getStatus();

        return {
            success: true,
            data: {
                before,
                consistency,
                after
            }
        };
    });
}

module.exports = indexerRoutes;
