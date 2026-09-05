# Transaction PoW Server

Every WrkzCoin transaction carries a small proof of work: the wallet appends a
nonce to the transaction extra and searches until the `cn_upx` hash of the
unsigned transaction prefix meets a difficulty derived from the number of inputs
and outputs — `40,000 + (inputs + 4 × outputs) × 1,000`, so a transaction with
two inputs and six outputs needs 66,000. The daemon refuses transactions that do
not carry it unless they pay at least 10,000 atomic units (100 WRKZ) in fees.

On a desktop that search takes seconds. On a phone it takes longer, and in the
single-threaded browser wallet it is impractical, which is why the web wallet
has been paying the 100 WRKZ bypass fee. `wrkz-txpow-server` moves the search to
a machine that is good at it.

The in-repo long form is `TXPOWSERVER.md`.

## What the server does and does not see

The proof of work is computed over the transaction prefix *before* the ring
signatures are made, so the server receives exactly the bytes the daemon will
see at broadcast a few seconds later: key images, ring member indexes, output
amounts and one-time keys, and the extra field.

It never sees a key, a seed or a signature. It cannot alter the transaction,
because the wallet only accepts 8 nonce bytes back and re-verifies them with one
hash, and it cannot spend anything. **The worst a bad server can do is waste the
wallet's time**, after which the wallet computes the proof itself.

Run the server where the wallet's remote node already runs and nothing new is
learned by anyone. A third-party server is one more party that learns the
sender's IP address and transaction shortly before broadcast.

## Running it

```bash
wrkz-txpow-server --bind-ip 0.0.0.0 --bind-port 17870 --threads 8
```

| Option | Default | Meaning |
| --- | --- | --- |
| `--bind-ip` | `127.0.0.1` | Interface to listen on. Use `0.0.0.0` or `::` for remote wallets |
| `--bind-port` | `17870` | TCP port |
| `--bind-ipv6-address` | off | Second listener on this IPv6 address, same port |
| `--trusted-proxy` | none | Address of a reverse proxy in front of the server. Repeat or comma-separate for several |
| `--threads` | all hardware threads | Hashing threads. All of them work on one job at a time |
| `--rate-limit` | `60` | Requests per minute from one client address. `0` disables |
| `--max-jobs-per-minute` | `120` | Jobs accepted per minute across all clients. `0` disables |
| `--max-queue` | `64` | Jobs allowed to wait for the workers before new ones get `503` |
| `--max-difficulty` | `1000000` | Refuse anything harder than this |
| `--max-wait-ms` | `30000` | Longest a request may be held open waiting for its result |
| `--job-timeout` | `600` | Seconds after which an uncollected queued job is dropped |
| `--result-ttl` | `300` | Seconds a finished result stays available for polling |
| `--api-key` | none | Require this value in the `X-API-KEY` header |
| `--enable-cors` | none | `Access-Control-Allow-Origin` value. The web wallet needs this |
| `--log-level` | `info` | `trace`, `debug`, `info`, `warning`, `fatal` or `disabled` |
| `--log-file` | none | Also append log lines to this file |

There is no service wrapper. On Linux a systemd unit or pm2 does the job; on
Windows use Task Scheduler or NSSM.

### Behind a reverse proxy

The server speaks plain HTTP. For HTTPS, put it behind the reverse proxy that
already terminates TLS for the node. Two things matter:

- **Tell the server who the proxy is.** Every request then arrives from the
  proxy's address, so without `--trusted-proxy` the per-address rate limit would
  treat all wallets as one client. With it, requests from that address are
  attributed to the client named in `X-Real-IP` (preferred) or the last entry of
  `X-Forwarded-For`. Requests from any other address keep their real address, so
  the headers cannot be forged by a direct client.
- **Let the proxy hold a request open** for at least the long-poll length. The
  wallets ask for 20 seconds; the server caps it at `--max-wait-ms`.

```bash
wrkz-txpow-server --bind-ip 127.0.0.1 --bind-port 17870 --trusted-proxy 127.0.0.1
```

```nginx
server {
    listen 443 ssl;
    server_name node.example.com;
    # ssl_certificate / ssl_certificate_key as for the node

    # The trailing slash on proxy_pass strips the prefix, so the server
    # keeps seeing /pow, /stats and /health.
    location /txpow/ {
        proxy_pass         http://127.0.0.1:17870/;
        proxy_http_version 1.1;
        proxy_set_header   Host              $host;
        proxy_set_header   X-Real-IP         $remote_addr;
        proxy_set_header   X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header   X-Forwarded-Proto $scheme;
        proxy_read_timeout 90s;
        proxy_buffering    off;
    }
}
```

