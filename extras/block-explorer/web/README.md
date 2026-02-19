# Explorer Web

Responsive static UI scaffold for desktop and mobile.

## Files

- `index.html`
- `styles.css`
- `app.js`

## Features

- Mobile-first responsive layout with desktop scaling
- Search box for block/tx/payment-id flows via `/api/v1/search`
- Live cards for:
  - `/api/v1/network/summary`
  - `/api/v1/index/status`
- Result panel rendering JSON payloads from API endpoints

## Serve

Serve this folder as static files and proxy `/api/` to the Fastify API process.
