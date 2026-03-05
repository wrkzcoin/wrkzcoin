# Security Hardening

## Network exposure

- Bind daemon RPC to localhost unless remote access is required.
- Bind wallet-api to localhost unless behind a trusted reverse proxy.
- Restrict inbound access using host firewall/security groups.

## Authentication baseline

Daemon RPC:

- Set `--rpc-access-token`.
- Require either `X-API-Key` or `Authorization: Bearer`.

Wallet API:

- Always set `--rpc-password`.
- Require `X-API-KEY` on every call.

Wallet Service JSON-RPC:

- Keep legacy security disabled.
- Require JSON request `password` field.

## Transport security

- If exposing externally, terminate TLS at reverse proxy (Nginx/Caddy/HAProxy).
- Restrict methods/paths at proxy where possible.
- Enable request size limits at proxy and app layers.

## Secrets handling

- Do not commit API tokens/passwords in scripts or repositories.
- Use environment variables or secret manager.
- Rotate RPC tokens/passwords regularly.

## Operational controls

- Keep rate limits enabled (`rpc-max-rpm`).
- Keep body size limits enabled (`rpc-max-body-bytes`).
- Monitor unauthorized attempts (`401`) and throttling (`429`).
- Disable CORS or scope it to trusted origin(s), avoid `*` in production.

## Minimal production checklist

1. RPC bound to private interface.
2. Auth enabled for every RPC surface.
3. TLS at ingress.
4. Firewall allowlist in place.
5. Logs monitored for auth failures and abuse.
6. Backup and restore procedure tested.
