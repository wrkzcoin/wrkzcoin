"use strict";

async function txRoutes(fastify) {
    fastify.get("/api/v1/tx/by-payment-id/:paymentId", async (request, _reply) => {
        const rawLimit = request.query && request.query.limit ? Number.parseInt(request.query.limit, 10) : 100;
        const limit = Number.isNaN(rawLimit) ? 100 : Math.min(Math.max(rawLimit, 1), 500);
        const result = fastify.services.tx.getTxByPaymentId(request.params.paymentId, limit);
        return { success: true, data: result };
    });

    fastify.get("/api/v1/tx/:hash", async (request, _reply) => {
        const result = await fastify.services.tx.getTxByHash(request.params.hash);
        return { success: true, data: result };
    });
}

module.exports = txRoutes;
