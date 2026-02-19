"use strict";

async function networkRoutes(fastify) {
    fastify.get("/api/v1/network/summary", async (_request, _reply) => {
        const summary = await fastify.services.network.getSummary();
        return { success: true, data: summary };
    });
}

module.exports = networkRoutes;

