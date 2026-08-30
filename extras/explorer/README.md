# WrkzCoin Explorer

This folder contains a static block explorer UI for WrkzCoin.

It is intended to be served by a normal web server such as nginx while the
WrkzCoin daemon runs separately with explorer RPC enabled.

## Requirements

- A built and running WrkzCoin daemon
- Explorer RPC enabled on the daemon
- A web server to serve the static files in this folder

## Daemon

Run the daemon with explorer mode enabled.

Example:

```powershell
.\Wrkzd.exe --daemon-mode explorer --enable-cors
```

Notes:

- `--daemon-mode explorer` is required for the explorer-specific RPC methods
- `--enable-cors` is useful for direct browser access
- If you use nginx as a reverse proxy, browser CORS issues are usually avoided

## Files

- `index.html`: main explorer page
- `app.js`: frontend logic
- `style.css`: explorer styling
- `vendor/wrkz-crypto.js`: self-contained address and key primitives used by the wallet tools
- `vendor/TurtleCoinUtils.js`: vendored client-side crypto helper used by Check Transaction
- `test/`: Node test suites for the crypto module and the page wiring

## Local Development

Serve this folder with any static file server.

Example with Python:

```powershell
cd extras\explorer
python -m http.server 8080
```

Then open:

```text
http://127.0.0.1:8080/
```

By default, the explorer expects the daemon RPC to be reachable through `/api`.

## Production Deployment

Recommended layout:

- nginx serves the static files in `extras/explorer`
- nginx proxies `/api` to the daemon RPC port

This keeps the browser on one origin and avoids cross-origin issues.

## nginx Sample

Example server block:

```nginx
server {
    listen 80;
    server_name explorer.example.com;

    root /var/www/wrkz-explorer;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://127.0.0.1:17856/;
        proxy_http_version 1.1;

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## Deploy Steps

1. Build the daemon with explorer support.
2. Restart the daemon with `--daemon-mode explorer`.
3. **Bump the cache-busting stamp** (see below).
4. Copy the contents of this folder to your web root, including `vendor/`.
5. Configure nginx to serve the static files and proxy `/api/` to the daemon.
6. Open the explorer in a browser and load a block or transaction page.

Copy the whole directory, not individual files. `index.html` loads
`vendor/wrkz-crypto.js`, so shipping the markup without the vendor directory
leaves every `Tools` menu item reporting that the crypto module failed to load.

### Cache-busting stamp

`index.html` references its stylesheet and scripts with a `?v=` query string:

```html
<link rel="stylesheet" href="style.css?v=20260830" />
<script src="vendor/wrkz-crypto.js?v=20260830"></script>
<script src="app.js?v=20260830"></script>
```

**Bump all three to the same new value on every deploy.** A CDN in front of the
explorer typically caches `.css` and `.js` but not `.html`, so without this a
deploy can leave browsers running a brand new `index.html` against a stale
`app.js` and `style.css`. That fails worse than being plainly out of date: the
new markup renders, but the old script has no handlers for it, so the page looks
subtly broken rather than merely old.

Changing the value makes them new URLs, so the CDN fetches them fresh and no
purge is needed. On Linux:

```bash
sed -i "s/?v=[0-9]\{8\}/?v=$(date +%Y%m%d)/g" index.html
```

`vendor/TurtleCoinUtils.js` is injected at runtime by `app.js`, which reuses the
same stamp automatically, so it has no `?v=` of its own to maintain.

`node test/wiring.test.js` fails if the three stamps ever disagree or if one is
missing, which is the case a manual edit is most likely to get wrong.

## Wallet and Address Tools

The `Tools` menu in the header holds four utilities. All four run entirely in
the browser against `vendor/wrkz-crypto.js`. None of them contacts the daemon,
so they keep working when the explorer's RPC backend is down, and they work from
a copy of this folder opened straight off disk.

| Tool | Route | What it does |
| --- | --- | --- |
| Paper Wallet | `#/paper` | Generates a new wallet and shows the address, 25-word seed, and both key pairs |
| Import from Seed | `#/import` | Recovers the same key set from a 25-word mnemonic or a 64-character private spend key |
| Integrated Address | `#/integrated` | Packs a payment ID into a standard address |
| Decode Address | `#/decode` | Verifies any address and extracts its keys, plus the payment ID if it is integrated |

Searching for an address in the header search box jumps straight to the decoder.

### Searching by payment ID

The header search box accepts a long payment ID as well as a block height, block
hash, transaction hash and address. Since a long payment ID is 64 hex characters
and so is a transaction hash and a block hash, the search tries them in that
order and falls back to the payment ID, which names a set of transactions rather
than one.

A payment ID with results routes to `#/paymentid/<hex>`, listing every
transaction that carries it.

This needs the daemon in explorer mode (`--daemon-mode explorer`), because it
reads a database index. A node reused heavily enough to exceed the node's cap
returns a truncated list, and the page says so rather than quietly showing part
of the answer.

