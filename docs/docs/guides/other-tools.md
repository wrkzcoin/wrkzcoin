# Other Tools

Smaller binaries, scripts and components that ship with the repository. Each one
is small enough not to need a page of its own, but none of them are documented
anywhere else.

## `wallet-upgrader`

Converts an old wallet container file to the current format. Only needed when
`wrkz-wallet` refuses a file with
[`6 UNSUPPORTED_WALLET_FILE_FORMAT_VERSION`](error-codes.md).

```bash
wallet-upgrader --wallet-file old.wallet --password "wallet-pass"
```

| Option | Meaning |
| --- | --- |
| `-w, --wallet-file <file>` | Container to upgrade |
| `-p, --password <pass>` | Its password |
| `-h, --help`, `-v, --version` | Print and exit |

Run with **neither** flag it prompts for the filename interactively, and offers
to add the `.wallet` extension when the file lacks it. Give both flags for a
non-interactive upgrade — the extension prompt is skipped in that case, so the
file is upgraded under whatever name you passed.

**Back the file up first.** The upgrade rewrites the container in place, and a
wallet written in the newer format cannot be opened by an older binary.

## `cryptotest`

Hash-function and crypto self-test. See
[The Miner App](miner.md#cryptotest).

## Block explorer

`extras/explorer` is a **static** block explorer UI — HTML, CSS and JavaScript,
no build step and no server-side component. It is served by an ordinary web
server while the daemon runs separately.

The daemon must run in explorer mode, which is what registers the
`f_*` JSON-RPC methods and `/queryblocksdetailed`:

```bash
Wrkzd --daemon-mode explorer --enable-cors
```

By default the page expects the daemon RPC at `/api`, so the usual production
layout is nginx serving the static files and reverse-proxying `/api` to the
daemon — which also avoids CORS entirely, making `--enable-cors` unnecessary.
For local work, any static file server will do:

```bash
cd extras/explorer
python -m http.server 8080
```

| File | What it is |
| --- | --- |
| `index.html`, `app.js`, `style.css` | The page |
| `vendor/wrkz-crypto.js` | Self-contained address and key primitives used by the wallet tools |
| `vendor/TurtleCoinUtils.js` | Vendored client-side crypto helper used by Check Transaction |
| `test/` | Node test suites for the crypto module and the page wiring |

!!! warning "Explorer mode is not for a plain public node"
    Explorer routes walk database indexes and are not something a node should
    answer for anyone who asks. Put a token or a proxy in front of it, and note
    that explorer mode **cannot** be combined with [lite mode](lite-node.md).

## Checkpoints

Checkpoints let a syncing node skip proof-of-work and ring-signature
verification below the last one, which is most of what makes a first sync take
hours rather than days. The daemon ships with compiled-in checkpoints and also
accepts a CSV file:

```bash
Wrkzd --load-checkpoints checkpoints.csv
```

The format is one `height,hash` line per checkpoint:

```
0,877e55b4e902b9bf4c9e0a7c16440f449339d56679c49d62261ae5c92596a6ce
1,93bb1fd850d9e904ca810cdb57935b6df45cd75fc3a86358a421e126c1ae7b51
```

Published checkpoints are at
[checkpoints.wrkz.work](https://checkpoints.wrkz.work/)
([direct CSV](https://checkpoints.wrkz.work/checkpoints.csv)).

### Generating your own

`scripts/checkpoints/gen_checkpoints.sh` walks a **synced** daemon over RPC and
writes the file:

```bash
./gen_checkpoints.sh -H 127.0.0.1 -p 17856 -o checkpoints.csv
```

| Option | Default | Meaning |
| --- | --- | --- |
| `-H, --host` | `127.0.0.1` | Daemon RPC host |
| `-p, --port` | `17856` | Daemon RPC port |
| `--token` | | `X-API-Key` value, if the daemon runs with an access token |
| `-s, --start` | `0`, or the resume point | First checkpoint height |
| `-e, --end` | top height minus confirmations | Last checkpoint height |
| `--step` | `1` for csv, `1000` otherwise | Interval between checkpoints |
| `--confirmations` | `180` | Stay this many blocks behind the tip. `0` goes to the top |
| `-c, --checkpoint-file` | | Existing file to resume from |
| `-o, --output` | `checkpoints.csv` | Output file, or `-` for stdout |
| `-m, --mode` | `csv` | `csv`, `append` (C++ entries to paste into `CHECKPOINTS`), `full` (a complete `CryptoNoteCheckpoints.h`), or `raw` |

The `--confirmations` default exists so a checkpoint is never written for a
block that could still be reorganised away.

## `wallet_capi` — the embedder C API

The desktop, mobile and web wallets do not talk to `wrkz-wallet-api` over HTTP;
they link `wallet_capi`, the wallet backend built as a shared library, and call
it through FFI (or WebAssembly). It exposes around 70 `wallet_*` functions
covering the whole wallet lifecycle, and returns the same
[error codes](error-codes.md) as every other surface, with
`wallet_error_code_to_string` to render them.

Two entry points worth knowing:

- `wallet_set_tx_pow_server(host, port, ssl)` — point the backend at a
  [transaction PoW server](txpow-server.md). An empty host turns it off. The
  WASM bridge exposes the same call as `setTxPowServer`.
- `wallet_capi_api_version` / `wallet_capi_version_string` — check what a
  loaded library actually is before calling into it.

There is no generated reference for this API yet; `src/walletcapi/wallet_capi.cpp`
is the source of truth, and
[Building the Wallet Apps](building-wallet-apps.md) covers producing the library.

## ZMQ subscriber test script

`scripts/zmq_sub_test.py` is a working subscriber for the daemon's
[ZMQ publisher](networking.md#zmq-publisher) — the quickest way to confirm a
build has ZeroMQ compiled in and that events are actually flowing.

## Build and packaging scripts

| Path | What it does |
| --- | --- |
| `scripts/docker/` | One-command release packaging for Linux, Windows, Android and the wallet apps |
| `scripts/cross-platform/` | Linux-hosted cross-build flows for aarch64, Windows, Android and macOS |
| `scripts/build-linux-wallet-lib.sh` | Static Linux `wallet_capi` |
| `scripts/cross-build-windows-wallet-lib.sh` | Windows `wallet_capi.dll` from Linux |
| `scripts/cross-build-android-wallet.sh`, `scripts/build-libucontext-android.sh` | Android `wallet_capi` and its dependency |
| `scripts/package-macos.sh`, `scripts/package-windows-x86_64.sh` | Packaging steps for those targets |
| `scripts/scan-payment-id-tags.py` | Audits extra sub-tag usage on chain — see [Encrypted Payment IDs](encrypted-payment-ids.md) |

See [Building from Source](building.md) for how these fit together.
