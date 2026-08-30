'use strict';

/* Static wiring checks for the explorer front end.

   There is no DOM in this test runner, so instead of rendering the page we
   cross-reference the three source files against each other. This catches the
   mistakes that a syntax check cannot: an element id that app.js reaches for
   but index.html never defines, a CSS class used in markup but never styled,
   and routes that exist in one place but not the other.

   Run with:  node test/wiring.test.js       (from extras/explorer) */

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(ROOT, 'index.html'), 'utf8');
const app  = fs.readFileSync(path.join(ROOT, 'app.js'), 'utf8');
const css  = fs.readFileSync(path.join(ROOT, 'style.css'), 'utf8');

let pass = 0, fail = 0;
const check = (name, ok, detail) => {
  if (ok) { pass++; console.log(`  ok   ${name}`); }
  else { fail++; console.log(`  FAIL ${name}${detail ? `\n         ${detail}` : ''}`); }
};

const uniq = a => [...new Set(a)];
const matchAll = (re, s, group = 1) => uniq([...s.matchAll(re)].map(m => m[group]));

// ─── element ids ─────────────────────────────────────────────────────────────

const htmlIds = new Set(matchAll(/\bid="([A-Za-z0-9_-]+)"/g, html));

/* Ids app.js looks up. Ids it creates at runtime and then reads back are listed
   as exemptions below. */
const usedIds = matchAll(/\$\('([A-Za-z0-9_-]+)'\)/g, app)
  .concat(matchAll(/getElementById\('([A-Za-z0-9_-]+)'\)/g, app))
  .concat(matchAll(/setText\('([A-Za-z0-9_-]+)'/g, app))
  .concat(matchAll(/setHtml\('([A-Za-z0-9_-]+)'/g, app));

/* Injected by render functions rather than declared in index.html. */
const RUNTIME_IDS = new Set(['ckAddr', 'ckKey', 'ckBtn', 'ckStatus', 'ckResult']);

const missingIds = uniq(usedIds).filter(id => !htmlIds.has(id) && !RUNTIME_IDS.has(id));
check('every id app.js reads exists in index.html or is created at runtime',
  missingIds.length === 0, `missing: ${missingIds.join(', ')}`);

// ─── routes ──────────────────────────────────────────────────────────────────

/* Each `case 'x':` in route() must have a matching #/x page container, and
   every #/x link in the markup must be a route the router knows. */
const routeCases = matchAll(/case '([a-z]+)':\s+return show/g, app);
const pageIds = matchAll(/\bid="page-([a-z]+)"/g, html);

const routesWithoutPage = routeCases.filter(r => !pageIds.includes(r));
check('every route has a page container', routesWithoutPage.length === 0,
  `no #page- element for: ${routesWithoutPage.join(', ')}`);

const linkedRoutes = matchAll(/href="#\/([a-z]+)"/g, html);
const unknownLinks = linkedRoutes.filter(r => !routeCases.includes(r));
check('every #/route link in the markup is handled by route()',
  unknownLinks.length === 0, `unhandled: ${unknownLinks.join(', ')}`);

const TOOL_ROUTES = ['paper', 'import', 'integrated', 'decode'];
const toolPagesLine = app.match(/const TOOL_PAGES = \[([^\]]*)\]/);
const declaredTools = toolPagesLine
  ? toolPagesLine[1].split(',').map(s => s.trim().replace(/'/g, '')).filter(Boolean)
  : [];
check('TOOL_PAGES matches the four tool routes',
  declaredTools.join(',') === TOOL_ROUTES.join(','),
  `got [${declaredTools.join(', ')}]`);

for (const r of TOOL_ROUTES) {
  check(`tool "${r}" is reachable from the Tools menu`,
    html.includes(`class="nav-menu-item" role="menuitem"`) && html.includes(`href="#/${r}"`));
}

// ─── css classes ─────────────────────────────────────────────────────────────

const cssClasses = new Set(matchAll(/\.([a-zA-Z][a-zA-Z0-9_-]*)/g, css));

/* Classes used in index.html and in app.js template literals. */
const htmlClasses = matchAll(/\bclass="([^"]+)"/g, html)
  .join(' ').split(/\s+/).filter(Boolean);

const NEW_CLASSES = [
  'nav-menu', 'nav-menu-btn', 'nav-menu-caret', 'nav-menu-list', 'nav-menu-item',
  'alert-warn', 'tool-warning', 'tool-hint', 'tool-actions', 'tool-actions-inline',
  'tool-status', 'tool-textarea', 'tool-result', 'tool-badges', 'tool-panel',
  'field', 'field-label', 'field-value', 'copy-btn',
  'seed-grid', 'seed-word', 'seed-index', 'seed-text',
];

const unstyled = NEW_CLASSES.filter(c => !cssClasses.has(c));
check('every new component class is styled', unstyled.length === 0,
  `unstyled: ${unstyled.join(', ')}`);

const unstyledHtml = uniq(htmlClasses).filter(c => !cssClasses.has(c));
check('no class in index.html is left unstyled', unstyledHtml.length === 0,
  `unstyled: ${unstyledHtml.join(', ')}`);

// ─── script loading ──────────────────────────────────────────────────────────

/* Matched against the real <script> tags, so a ?v= stamp on either does not
   quietly turn this check into a no-op. */
const cryptoTag = html.search(/<script src="vendor\/wrkz-crypto\.js(\?[^"]*)?"/);
const appTag = html.search(/<script src="app\.js(\?[^"]*)?"/);
check('both script tags are present', cryptoTag !== -1 && appTag !== -1,
  `crypto at ${cryptoTag}, app at ${appTag}`);
