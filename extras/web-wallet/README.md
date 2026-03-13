# PLUTON Web — WRKZ Web Wallet

Browser-based wallet for WrkzCoin, powered by WebAssembly. The C/C++ wallet
library (`wallet_capi`) is compiled to WASM with Emscripten; the UI is built
with Flutter Web. Wallet data is persisted in the browser's IndexedDB.

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│  Flutter Web (Dart)                                  │
│  ┌────────────┐  dart:js_interop  ┌───────────────┐  │
│  │ wallet_web  │ ───────────────► │ walletBridge  │  │
│  │   .dart     │                  │    .js        │  │
│  └────────────┘                  └───────┬───────┘  │
│                                          │          │
│                            cwrap / ccall │          │
│                                          ▼          │
│                                  ┌───────────────┐  │
│                                  │ wallet_wasm   │  │
│                                  │  .wasm        │  │
│                                  └───────┬───────┘  │
│                                          │          │
│                            base64 I/O    │          │
│                                          ▼          │
│                                  ┌───────────────┐  │
│                                  │ IndexedDB     │  │
│                                  │ (wallet_      │  │
│                                  │  storage.js)  │  │
│                                  └───────────────┘  │
└──────────────────────────────────────────────────────┘
```

**Data flow:** Dart → JS interop → `wallet_bridge.js` → Emscripten `cwrap` →
`wallet_wasm_request()` (C++) → `wallet_capi` → WalletBackend. File I/O is
redirected to an in-memory store (`wasm_fs_bridge.h`) which is synced to
IndexedDB via base64 import/export.

---

## Requirements

| Component        | Version / Notes                                  |
|------------------|--------------------------------------------------|
| **Flutter SDK**  | 3.29+ (Dart SDK ^3.10.7)                         |
| **Emscripten**   | 3.1.50+ (for WASM build)                         |
| **CMake**        | 3.16+                                            |
| **Node.js**      | 18+ (optional — only for local dev server)       |
| **Browser**      | Chrome 89+, Firefox 89+, Safari 15.2+, Edge 89+  |

### Browser requirements

- **WebAssembly** support (all modern browsers).
- **SharedArrayBuffer** — required if the WASM module is built with pthreads
  (`WRKZ_WASM_PTHREADS=ON`). The web server **must** send these headers:
  ```
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
  ```
  Without these headers the browser will refuse to allocate shared memory and
  the WASM module will fail to initialise.

---

## Building

### 1. Build the WASM module

```bash
# Install / activate Emscripten (if not already)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest
source ./emsdk_env.sh          # Linux / macOS
# emsdk_env.bat                # Windows

# From the repo root
mkdir build-wasm && cd build-wasm

emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DWRKZ_BUILD_WALLET_CAPI=ON \
  -DWRKZ_BUILD_WALLET_WASM=ON
  # Optional: -DWRKZ_WASM_PTHREADS=ON   (enables multi-threaded sync)

cmake --build . -j$(nproc)
```

Output files (in `build-wasm/wasm/`):

| File                     | Description                         |
|--------------------------|-------------------------------------|
| `wallet_wasm.js`         | Emscripten ES6 glue module          |
| `wallet_wasm.wasm`       | Compiled WebAssembly binary         |
| `wallet_wasm.worker.js`  | Web Worker (only with pthreads)     |

### 2. Copy WASM artefacts into the Flutter web directory

```bash
cp build-wasm/wasm/wallet_wasm.js   extras/web-wallet/web/
cp build-wasm/wasm/wallet_wasm.wasm extras/web-wallet/web/

# If built with pthreads:
cp build-wasm/wasm/wallet_wasm.worker.js extras/web-wallet/web/
```

Also copy the JS bridge files:

```bash
cp extras/web-wallet-wasm/wasm/js/wallet_bridge.js extras/web-wallet/web/
cp extras/web-wallet-wasm/wasm/js/wallet_storage.js extras/web-wallet/web/
```

### 3. Build the Flutter web app

```bash
cd extras/web-wallet

flutter pub get        # also generates l10n files
flutter build web      # production build → build/web/
```

The production bundle is in `extras/web-wallet/build/web/`.

---

## Running locally (development)

```bash
cd extras/web-wallet
flutter run -d chrome
```

> **Note:** `flutter run` uses its own dev server which does **not** set
> COOP/COEP headers by default. If the WASM module uses pthreads, use the
> manual method below instead.

### Manual dev server with COOP/COEP headers

```bash
# Build first
flutter build web

