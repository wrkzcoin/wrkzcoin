# Security Hardening

What to lock down before a node, wallet API or wallet service is reachable by
anybody but you, and why each control matters.

## Network exposure

- Bind daemon RPC to localhost unless remote access is required.
- Bind wallet-api to localhost unless behind a trusted reverse proxy.
- Restrict inbound access using host firewall/security groups.
- The stratum server (`--stratum-bind-port`) is off by default and binds to
  loopback when enabled. It has no account and no password: anyone who reaches
  the port can mine to their own address using this node's CPU to build
  templates. Put it behind a firewall or VPN before moving it off loopback, and
  cap it with `--stratum-max-connections`.

- The transaction PoW server (`wrkz-txpow-server`) also binds loopback by
  default. It never sees a key or a signature, but it does see the transaction
  prefix and the client's address shortly before broadcast, so treat it as
  something you run for your own wallets. See
  [Transaction PoW Server](txpow-server.md).

## Prefer a local socket over loopback where you can

On POSIX, `--rpc-ipc-path` serves the same RPC over an AF_UNIX socket whose file
mode decides who may connect — kernel-enforced, rather than a shared secret
every process on the box could read out of a config file. It runs alongside the
TCP listener, so an integration on the same machine can move to it without
changing anything else. Console commands are served **only** there. See
[Local IPC and Console](ipc-and-console.md).

## Authentication baseline

Daemon RPC:

- Set `--rpc-access-token`.
- Require either `X-API-Key` or `Authorization: Bearer`.
- On the daemon's IPC socket the token is not required by default, because the
  socket's mode already decided who may connect. Add `--rpc-ipc-require-token`
  if you want both.

Wallet API:

- Always set `--rpc-password`.
- Require `X-API-KEY` on every call.

Wallet Service JSON-RPC:

- Keep legacy security disabled.
- Require JSON request `password` field.

`wrkz-wallet-api` and `wrkz-service` require their password on an IPC socket
too. Those endpoints move money, so the authentication step is never dropped.

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

## A note on lite nodes

Do not run a [lite node](lite-node.md) as a public node. It cannot serve blocks
below its lite height, so it cannot help new nodes bootstrap, and a user
restoring an older seed against it is clamped to that height and sees a balance
that reads too low. That arrives as a support ticket about missing funds, not as
a bug report. Use `--prune` if disk is the problem.

## Minimal production checklist

1. RPC bound to private interface, or to a local socket.
2. Auth enabled for every RPC surface reachable over TCP.
3. TLS at ingress.
4. Firewall allowlist in place, including the stratum port if it is on.
5. Logs monitored for auth failures and abuse.
6. Backup and restore procedure tested.