check('wrkz-crypto.js is loaded before app.js', cryptoTag !== -1 && cryptoTag < appTag);

check('the crypto module file exists',
  fs.existsSync(path.join(ROOT, 'vendor', 'wrkz-crypto.js')));

// ─── cache-busting stamps ────────────────────────────────────────────────────

/* index.html is not cached by the CDN but style.css and the scripts are, so a
   deploy that does not change their URLs can leave a new page running an old
   app.js. Every CDN-cached asset must therefore carry ?v=, and all of them must
   carry the SAME value — a half-bumped set is the exact failure this prevents. */
const CACHED_ASSETS = ['style.css', 'app.js', 'vendor/wrkz-crypto.js'];

const stamps = {};
for (const asset of CACHED_ASSETS) {
  const re = new RegExp(`(?:href|src)="${asset.replace(/[.\/]/g, '\\$&')}(\\?v=([^"]*))?"`);
  const m = html.match(re);
  check(`${asset} is referenced in index.html`, !!m);
  if (!m) continue;
  check(`${asset} carries a ?v= stamp`, !!m[1], 'no version query string');
  stamps[asset] = m[2];
}

const values = uniq(Object.values(stamps).filter(v => v !== undefined));
check('all cache-busting stamps share one value', values.length === 1,
  `found ${values.length} distinct values: ${values.join(', ')}`);
check('the stamp is non-empty', values[0] !== '' && values[0] !== undefined);

/* TurtleCoinUtils.js is injected by app.js rather than declared in the markup,
   so it must reuse the stamp instead of hardcoding a second one. */
check('app.js derives ASSET_VERSION from its own script tag',
  /const ASSET_VERSION = \(\(\) => \{[\s\S]*?script\[src\*="app\.js"\]/.test(app));
check('TurtleCoinUtils.js is loaded through versioned()',
  app.includes("versioned('vendor/TurtleCoinUtils.js')"));
check('no other hardcoded ?v= in app.js',
  !/['"`][^'"`]*\?v=\d/.test(app), 'a second version literal would drift');

// ─── dead code must stay dead ────────────────────────────────────────────────

check('no leftover _cnCrypto references', !app.includes('_cnCrypto'));
check('exactly one runCheckTx definition',
  (app.match(/^async function runCheckTx\(/gm) || []).length === 1,
  `found ${(app.match(/^async function runCheckTx\(/gm) || []).length}`);

// ─── the tools must not talk to the network ──────────────────────────────────

/* Everything from the tools banner to the init section should be free of
   fetch/apiGet/apiPost — the whole point is that keys never leave the page. */
const toolsStart = app.indexOf('WALLET / ADDRESS TOOLS');
const toolsEnd = app.indexOf('// ─── INIT ───');
check('tools section boundaries found', toolsStart > 0 && toolsEnd > toolsStart);

const toolsSrc = app.slice(toolsStart, toolsEnd);
for (const bad of ['fetch(', 'apiGet(', 'apiPost(', 'XMLHttpRequest', 'WebSocket', 'localStorage']) {
  check(`tools section never uses ${bad.replace('(', '')}`, !toolsSrc.includes(bad));
}

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail === 0 ? 0 : 1);
