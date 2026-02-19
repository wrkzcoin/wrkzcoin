"use strict";

async function searchRoutes(fastify) {
    fastify.get("/api/v1/search", async (request, _reply) => {
        const result = await fastify.services.search.search(request.query.q);
        return { success: true, data: result };
    });
}

module.exports = searchRoutes;

