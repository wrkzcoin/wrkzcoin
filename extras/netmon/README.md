# WrkzCoin Network Monitor — dashboard

The static front end for `wrkz-netmon`. Three files and a test folder; no build
step, no npm, no dependencies.

**The full guide lives in [NETMON.md](../../NETMON.md)** at the repo root:
building the binary, fetching the DB-IP location files, the crawler etiquette
rules, and an nginx layout. This file covers only the front end.

## Running it

The binary serves this folder itself, which is the simplest way to see it:

```bash
wrkz-netmon --web-root extras/netmon
# then open http://127.0.0.1:17871/
```

Behind a web server, copy the folder to the web root and proxy `/api/` to the
monitor — see the nginx section of NETMON.md.

## Files

| File | What it is |
| --- | --- |
| `index.html` | The page shell: header, the five page containers, footer |
| `app.js` | Router, API client and every renderer |
| `style.css` | Theme tokens and components |
| `test/` | Node test suites, no npm dependencies |

## The API it expects

Everything goes through one helper against `/api`:

| Endpoint | Used by |
| --- | --- |
| `/api/summary` | Overview |
| `/api/stats` | Nothing here — aggregates for bots and embeds, see NETMON.md |
| `/api/peers` | Peers table |
| `/api/peers/<address:port>` | Node detail |
| `/api/geo` | Geography |
| `/api/versions` | Versions |

`/api` is not negotiable and is not a preference. httplib checks its static
mount points **before** its route handlers, so with the dashboard mounted at
`/` a file in this folder would shadow a same-named route. Namespacing the API
makes that collision impossible. There is a note to the same effect in
`src/netmon/HttpApi.h`.

## Measured vs. reported

Every peer object from the API separates two kinds of fact, and the UI keeps
them apart on every screen:

- **top level** — what the crawler measured: reachability, RTT, uptime, first
  and last seen
- **`reported`** — what the node claimed in its handshake: version, height, top
  block hash, capability flags. A node can say anything here
- **`location`** — from a GeoIP database. Neither measured nor claimed

Do not merge those into one flat row. The distinction is the reason a reader can
trust the reachability figures at all.

## Deploying

1. `rsync -a --delete extras/netmon/ /var/www/wrkz-netmon/`
2. **Bump the `?v=` stamp** in `index.html` (see below)
3. Reload nginx if its config changed

### Cache-busting stamp

`index.html` references its stylesheet and script with a `?v=` query string:

```html
<link rel="stylesheet" href="style.css?v=20260907" />
<script src="app.js?v=20260907"></script>
```

**Bump both to the same new value on every deploy.** A CDN typically caches
`.css` and `.js` but not `.html`, and `wrkz-netmon` sends
`Cache-Control: no-cache` for the assets it serves — so without a stamp bump a
deploy can leave browsers running a brand new `index.html` against a stale
`app.js`. That fails worse than being plainly out of date: the new markup
renders, the old script has no handlers for it, and the page looks subtly broken
rather than merely old.

```bash
sed -i "s/?v=[0-9]\{8\}/?v=$(date +%Y%m%d)/g" index.html
```

`test/wiring.test.js` fails if the two stamps ever disagree, which is what a
manual edit is most likely to get wrong.

## Tests

```bash
cd extras/netmon
node test/wiring.test.js
node test/tokens.test.js
```

`wiring.test.js` cross-references the three files: every element id `app.js`
reaches for exists in the markup, every page in `PAGES` has a section and
exactly one starts active, every `data-nav` has a matching `setNav` call, every
CSS class used has a rule, every expected endpoint is called, all network
traffic goes through the single `fetch`, and every `localStorage` access sits in
a `try`/`catch`.

`tokens.test.js` diffs the `:root[data-theme=...]` blocks of this stylesheet
against `extras/explorer/style.css` and fails on any difference. The two pages
share a theme by copy, not by import, so this is the only thing stopping them
drifting into looking like different products. **When it fails, reconcile both
files** — do not "fix" one side.

## Theme

Dark by default, light available, remembered in `localStorage` under
`netmon-theme`. Every access is wrapped: `localStorage` throws outright in some
browser configurations, and an unguarded read takes the whole page down on load.

## Adding a country to the map

`CENTROIDS` in `app.js` maps a two-letter code to a lat/lon pair. It covers the
codes seen on this network; add any that turn up. A country with no centroid
still appears in the side panel with its count — it just has no bubble.

The projection is equirectangular, `x = (lon + 180) × 2.7778` and
`y = (90 − lat) × 2.7778` over a `0 15 1000 385` viewBox. The continent outlines
in `MAP_LAND` are deliberately coarse: they are a backdrop for the bubbles,
which sit at real centroids.

## Offline

The page makes no external requests — no CDN, no web font, no analytics. Opened
from disk without a monitor running it renders the shell and reports that it
cannot reach the API, which is the honest answer.
