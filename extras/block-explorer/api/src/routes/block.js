"use strict";

async function blockRoutes(fastify) {
    fastify.get("/api/v1/block/:id", async (request, _reply) => {
        const result = await fastify.services.block.getBlockById(request.params.id);
        return { success: true, data: result };
    });
}

module.exports = blockRoutes;

