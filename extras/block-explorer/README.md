# Block Explorer (Extras)

This folder contains an extendable block explorer workspace.

## Layout

- `api/`: Node.js + Fastify API service (MVP routes implemented)
- `web/`: Placeholder for frontend app/static files

## MVP API routes

- `GET /api/v1/health`
- `GET /api/v1/search?q=...`
- `GET /api/v1/block/:id`
- `GET /api/v1/tx/:hash`
- `GET /api/v1/tx/by-payment-id/:paymentId`
- `GET /api/v1/network/summary`
- `GET /api/v1/index/status`
- `POST /api/v1/index/repair`
