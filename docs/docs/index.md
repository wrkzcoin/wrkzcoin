# WrkzCoin Documentation

Reference and operator documentation for the WrkzCoin daemon, wallets and RPC
surfaces. The source of truth is always the code in
[wrkzcoin/wrkzcoin](https://github.com/wrkzcoin/wrkzcoin); these pages track the
`development` branch.

## Start here

| I want to&nbsp;… | Go to |
| --- | --- |
| Get a binary | [Building from Source](guides/building.md), or [latest.wrkz.work](https://latest.wrkz.work) |
| Run a node | [Running a Node](guides/running-daemon.md) |
| Look up a daemon flag | [Configuration Reference](guides/daemon-configuration.md) |
| Run a node on a small disk | [Lite Nodes](guides/lite-node.md) and [Snapshots](guides/lite-snapshots.md) |
| Mine | [Solo Mining](guides/solo-mining.md) |
| Build on the daemon RPC | [Daemon RPC Overview](daemon-rpc/overview.md) |
| Build a payment integration | [Wallet API Overview](wallet-api/overview.md) |
| Use a wallet | [Wallet CLI](guides/wallet-cli.md) or the [wallet apps](guides/wallet-apps.md) |
| Look up an error code | [Wallet Error Codes](guides/error-codes.md) |
| Check a fork height or ring size | [Network Parameters](guides/network-parameters.md) |
| See what changed | [Changelog](changelog/daemon.md) |
| Build a wallet app | [Building the Wallet Apps](guides/building-wallet-apps.md) |
| Find a script or a small binary | [Other Tools](guides/other-tools.md) |
| Talk to somebody | [Community](community.md) |

## Binaries

| Binary | What it is |
| --- | --- |
| `Wrkzd` | The daemon: P2P node, RPC server, optional stratum server |
| `wrkz-wallet` | Interactive wallet CLI |
| `wrkz-wallet-api` | HTTP wallet API, for services and the wallet apps |
| `wrkz-service` | Legacy JSON-RPC wallet service (payment service) |
| `wrkz-txpow-server` | Optional transaction proof-of-work helper |
| `miner` | Reference CPU miner |
| `cryptotest` | Hash function self-test |
| `wallet-upgrader` | Converts old wallet container files |

The Flutter desktop, mobile and web wallets live under `extras/` and are built
separately; see [Desktop, Mobile and Web Wallets](guides/wallet-apps.md).

## Default ports

| Port | Service |
| --- | --- |
| `17855` | P2P |
| `17856` | Daemon RPC |
| `17857` | ZMQ publisher (`--zmq-pub`) |
| `17858` | Stratum, by convention — off unless `--stratum-bind-port` is given |
| `17870` | `wrkz-txpow-server`, if you run one |
| `7856` | `wrkz-wallet-api` **and** `wrkz-service` |

`wrkz-wallet-api` and `wrkz-service` share the same default port
(`SERVICE_DEFAULT_PORT` in `src/config/CryptoNoteConfig.h`). Give one of them a
different `--port` / `--bind-port` if you run both on one machine.

## For AI agents and scripts

Every page is published as plain markdown as well as HTML, so a model or a
script can read the docs without parsing the rendered site.

| URL | What it is |
| --- | --- |
| [`/llms.txt`](https://docs.wrkz.work/llms.txt) | An index in the [llmstxt.org](https://llmstxt.org) format: every page, grouped by section, one line of description each |
| [`/llms-full.txt`](https://docs.wrkz.work/llms-full.txt) | The entire documentation set concatenated as one markdown file |
| `<any page URL> + .md` | The markdown source of that page — e.g. [`/guides/lite-node.md`](https://docs.wrkz.work/guides/lite-node.md) |

Point a coding agent at `/llms.txt` and let it fetch the pages it needs; paste
`/llms-full.txt` when you want the whole thing in context at once. All three are
regenerated on every docs build, so they never lag the site.

**They describe the documented behaviour, not your node.** Version-specific
answers still need `Wrkzd --version` and the [changelog](changelog/daemon.md).

## Building the documentation

These pages are MkDocs Material sources under `docs/` in the repository.

```bash
pip install -r docs/requirements.txt
mkdocs serve -f docs/mkdocs.yml
mkdocs build -f docs/mkdocs.yml
```

Every page names the file it documents. When a route, a method handler or a
command-line flag changes, update the page that names it in the same change.