# Serve with correct headers (using Python)
cd build/web
python3 -c "
from http.server import HTTPServer, SimpleHTTPRequestHandler
class H(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()
HTTPServer(('localhost', 8080), H).serve_forever()
"
```

Then open http://localhost:8080.

---

## Deployment

### Static hosting (Nginx)

```nginx
server {
    listen 80;
    server_name web.domain.com;
    return 301 https://$host$request_uri;
}

server {
    listen 443 ssl http2;
    server_name web.domain.com;

    ssl_certificate     /etc/letsencrypt/live/web.domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/web.domain.com/privkey.pem;
    ssl_protocols TLSv1.2 TLSv1.3;

    root /path/to/extras/web-wallet/build/web;
    index index.html;

    # ── Same-origin proxy for daemon RPC ─────────────────────────────
    # Eliminates CORS entirely — set node URL to /daemon/ in the wallet.
    location /daemon/ {
        proxy_pass         https://node-fin.wrkz.work/;
        proxy_ssl_verify   off;
        proxy_set_header   Host              node-fin.wrkz.work;
        proxy_set_header   Origin            https://web.domain.com;
        proxy_set_header   X-Forwarded-For   $remote_addr;
        proxy_set_header   X-Forwarded-Proto https;
    }

    # ── SPA entry point — never cached ───────────────────────────────
    location = /index.html {
        add_header Cache-Control                "no-cache, no-store, must-revalidate" always;
        add_header Cross-Origin-Opener-Policy   "same-origin"  always;
        add_header Cross-Origin-Embedder-Policy "require-corp" always;
        add_header Cross-Origin-Resource-Policy "cross-origin" always;
        try_files $uri =404;
    }

    # ── WASM binary ──────────────────────────────────────────────────
    location ~* \.wasm$ {
        default_type application/wasm;
        add_header Cross-Origin-Opener-Policy   "same-origin"  always;
        add_header Cross-Origin-Embedder-Policy "require-corp" always;
        add_header Cross-Origin-Resource-Policy "cross-origin" always;
        try_files $uri =404;
    }

    # ── JS / ES-module workers ───────────────────────────────────────
    # CORP header so Emscripten pthread workers can load wallet_wasm.js
    # from within a COEP context.
    location ~* \.(js|mjs)$ {
        add_header Access-Control-Allow-Origin  "*"            always;
        add_header Access-Control-Allow-Methods "GET, OPTIONS" always;
        add_header Cross-Origin-Opener-Policy   "same-origin"  always;
        add_header Cross-Origin-Embedder-Policy "require-corp" always;
        add_header Cross-Origin-Resource-Policy "cross-origin" always;
        try_files $uri =404;
    }

    # ── Other static assets ──────────────────────────────────────────
    location ~* \.(css|png|jpg|jpeg|gif|svg|ico|webp|ttf|otf|woff|woff2)$ {
        add_header Cross-Origin-Resource-Policy "cross-origin" always;
        try_files $uri =404;
    }

    # ── SPA catch-all with full cross-origin isolation headers ───────
    location / {
        if ($request_method = OPTIONS) {
            add_header Access-Control-Allow-Origin  "*"              always;
            add_header Access-Control-Allow-Methods "GET, POST, OPTIONS" always;
            add_header Access-Control-Allow-Headers "*"              always;
            add_header Cross-Origin-Opener-Policy   "same-origin"   always;
            add_header Cross-Origin-Embedder-Policy "require-corp"  always;
            add_header Cross-Origin-Resource-Policy "cross-origin"  always;
            add_header Content-Length 0;
            add_header Content-Type   text/plain;
            return 204;
        }

        add_header Cross-Origin-Opener-Policy   "same-origin"  always;
        add_header Cross-Origin-Embedder-Policy "require-corp" always;
        add_header Cross-Origin-Resource-Policy "cross-origin" always;
        try_files $uri $uri/ /index.html;
    }
}
```

### Static hosting (Caddy)

```caddyfile
wallet.example.com {
    root * /var/www/pluton-web/build/web
    file_server
    try_files {path} /index.html

    header {
        Cross-Origin-Opener-Policy  "same-origin"
        Cross-Origin-Embedder-Policy "require-corp"
    }
}
```

### Static hosting (GitHub Pages / Cloudflare Pages / Vercel)

1. Push the contents of `build/web/` to your hosting provider.
2. Add a `_headers` file (Cloudflare/Netlify) or `vercel.json` to set the
   COOP/COEP headers. Example `_headers`:
   ```
   /*
     Cross-Origin-Opener-Policy: same-origin
     Cross-Origin-Embedder-Policy: require-corp
   ```

> **GitHub Pages** does not support custom response headers. If you need
> pthreads, use Cloudflare Pages, Vercel, or your own server.

### Docker

```dockerfile
FROM nginx:alpine
COPY build/web /usr/share/nginx/html
COPY nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
```

---

## Project structure

```
extras/web-wallet/
├── lib/
│   ├── app/              # App entry, router
│   ├── core/
│   │   ├── api/models/   # Balance, Transaction, WalletStatus
│   │   ├── auth/         # Password storage (flutter_secure_storage)
│   │   ├── config/       # Coin ticker, daemon defaults
│   │   ├── ffi/          # wallet_web.dart (JS interop → WASM)
│   │   └── providers/    # Riverpod providers & notifiers
│   ├── features/         # UI screens (overview, receive, transfer, etc.)
│   ├── l10n/             # ARB translation files (9 languages)
│   └── shared/           # Theme, formatters, reusable widgets
├── web/
│   ├── index.html        # Loads Flutter + WASM bridge
│   ├── manifest.json     # PWA manifest
│   ├── wallet_bridge.js  # ← copy from web-wallet-wasm/wasm/js/
│   ├── wallet_storage.js # ← copy from web-wallet-wasm/wasm/js/
│   ├── wallet_wasm.js    # ← copy from WASM build output
│   └── wallet_wasm.wasm  # ← copy from WASM build output
├── pubspec.yaml
└── l10n.yaml

extras/web-wallet-wasm/
└── wasm/
    ├── include/
    │   └── wasm_fs_bridge.h        # In-memory filesystem for WASM
    ├── js/
    │   ├── wallet_bridge.js        # JS wrapper around WASM module
    │   └── wallet_storage.js       # IndexedDB persistence layer
    └── src/
        └── wallet_wasm_exports.cpp # Emscripten entry point (JSON RPC)
```

---

## How it works

### Wallet lifecycle

1. **Page load** — `index.html` imports `wallet_bridge.js`, which dynamically
   loads `wallet_wasm.js` (Emscripten ES6 module). A `walletBridgeReady` event
   fires when initialisation completes.

2. **Create / Open** — The Flutter UI calls `WalletCApi.create()` or `.open()`
   (in `wallet_web.dart`), which calls `walletBridge.call('create', {...})`
   via `dart:js_interop`. The JS bridge invokes `_wallet_wasm_request()` in
   the WASM module.

3. **Persistence** — The WASM module uses an in-memory file store
   (`wasm_fs_bridge.h`). After create/save, `wallet_bridge.js` calls
   `exportFileData` to get the wallet bytes as base64, then writes them to
   IndexedDB via `wallet_storage.js`.

4. **Sync** — The wallet connects to a daemon node over HTTP(S). On web this
   goes through the browser's `fetch` / `XMLHttpRequest` (Emscripten's Nigel
   networking layer has `#if defined(__EMSCRIPTEN__)` support).

5. **Transactions** — Send/receive works identically to the desktop wallet.
   PoW mining for transactions runs in the WASM thread.

### Storage

| Store             | Contents                                    |
|-------------------|---------------------------------------------|
| `wallet_files`    | Encrypted wallet binary (IndexedDB)         |
| `wallet_meta`     | Last-opened wallet name, preferences        |
| `localStorage`    | Theme, language, notification preferences    |

All wallet data is encrypted by `wallet_capi` before reaching the browser.
The browser never sees private keys or seed phrases in plaintext.

---

## Internationalization

9 languages supported: English, French, German, Spanish, Portuguese, Chinese,
Vietnamese, Japanese, Russian.

Translation files are in `lib/l10n/app_*.arb`. To add a new language:

1. Copy `app_en.arb` to `app_XX.arb` (where `XX` is the ISO 639-1 code).
2. Translate the values.
3. Run `flutter pub get` to regenerate.

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| WASM fails to load | Check browser console. Ensure `wallet_wasm.js` + `.wasm` are in `web/`. |
| `SharedArrayBuffer is not defined` | Server is missing COOP/COEP headers. See Deployment section. |
| Wallet won't connect to daemon | The daemon must have CORS enabled or be behind a reverse proxy that adds `Access-Control-Allow-Origin`. |
| Blank screen after build | Run `flutter clean && flutter pub get && flutter build web`. |
| l10n errors / `S` undefined | Run `flutter pub get` — this generates `l10n/generated/`. |

---

## Security notes

- Private keys and seed phrases are handled entirely within the WASM sandbox.
  They never leave the WebAssembly linear memory except when explicitly
  requested by the user (e.g., "show seed phrase" screen).
- Wallet files stored in IndexedDB are encrypted with the user's password
  (same encryption as the desktop wallet).
- The web wallet is a **client-side application** — no data is sent to any
  server other than the daemon node for blockchain sync.
- For maximum security, serve over HTTPS and consider Content Security Policy
  headers that restrict script sources.

---

## License

Same license as the parent WrkzCoin repository.