**Short payment IDs cannot be looked up.** They are encrypted against the shared
secret between sender and receiver, so the same payment ID is different bytes in
every transaction and there is nothing stable to index. Searching for one routes
to a page that explains this, rather than reporting "not found" — which would
wrongly suggest the payment ID had never been used.

### Payment ID lengths

WrkzCoin accepts both payment ID forms, and the tools handle each:

- 16 hex characters produces a 120-character integrated address
- 64 hex characters produces a 186-character integrated address

Note that the vendored `TurtleCoinUtils.js` only decodes the 64-character form.
The tools deliberately do not use it, which is why the 16-character form works
here.

### Offline use

For a paper wallet that will hold real funds, do not trust a hosted copy of this
page. Save the folder to disk, disconnect from the network, and open
`index.html` directly. The block explorer views will fail without a daemon, but
every tool in the `Tools` menu keeps working, because none of them makes a
network request.

Keys are generated with `crypto.getRandomValues`. If a browser does not expose
it, the tool refuses to generate rather than falling back to a weaker source.

### Correctness

`vendor/wrkz-crypto.js` reimplements the daemon's primitives in plain JavaScript
rather than wrapping a bundled library. Each function names the C++ routine it
mirrors. The test suite checks the two against each other:

```powershell
cd extras\explorer
node test\crypto.test.js
node test\wiring.test.js
node test\ui.test.js
```

`crypto.test.js` reads the address prefix from `src/config/CryptoNoteConfig.h`,
the payment ID and address lengths from `src/config/WalletConfig.h`, and the
mnemonic word list from `src/mnemonics/WordList.h`, so the browser tools cannot
drift away from the daemon without the tests failing. It also checks Keccak-256,
CRC32 and ed25519 against published vectors, round-trips a real WrkzCoin
address, and generates 300 wallets to confirm every one re-imports to the same
address from both its seed and its spend key.

`wiring.test.js` cross-references `index.html`, `app.js` and `style.css` — every
element id the script reaches for, every route, every new CSS class — and
asserts that the tools section of `app.js` contains no network call.

`ui.test.js` boots `app.js` against a minimal DOM shim and clicks through all
four tools, including generating a wallet, copying the seed out of the rendered
page, and importing it back to the same address.

All three suites need only Node; there are no npm dependencies.

## Check Transaction

The `Check Transaction` feature runs entirely in the browser.

It does not send private keys to the server.

It relies on:

- transaction data already loaded on the page
- the recipient address
- a 64-character private key entered by the user
- `vendor/TurtleCoinUtils.js`

For view-key based checking to work, the transaction JSON exposed by the daemon
must include the transaction public key.

## Troubleshooting

### Explorer page loads but data fails

Check:

- daemon is running
- daemon was started with `--daemon-mode explorer`
- nginx `/api/` proxy points to the correct daemon port

### Check Transaction shows no matches

Check:

- the address is correct
- the private key is correct
- the transaction page JSON includes the transaction public key
- the daemon was restarted after any RPC changes

### The page looks half-broken after a deploy

Symptoms: the `Tools` button renders as an unstyled box, its caret is huge, and
clicking it does nothing. That means the browser has the new `index.html` but an
old `app.js` and `style.css` — no handlers are bound and none of the new rules
exist.

Check what is actually being served, bypassing the CDN cache:

```bash
curl -sI "https://your-explorer/app.js?cb=$RANDOM" | grep -i last-modified
curl -sI  https://your-explorer/app.js            | grep -i last-modified
```

If those two dates differ, the CDN is holding a stale copy. Bump the
cache-busting stamp (see "Deploy Steps") so this cannot happen again, and purge
`app.js` and `style.css` once to clear the copies already cached.

### A file returns 404 even though it is on disk

Confirm nginx is serving the directory you deployed to. Compare the size of a
file the browser gets against the one you copied:

```bash
curl -sI https://your-explorer/vendor/TurtleCoinUtils.js | grep -i content-length
ls -l /path/you/deployed/to/vendor/TurtleCoinUtils.js
```

If they differ, the `root` in your nginx server block points somewhere else and
your deploy went to an unused directory. `nginx -T | grep -E 'server_name|root'`
will show the path actually in use.

### Tools menu items show "wrkz-crypto.js failed to load"

Confirm `vendor/wrkz-crypto.js` was copied to the web root alongside
`index.html`, and that the web server serves `.js` files from `vendor/`.

### Copy buttons do nothing

`navigator.clipboard` requires a secure context. Over plain HTTP on a hostname
other than `localhost`, the page falls back to `document.execCommand('copy')`,
which some browsers refuse. Serve the explorer over HTTPS, or select the text
manually.

### Browser shows RPC or CORS errors

If you access the daemon directly from the browser, confirm:

- `--enable-cors` is set

If you use nginx reverse proxy, prefer routing through `/api` instead.