In the wallet, enter `node.example.com/txpow` as the host, `443` as the port and
switch SSL on. The host field also accepts a full URL such as
`https://node.example.com/txpow`, in which case the scheme decides SSL. Serving
from the root works the same with a plain host name.

!!! warning "Do not set CORS twice"
    CORS for the web wallet can come either from `--enable-cors` on the server
    or from `add_header` directives in nginx — not both, or the browser sees the
    header twice and rejects it.

### Throughput

The server uses the same reference hash implementation as the wallets. On a
16-thread laptop that is roughly 4,000 to 5,000 hashes per second, so a typical
transaction takes 10 to 20 seconds of the whole machine. `--max-queue` and
`--max-jobs-per-minute` exist so a burst turns into refusals — which make the
wallets compute locally — rather than into a queue nobody lives to see the end
of.

## Protocol

All bodies are JSON. Every job reply has a `status` of `done`, `pending`,
`cancelled` or `error`.

### `POST /pow`

```json
{ "prefix": "<hex of the serialized transaction prefix>", "wait_ms": 20000, "height": 4300000 }
```

The prefix must already end with the PoW tag byte `0x04` followed by 8 nonce
bytes (the wallet zero-fills them). `wait_ms` is optional and capped at
`--max-wait-ms`; the request is held open that long waiting for a result.
`height` is optional and only matters for historical difficulty rules.

While the job is queued or running (HTTP `200`):

```json
{ "status": "pending", "job_id": "…32 hex…", "state": "running",
  "difficulty": 66000, "hashes": 12800, "elapsed_ms": 3100 }
```

Once it is solved (HTTP `200`):

```json
{ "status": "done", "job_id": "…", "nonce": "3a9f1c0000000000",
  "difficulty": 66000, "hashes": 71424, "elapsed_ms": 14020 }
```

`nonce` is the 8 bytes to copy over the trailing 8 bytes of the prefix, in that
byte order. The wallet verifies `cn_upx(prefix)` against the difficulty before it
signs.

Refusals:

| Code | When |
| --- | --- |
| `400` | Prefix does not parse, is not canonically serialized, has no fee, carries too many outputs, lacks the nonce tag, or is above `--max-difficulty` |
| `401` | API key missing or wrong |
| `429` | The client, or the server as a whole, is over its per-minute limit |
| `503` | The queue is full |

### Other routes

| Route | What it does |
| --- | --- |
| `GET /pow/<job_id>?wait_ms=20000` | Same job reply. `404` for an unknown or expired job |
| `DELETE /pow/<job_id>` | Cancels a queued or running job |
| `GET /stats` | Counters since start-up |
| `GET /health` | Queue depth and capacity, for load balancers. Outside the API key and the rate limit |

`/stats` reports jobs received, accepted, completed, failed, cancelled, expired
and rejected by reason; total hashes, busy time and average hash rate; average
and worst solve time; queue depth and the shape of the job currently running
(never its id, which would let a reader cancel it); HTTP request counts; the two
limits a client can act on (`max_difficulty`, `max_wait_ms`); and the version.

It deliberately does **not** describe the deployment — bind addresses, trusted
proxies, rate limits, the CORS origin and whether an API key is required stay in
the start-up banner and the operator's command line.

## Wallet side

The desktop, mobile and web wallets have a *Transaction PoW Server* section in
Settings: a switch, host, port, SSL, a *Test* button and *Apply*.

- **The switch is off by default**, so every wallet keeps computing the proof of
  work on its own CPU until the user turns it on.
- The fields come prefilled with the project's public server, `txpow.wrkz.work`
  on port `443` with SSL, so enabling it is one switch. Any other server can be
  entered instead, and the host may include a path or be a full URL when the
  server sits behind a proxy.
- *Test* calls `/health` over the same client path a transaction would use,
  including the native SSL support check, and reports latency, thread count and
  queue occupancy without saving anything.
- When the setting is on, the wallet asks the server first and waits up to two
  minutes. If the server is unreachable, refuses the job, times out or returns a
  nonce that does not verify, the wallet computes the proof on its own CPU as
  before.
- The setting is stored on the device and applied every time a wallet is opened.

The web wallet keeps paying the 100 WRKZ bypass fee while no server is
configured, because its local fallback is far slower than on any other platform.
With a server configured it pays the normal minimum fee.

**`wrkz-wallet`, `wrkz-wallet-api` and `wrkz-service` always compute the proof
of work on their own CPU** and have no setting for this.

For embedders, the C API exposes `wallet_set_tx_pow_server(host, port, ssl)`; an
empty host turns the feature off. The WASM bridge exposes the same as
`setTxPowServer`.
