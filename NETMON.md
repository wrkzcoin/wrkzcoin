# wrkz-netmon — network monitor

> The published version of this page is at
> <https://docs.wrkz.work/guides/netmon/>. This file is the in-repo long form
> that page tracks.

`wrkz-netmon` walks the WrkzCoin P2P network and serves what it finds: which
nodes are reachable, what version they run, how far behind the tip they are,
whether the network agrees on one chain, and where the nodes sit.

It speaks the real handshake, so everything it reports is either **measured**
(the port answered, how long that took, how often it has answered before) or
**claimed by the node** (protocol version, height, top block hash, capability
flags). The dashboard keeps those two apart on every screen, because a node can
say anything about itself and nothing about whether it answered.

It needs **no blockchain and no local daemon**. The network id and the genesis
hash are compiled in, so it bootstraps from the seed nodes on its own.

---

## Contents

- [Quick start](#quick-start)
- [What it can and cannot tell you](#what-it-can-and-cannot-tell-you)
- [Building](#building)
- [Running](#running)
- [Location data (DB-IP Lite)](#location-data-db-ip-lite)
- [Behind nginx](#behind-nginx)
- [The JSON API](#the-json-api)
- [Crawler etiquette](#crawler-etiquette)
- [Data on disk](#data-on-disk)
- [Troubleshooting](#troubleshooting)

---

## Quick start

```bash
# Build just this target (the tree must have been configured once already)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target NetMon

# Run it, serving the dashboard from the repo
./build/src/wrkz-netmon --web-root extras/netmon
```

Open <http://127.0.0.1:17871/>. The first sweep starts immediately; the seeds
answer within a second or two and the first peer lists arrive with them.

On Windows the binary is `build\src\Release\wrkz-netmon.exe`.

---

## What it can and cannot tell you

Worth reading before you wire it into a dashboard someone else will look at.

| It can | Because |
|---|---|
| Reachable vs. known-only ("white" vs. "grey") | It dials every address itself, so a completed handshake **is** the proof the port is open |
| Node height and top block hash | `CORE_SYNC_DATA` in the handshake response |
| **Chain split detection** | Nodes at the same height on *different* top hashes. Nothing else in the toolchain surfaces this |
| Full / pruned / lite mix, and how deep each serves | `capability_flags`, `pruned_node_height`, `lite_start_height` |
| P2P protocol version adoption over time | `basic_node_data.version` |
| Whether `P2P_MINIMUM_VERSION` can be raised yet | The version histogram, which is exactly what that decision waits on |
| Clock skew across the network | `basic_node_data.local_time` against this host's clock |
| Churn, uptime, latency per node | Its own repeated sweeps |
| IPv4 / IPv6 split | Which family the address came in |

| It cannot | Because |
|---|---|
| **Attribute a block to a miner** | Coinbase outputs are one-time keys. There is no account model to attribute to |
| **Search by wallet address** | Same reason |
| Report a **software version** for most nodes | That string only exists on the daemon's RPC port, and `rpcInterface` defaults to `127.0.0.1`. `--probe-rpc` asks the minority that do expose it. Network-wide coverage would need a `user_agent` field on the handshake itself |
| Measure block propagation between other nodes | That needs an agent running *on* those nodes, reporting in. A different product |
| Locate a node without a database | See [Location data](#location-data-db-ip-lite) |

The "peer version" you get network-wide is a **protocol byte**, currently 16 to
19 — four buckets, not `Wrkzd/0.4.8.280`. That is a property of the wire format,
not a limitation of this tool.

---

## Building

Nothing new is required. Same toolchain as the rest of the tree:

| | |
|---|---|
| CMake | 3.16 or later |
| Compiler | GCC 10+, Clang 10+ (15+ on Linux), or MSVC 19.29+ |
| Also | Git, and Make or Ninja |

OpenSSL and zlib stay optional and are detected the same way. zlib is worth
having: it is what lets httplib gzip the dashboard's CSS and JS.

The target links `System`, `Serialization`, `Crypto`, `Common`, `Logger` and
`Config` — **no RocksDB, no CryptoNoteCore, no P2P library**. It compiles
`p2p/LevinProtocol.cpp` directly rather than linking all of `P2P`, which would
drag in miniupnpc and the whole chain layer for the sake of one file.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target NetMon      # -> build/src/wrkz-netmon
```

Windows, from an x64 Native Tools prompt:

```bat
cmake -S . -B build
cmake --build build --config Release --target NetMon --parallel
```

Three things that bite when working on this target:

1. **A new source file needs a re-configure.** The sources are globbed, so
   after adding a file to `src/netmon/` run `cmake -S . -B build` again.
2. **Kill the running binary before rebuilding.** A live `wrkz-netmon` locks
   its own executable and the link step fails with LNK1104 on Windows.
3. **The CMake target is `NetMon`, the output is `wrkz-netmon`.** They may not
   share a name — see the comment above the convenience targets at the bottom of
   `src/CMakeLists.txt`.

---

## Running

```bash
wrkz-netmon --web-root extras/netmon
```

That is the whole of it for a local install. The binary serves both the
dashboard and its JSON API on one port, so **nginx is optional**.

### Options that matter

| Flag | Default | What it does |
|---|---|---|
| `--bind-ip` | `127.0.0.1` | Where the dashboard and API listen |
| `--bind-port` | `17871` | Port for both |
| `--bind-ipv6-address` | *(off)* | Second listener, same port |
| `--web-root <dir>` | *(off)* | Serve these static files at `/`. Omit for API only |
| `--sweep-interval` | `600` | Seconds between sweeps. Minimum 60, and the minimum is enforced |
| `--concurrency` | `64` | Probes in flight. These are fibers, not threads |
| `--probe-timeout` | `5000` | ms for one connect + handshake |
| `--failure-backoff` | `600` | Seconds a refused or timed-out address is skipped |
| `--max-targets` | `0` | Addresses per sweep; 0 means all of them |
| `--seed-node <addr>` | *(none)* | Extra bootstrap address. Repeatable |
| `--no-default-seeds` | off | Ignore the compiled-in seeds. For a test network |
| `--probe-rpc` | off | Also ask each peer's RPC port for `/info` (software versions) |
| `--geoip-db <file>` | *(none)* | DB-IP Lite country CSV |
| `--asn-db <file>` | *(none)* | DB-IP Lite ASN CSV |
| `--data-dir` | `netmon-data` | Node table and history |
| `--history-days` | `30` | Days of per-node daily reachability to keep |
| `--enable-cors <origin>` | *(none)* | `Access-Control-Allow-Origin`. Quote it: `--enable-cors '*'` |
| `--trusted-proxy <ip>` | *(none)* | Believe this proxy's `X-Real-IP` when logging. Repeatable |
| `--log-level` | `info` | trace, debug, info, warning, fatal, disabled |

`wrkz-netmon --help` prints the full list.

### What it needs from the host

**Required:**

1. **Unrestricted outbound TCP.** This is the requirement most likely to bite.
   Peers advertise whatever port they like (`my_port` is a `uint32` in the
   handshake), so 17855 is a convention, not a constraint. A host with egress
   filtering reports reachable nodes as closed, which is worse than reporting
   nothing.
2. **A synchronised clock.** The clock-skew panel measures peers against *this*
   host. An unsynced monitor makes the whole network look broken. Run NTP.

**Not required:** a blockchain, a daemon, a database, or an inbound port. The
crawler never listens for P2P — only for its own dashboard.

**Optional:** a working IPv6 route, to reach the IPv6-only nodes. Without one
they all look unreachable, which is wrong data rather than missing data.

### Resource use

Hundreds of concurrent sockets on one fiber thread: tens of MB of RAM,
negligible CPU between sweeps. Disk is a few MB for the node table.

On **Windows**, keep `--concurrency` modest. The Windows dispatcher never
received two of the fiber-lifetime fixes the Linux, macOS and Android ones
carry, and 512 is refused outright for that reason.

---

## Location data (DB-IP Lite)

Neither database is bundled. They carry their own licence and their own monthly
release, so they are downloaded separately and passed in at runtime. Without
them the map is empty and **every other view works normally**.

[DB-IP Lite](https://db-ip.com/db/lite.php) is the recommended source: the free
files are **CC-BY 4.0**, which means they may be redistributed with attribution,
unlike MaxMind's GeoLite2 (free but account-gated and not redistributable).

### Fetching them

There is a script for this:

```bash
bash scripts/netmon/refresh-geoip.sh --dir /opt/wrkz
```

It installs two files under `--dir` with stable names, so the service
configuration never has to name a month:

```
/opt/wrkz/dbip-country-lite.csv
/opt/wrkz/dbip-asn-lite.csv
```

Measured against the 2026-09 release: 717,170 country ranges and 473,272 ASN
ranges, about 60 MB of CSV on disk. `--dry-run` prints the URLs it would use,
`--month 2026-08` pins a release, and `--restart wrkz-netmon` restarts the
service but only when a file actually changed.

It handles three things a bare `curl` does not. DB-IP does not publish the new
month at the stroke of midnight on the 1st, so it falls back to the previous
month rather than failing. A truncated download or an error page would
otherwise replace a good database with rubbish, so nothing is installed until
the archive passes `gzip -t`, the first data line looks like a range, and there
are more than a thousand rows. And the install is a rename over the target, so
a monitor starting up mid-refresh reads either the old file or the new one,
never half of either.

### Fetching them by hand

The URLs carry the release month, so substitute the current one:

```bash
MONTH=$(date +%Y-%m)

# Country: ~500k ranges, IPv4 and IPv6 in one file
curl -fSLO "https://download.db-ip.com/free/dbip-country-lite-${MONTH}.csv.gz"

# ASN: which network hosts each range
curl -fSLO "https://download.db-ip.com/free/dbip-asn-lite-${MONTH}.csv.gz"

gunzip dbip-country-lite-${MONTH}.csv.gz dbip-asn-lite-${MONTH}.csv.gz
```

PowerShell:

```powershell
$month = Get-Date -Format 'yyyy-MM'
foreach ($db in 'country', 'asn') {
  $file = "dbip-$db-lite-$month.csv.gz"
  Invoke-WebRequest "https://download.db-ip.com/free/$file" -OutFile $file
}
# then gunzip with 7-Zip, tar, or any archiver
```

Then point the monitor at them:

```bash
wrkz-netmon --web-root extras/netmon \
            --geoip-db /opt/wrkz/dbip-country-lite-2026-09.csv \
            --asn-db   /opt/wrkz/dbip-asn-lite-2026-09.csv
```

Startup prints how many ranges loaded and how many lines it skipped. Skipped
lines are normal in small numbers — DB-IP marks unallocated ranges `ZZ`, which
would otherwise show as a country.

### Expected format

Both files are plain CSV, quoted or not depending on the release. The loader
handles either, and the ASN name field may itself contain commas:

```
1.0.0.0,1.0.0.255,AU
2001:200::,2001:200:ffff:ffff:ffff:ffff:ffff:ffff,JP
```

```
1.0.0.0,1.0.0.255,13335,"CLOUDFLARENET"
```

If you use a different provider, match those column orders — start, end,
then the country code or the AS number and name.

### Keeping them current

DB-IP publishes monthly. One cron entry is enough:

```cron
# 03:17 on the 3rd - late enough that the new release is up, and the script
# falls back to last month's if it is not.
17 3 3 * * /opt/wrkz/scripts/netmon/refresh-geoip.sh --dir /opt/wrkz --restart wrkz-netmon
```

`--restart` only fires when a file changed, so a month DB-IP skips costs
nothing and leaves the service alone.

Both files are read **once, at startup**, so a refresh has no effect until the
monitor restarts. Nothing breaks in the meantime: stale data degrades quietly,
and nodes on newly allocated ranges simply show as unlocated.

### Attribution

CC-BY requires it. The dashboard footer is the natural place:

> IP geolocation by [DB-IP](https://db-ip.com)

---

## Behind nginx

The single-binary path is the default for a reason, so reach for nginx only
when you need what it actually adds: **TLS, caching, and rate limiting for
anonymous traffic**. `wrkz-netmon` has no rate limiter of its own, so a directly
exposed instance has nothing throttling it.

### Decide what you are publishing first

The dashboard shows **per-node IP addresses** on the Peers and node-detail
views. Those operators never opted in to being crawled. Before putting this on
a public hostname, decide whether the public should see per-node detail or only
the aggregates. The layout below does the second: it serves the map, the version
panels and the summary to everyone, and keeps the per-node views behind an
allowlist.

### Layout

- nginx serves the static files from a copy of `extras/netmon`
- nginx proxies `/api/` to `wrkz-netmon` on loopback
- `wrkz-netmon` runs **without** `--web-root`, so it only answers the API

```nginx
upstream wrkz_netmon {
    server 127.0.0.1:17871;
    keepalive 8;
}

# Anonymous traffic gets a budget. The dashboard polls /api/summary every
# 20 s per open tab, so this is generous for a reader and tight for a scraper.
limit_req_zone $binary_remote_addr zone=netmon_api:10m rate=30r/m;

server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name netmon.example.com;

    ssl_certificate     /etc/letsencrypt/live/netmon.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/netmon.example.com/privkey.pem;

    root /var/www/wrkz-netmon;
    index index.html;

    # ---- static dashboard ------------------------------------------------
    location / {
        try_files $uri $uri/ /index.html;
    }

    # index.html must not be cached, or a deploy leaves browsers running a new
    # page against an old app.js. The ?v= stamp handles the assets.
    location = /index.html {
        add_header Cache-Control "no-cache";
    }

    # ---- public API ------------------------------------------------------
    location /api/ {
        limit_req zone=netmon_api burst=20 nodelay;

        proxy_pass http://wrkz_netmon;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        proxy_connect_timeout 5s;
        proxy_read_timeout    30s;
    }

    # ---- per-node detail: operator view only -----------------------------
    # Publishes one node's address, ASN and full claim set. Everything above
    # this block is aggregate-only, so this is the line worth guarding.
    location /api/peers {
        allow 10.0.0.0/8;
        allow 192.168.0.0/16;
        deny  all;

        proxy_pass http://wrkz_netmon;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
        proxy_set_header Host      $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    location = /health {
        proxy_pass http://wrkz_netmon;
        access_log off;
    }
}

server {
    listen 80;
    listen [::]:80;
    server_name netmon.example.com;
    return 301 https://$host$request_uri;
}
```

**`location /api/peers` must come after `location /api/`** in the file only for
readability — nginx picks the longest matching prefix regardless of order, so
`/api/peers` and `/api/peers/1.2.3.4:17855` both land in the guarded block while
`/api/summary`, `/api/geo` and `/api/versions` stay public.

With that in place, run the monitor API-only and tell it who the proxy is:

```bash
wrkz-netmon --bind-ip 127.0.0.1 --bind-port 17871 \
            --trusted-proxy 127.0.0.1 \
            --geoip-db /opt/wrkz/dbip-country-lite-2026-09.csv \
            --asn-db   /opt/wrkz/dbip-asn-lite-2026-09.csv
```

`--trusted-proxy` only affects **which address gets written to the log**. It
grants nothing, so a spoofed `X-Real-IP` from an untrusted source buys an
attacker nothing but a wrong log line — and from an untrusted source the header
is ignored anyway.

### Deploying the static files

```bash
rsync -a --delete extras/netmon/ /var/www/wrkz-netmon/
```

**Bump the `?v=` stamp in `index.html` on every deploy.** A CDN will cache
`style.css` and `app.js` but not `index.html`, and the failure mode is nasty:
the new markup renders against the old script, so the page looks subtly broken
rather than plainly stale.

```bash
sed -i "s/?v=[0-9]\{8\}/?v=$(date +%Y%m%d)/g" extras/netmon/index.html
```

### CORS, if the page is on another origin

Only needed when the dashboard is served from a different host than the API.
Same-origin (either deployment above) needs nothing.

```bash
wrkz-netmon --enable-cors 'https://netmon.example.com'
```

Quote it. An unquoted `--enable-cors *` is expanded by the shell into the first
filename in the working directory.

### systemd unit

```ini
[Unit]
Description=WrkzCoin network monitor
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=wrkz
Group=wrkz
WorkingDirectory=/opt/wrkz
ExecStart=/opt/wrkz/wrkz-netmon \
  --bind-ip 127.0.0.1 --bind-port 17871 \
  --trusted-proxy 127.0.0.1 \
  --data-dir /var/lib/wrkz-netmon \
  --geoip-db /opt/wrkz/dbip-country-lite.csv \
  --asn-db /opt/wrkz/dbip-asn-lite.csv \
  --log-level info
Restart=on-failure
RestartSec=10

NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/wrkz-netmon

[Install]
WantedBy=multi-user.target
```

`ProtectSystem=strict` is safe here because the monitor writes only to
`--data-dir`. Do not add an egress restriction: outbound TCP to arbitrary hosts
and ports is the whole job.

---

## The JSON API

Every path lives under `/api/`, deliberately. httplib checks its static mount
points **before** its route handlers, so a file in the web root would otherwise
shadow a same-named route.

| Endpoint | Returns |
|---|---|
| `GET /api/summary` | Everything the overview needs: counts, height buckets, top hashes, versions, capability mix, clock skew, churn, sweep history |
| `GET /api/peers` | The node table. `?filter=all\|reachable\|grey\|full\|pruned\|lite\|ipv6\|behind`, `?q=`, `?offset=`, `?limit=` (max 1000) |
| `GET /api/peers/<address:port>` | One node, including its daily history |
| `GET /api/geo` | Country and ASN aggregates, and whether a database is loaded |
| `GET /api/versions` | Version histogram, software builds, and what raising the floor would cost |
| `GET /api/health` | Liveness. Also at `/health` |

Every peer object separates the two kinds of fact:

```json
{
  "key": "192.0.2.44:17855",
  "reach": "open",
  "rttMs": 28,
  "sweepsAnswered": 4094,
  "sweepsOffered": 4118,
  "reported": {
    "p2pVersion": 19,
    "height": 4210941,
    "topId": "593d8f02...",
    "pruned": false,
    "lite": false,
    "softwareVersion": ""
  },
  "location": { "country": "DE", "asn": "AS24940", "asName": "Hetzner" }
}
```

Top level is measured. `reported` is what the node claimed and can be spoofed.
`location` came out of a database and is neither.

---

## Crawler etiquette

These are constants in the code, not options, and they should stay that way.
A crawler touches every node on the network on every sweep; getting these wrong
does real damage to other people's infrastructure.

1. **`my_port = 0` in every handshake.** A peer receiving that skips its back
   ping and does **not** add the sender to its white list, so the monitor is
   never gossiped around as a peer. This is what the daemon's own
   `--hide-my-port` does internally.

2. **`current_height = 0`.** A receiving node folds a handshake's
   `current_height` into its observed-height maximum, so an inflated value
   poisons `network_height` in `/info` on **every node it touches** until that
   node restarts. Zero is stronger than merely honest: the daemon's lite-node
   depth check is guarded on `current_height > 0` and can call `exit(1)` after
   only four samples, so a crawler claiming *any* height could shut down a
   booting lite node.

3. **`top_id` = the genesis hash.** Every node has it, so the receiver's
   `hasBlock()` succeeds and it takes the "nothing to do here" branch instead of
   deciding it might need to sync from us and opening a chain request the
   crawler has already hung up on.

4. **One command, then hang up.** Every node runs an inbound cap
   (`--in-peers`, 15 by default). A crawler that lingers holds one of those
   slots on every node in the network simultaneously.

5. **Sweep no faster than every 60 s, and 5–15 minutes in practice.** The
   daemon handshakes a live peer once a minute and leaves a failed address alone
   for ten. Matching that makes the monitor indistinguishable from an ordinary
   peer. The 60 s floor is enforced at startup.

6. **Never relay, never request blocks, never advertise capabilities you do
   not have.**

The genesis hash is taken from the height-0 entry of `CryptoNoteCheckpoints.h`
rather than a second hardcoded copy, so it cannot drift from what the daemon
validates against.

---

## Data on disk

`--data-dir` holds one file:

```
netmon-data/nodes.ndjson
```

Newline-delimited JSON: a header line with the sweep counters and the reachable
series, then one line per address. Written to a temporary file and renamed over
the target, so a crash mid-write leaves the previous table intact rather than a
truncated one.

It is meant to be greppable:

```bash
# every reachable lite node
grep '"reach":"open"' netmon-data/nodes.ndjson | grep '"capabilityFlags":2'

# heights, sorted
jq -r 'select(.reach=="open") | .height' netmon-data/nodes.ndjson | sort -n | uniq -c
```

Deleting the file starts a fresh crawl from the seeds; nothing else depends on
it. A file that will not parse is a hard startup failure rather than a silent
reset, because the alternative loses every day of history it held.

---

## Troubleshooting

### The dashboard loads but every panel says it cannot reach the monitor

The page calls `/api/...` on its own origin. Check the API directly:

```bash
curl -s http://127.0.0.1:17871/api/summary | head -c 200
```

If that works and the page does not, the browser is on a different origin —
either serve the files from the binary with `--web-root`, put both behind one
nginx server block, or set `--enable-cors`.

### Every node shows as CLOSED or TIMEOUT

Almost always host egress filtering. Peers advertise arbitrary ports, so a
firewall that allows only 17855 outbound will report most of the network as
unreachable. Test one by hand:

```bash
# take an address from the table and try its advertised port
nc -vz 195.7.5.101 17855
```

### `Sweep found no addresses to probe`

The seeds did not resolve or did not answer. Check DNS, then try an explicit
address:

```bash
wrkz-netmon --seed-node 195.7.5.101:17855 --log-level debug
```

`--log-level debug` prints one line per failed probe with the reason.

### No software versions anywhere

Expected. `rpcInterface` defaults to `127.0.0.1`, so most nodes never answer an
RPC probe at all. `--probe-rpc` asks the ones that do, and the Versions page
says how many that turned out to be.

### The map is empty but countries are listed

The country database loaded but the country has no centroid in the front end.
`CENTROIDS` in `extras/netmon/app.js` covers the codes seen on this network;
add any that turn up. A missing centroid affects only the bubble, never the
count.

### The map is empty and no countries are listed

No database is loaded. See [Location data](#location-data-db-ip-lite). The page
says so explicitly rather than drawing an empty map.

### `--web-root is not a directory`

Checked at startup on purpose. `set_mount_point` merely returns false for a bad
path, and a dashboard that 404s every asset with nothing in the log to explain
it is a bad afternoon.

### It stopped finding new nodes

Normal once the crawl has converged — a network of a few hundred reachable nodes
is fully mapped within a couple of sweeps, and after that "new addresses 0" is
the correct answer. The grey count keeps growing slowly as nodes churn.
