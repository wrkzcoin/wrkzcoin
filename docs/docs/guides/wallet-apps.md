# Desktop, Mobile and Web Wallets

Three graphical wallets live under `extras/` in the repository. All three are
Flutter apps over `wallet_capi` — the same wallet backend `wrkz-wallet` and
`wrkz-wallet-api` use, built as a shared library and reached through FFI (or
WebAssembly in the browser). There is no wallet daemon behind them and no
sidecar process.

| | Source | Platforms | Storage |
| --- | --- | --- | --- |
| **Desktop** | `extras/desktop-wallet` | Windows, Linux, macOS | Wallet files on disk; settings in the platform secure store |
| **Mobile** | `extras/mobile-wallet` | Android, iOS | `<app docs>/wallets/`, one registry plus `.wallet` / `.wallet.keys` per wallet |
| **Web** | `extras/web-wallet` | Any browser meeting the requirements below | IndexedDB, in the browser |

Each folder has its own `README.md` with build and packaging instructions; this
page covers what an operator or a user needs to know about how they behave.

## Common ground

- Multiple wallets, identified by a caption and switched from Settings or a
  picker.
- Create, import from seed, import from keys, view-only import.
- Prepare-then-send with a fee preview, and a **Sweep All** mode for
  consolidating many small inputs.
- Integrated address generation with short or long payment IDs — see
  [Encrypted Payment IDs](encrypted-payment-ids.md).
- History with search and direction filters.
- A daemon node setting with presets, and a **Test** button that checks a node
  before switching to it.
- A **Transaction PoW Server** setting, off by default — see
  [Transaction PoW Server](txpow-server.md).
- Lite-node awareness: all three read `lite_start_height` from the connected
  node and say what it can serve.

The desktop default node is `nodes.wrkz.work:17856`.

## Lite-node behaviour

Every app reads `daemonLiteStartHeight` and `isSyncStalledByLiteNode` from the
wallet status and reacts to both:

- A standing notice names the lowest block the connected node holds. It stays up
  once the wallet is synced — which is exactly when a balance missing its older
  half looks most trustworthy.
- The node setting shows *Serves blocks from*: a height for a lite node, *Full
  chain* otherwise.
- **Rescan is floored at the node's start height.** Asking for less is refused
  in the app and refused again by the wallet backend (error 62), because a lite
  node answers a lower request from its own start anyway, which would move the
  recorded scan position over blocks nobody looked at.

If the node starts above the wallet's own start height, the notice becomes a
warning: funds received in between cannot be found through that node and the
balance reads low. Connect a node holding the whole chain to see them. Full
background is in [Lite Nodes](lite-node.md).

## Desktop: running a node for you

Settings has a **Local Lite Node** card that supervises a `Wrkzd` child process
on the same machine, so the wallet can sync against a node the user owns.

The daemon is a **separate binary**, not bundled by the Flutter build. It is
looked for in `$WRKZ_DAEMON_PATH`, next to the wallet executable, in a
`sidecar/` folder beside it, in the macOS bundle's `Contents/Resources`, and in
the working directory. Build the normal `Wrkzd` target and drop it next to the
app — about 9 MB on a release. If it is absent the card says so and nothing else
changes.

| | |
| --- | --- |
| Data directory | `<app support>/node`, with its own config, pid file and log |
| Ports | An ephemeral RPC and P2P port picked once and stored, so it never collides with a daemon the user runs themselves |
| RPC binding | `127.0.0.1` only, **unauthenticated** |
| Console | `--no-console`; the process is stopped by signal |
| Restarts | A node that was running comes back on the next launch. One stopped on purpose stays stopped |
| Quitting the app | **The node keeps running.** A first sync takes hours and is not worth throwing away every time the window closes. Stop it from Settings |
| Crash recovery | A node left behind by a force-quit app is found by its port and adopted rather than started twice on the same database |

!!! warning "The managed node's RPC has no token"
    `--rpc-access-token` cannot be used here: the wallet backend builds its own
    request headers and has no way to send one. Anything running as any user on
    the machine can reach this node. It binds loopback only.

**The start height is the whole decision.** The setup wizard defaults to the
wallet's own start height. Choosing higher is allowed but needs an explicit
acknowledgement, because a node starting above the wallet can never show that
wallet's older transactions. The height cannot be changed afterwards — the
daemon refuses to start against a database built at a different one — so
changing it means deleting the node and syncing again.

The setup dialog can also **import a lite node snapshot** instead of syncing the
index region, which turns hours into minutes. See
[Lite Node Snapshots](lite-snapshots.md).

**Switching between nodes.** The node's life is independent of the wallet's
connection: it keeps syncing whether or not the wallet points at it, so the
usual flow is to stay on a remote node for the hours the first sync takes and
switch over when it is ready. *Use this node* stays disabled until the node
reports itself synced. Stopping or deleting the node moves the wallet back to
the remote one automatically rather than leaving it addressing a dead port.

## Mobile: no node on the phone yet

There is no embedded node on Android or iOS, and it is a longer way off than the
desktop version was:

- The Android build profile forces `WRKZ_BUILD_EXECUTABLES=OFF`, and RocksDB is
  only built when executables are on, so the daemon has never been compiled for
  Android in this tree.
- Size does not follow the start height. Key output info is written for every
  block from genesis, so a lite node is around 6 GB whatever height it starts
  at, and it downloads the whole chain to build that index.
- A first sync is hours of continuous work, against Android's doze and
  background execution limits.

Until then, point the wallet at a node you run yourself — a desktop machine on
the same network works well, and the desktop app can host one for you.

Mobile also has biometric unlock, auto-lock on backgrounding, incoming
transaction notifications, and QR scanning for send and receive.

## Web: what the browser needs

The wallet library is compiled to WebAssembly with Emscripten and runs entirely
in the page; wallet data is persisted in IndexedDB and never leaves the browser.

Because the WASM build uses pthreads, **the page must be served
cross-origin-isolated** — `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp` — or `SharedArrayBuffer` is
unavailable and the wallet cannot start. `extras/web-wallet/README.md` has
working nginx, Caddy, Docker and static-host configurations.

The web wallet is also the one platform where transaction proof of work is
genuinely expensive. Without a [Tx PoW server](txpow-server.md) configured it
pays the 100 WRKZ bypass fee instead of computing the proof; with one, it pays
the normal minimum fee.

## Building

Each app's `README.md` carries the full recipe. In outline:

1. Build `wallet_capi` from this tree (`-DWRKZ_BUILD_WALLET_CAPI=ON`), or
   `wallet_wasm` with Emscripten for the web wallet.
2. Place the resulting library where the Flutter build expects it.
3. `flutter build <windows|linux|macos|apk|web>`.

The step-by-step version, per platform, is in
[Building the Wallet Apps](building-wallet-apps.md).

`scripts/docker` builds release packages for the CLI binaries and for the web,
desktop and mobile wallet targets — see `COMPILE.md`.
