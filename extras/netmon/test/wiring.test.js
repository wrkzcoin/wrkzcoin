/* Cross-references index.html, app.js and style.css.
 *
 * These are the mistakes a hand edit actually makes: a bumped cache stamp on
 * one file but not the other, a getElementById for an element that was
 * renamed, a route with no page, a CSS class nothing uses. Node only, no npm.
 *
 *   node test/wiring.test.js
 */

'use strict';

const fs = require('fs');
const path = require('path');

const DIR = path.join(__dirname, '..');

const html = fs.readFileSync(path.join(DIR, 'index.html'), 'utf8');
const app = fs.readFileSync(path.join(DIR, 'app.js'), 'utf8');
const css = fs.readFileSync(path.join(DIR, 'style.css'), 'utf8');

let checks = 0;
let failures = 0;

function ok(condition, message) {
  checks++;
  if (!condition) {
    failures++;
    console.log(`  FAIL  ${message}`);
  }
}

function section(name) {
  console.log(`\n${name}`);
}

/* ─── cache-busting stamps ────────────────────────────────────────────────── */

section('Cache-busting stamps');

const stamps = [...html.matchAll(/\?v=(\d{8})/g)].map((m) => m[1]);

ok(stamps.length === 2, `expected 2 ?v= stamps in index.html, found ${stamps.length}`);
ok(new Set(stamps).size === 1, `the ?v= stamps disagree: ${stamps.join(', ')}`);
ok(/href="style\.css\?v=\d{8}"/.test(html), 'style.css has no ?v= stamp');
ok(/src="app\.js\?v=\d{8}"/.test(html), 'app.js has no ?v= stamp');

/* ─── element ids ─────────────────────────────────────────────────────────── */

section('Element ids reached by app.js');

const wanted = new Set([...app.matchAll(/\bel\('([A-Za-z0-9_]+)'\)/g)].map((m) => m[1]));
const declared = new Set([...html.matchAll(/\bid="([A-Za-z0-9_]+)"/g)].map((m) => m[1]));

/* Created by app.js itself rather than present in the markup. */
const dynamic = new Set(['pagePrev', 'pageNext', 'clearSearch']);

for (const id of wanted) {
  if (dynamic.has(id)) continue;
  ok(declared.has(id), `app.js reads #${id}, which index.html does not declare`);
}

ok(wanted.size > 20, `only ${wanted.size} ids reached from app.js — did the selector helper change?`);

/* ─── pages and routes ───────────────────────────────────────────────────── */

section('Pages and routes');

const pageList = app.match(/const PAGES = \[([^\]]+)\]/);
ok(pageList !== null, 'app.js has no PAGES list');

if (pageList) {
  const pages = [...pageList[1].matchAll(/'([A-Za-z0-9_]+)'/g)].map((m) => m[1]);

  ok(pages.length >= 5, `only ${pages.length} pages listed`);

  for (const page of pages) {
    ok(declared.has(page), `PAGES names #${page}, which index.html does not declare`);
    ok(new RegExp(`id="${page}"[^>]*class="page`).test(html), `#${page} is missing class="page"`);
  }

  const sections = [...html.matchAll(/<section id="([A-Za-z0-9_]+)" class="page/g)].map((m) => m[1]);

  for (const s of sections) {
    ok(pages.includes(s), `index.html declares page #${s}, which PAGES does not list`);
  }

  const active = [...html.matchAll(/class="page active"/g)];
  ok(active.length === 1, `exactly one page should start active, found ${active.length}`);
}

/* Every nav link's data-nav must be a value setNav is called with. */
const navValues = new Set([...html.matchAll(/data-nav="([a-z]+)"/g)].map((m) => m[1]));
const navCalls = new Set([...app.matchAll(/setNav\('([a-z]+)'\)/g)].map((m) => m[1]));

for (const value of navValues) {
  ok(navCalls.has(value), `index.html has data-nav="${value}" but app.js never calls setNav('${value}')`);
}

/* ─── CSS classes ────────────────────────────────────────────────────────── */

section('CSS classes');

const cssClasses = new Set([...css.matchAll(/\.([a-z][a-z0-9-]*)\s*[{,:]/g)].map((m) => m[1]));
const usedInHtml = new Set();

/* app.js builds class attributes inside template literals, so a raw split on
   whitespace picks up fragments of the interpolation. Strip every ${...} span
   first, then keep only what actually looks like a class name. */
function collectClasses(source) {
  for (const m of source.matchAll(/class="([^"]*)"/g)) {
    m[1]
      .replace(/\$\{[^}]*\}/g, ' ')
      .split(/\s+/)
      .filter((c) => /^[a-z][a-z0-9-]*$/.test(c))
      .forEach((c) => usedInHtml.add(c));
  }
}

collectClasses(html);
collectClasses(app);

/* Toggled from script rather than written into markup. */
const scripted = new Set(['hidden', 'active', 'off-tip', 'full', 'part', 'down', 'none', 'good', 'warn', 'bad', 'mute']);

for (const cls of usedInHtml) {
  if (scripted.has(cls)) continue;
  ok(cssClasses.has(cls), `class "${cls}" is used but style.css defines no rule for it`);
}

/* ─── API surface ────────────────────────────────────────────────────────── */

section('API surface');

/* Calls are a mix of get('/summary') and get(`/peers?...`). */
const apiCalls = new Set([...app.matchAll(/get\([`'](\/[a-z]+)/g)].map((m) => m[1]));
const expected = ['/summary', '/peers', '/geo', '/versions'];

for (const endpoint of expected) {
  ok(apiCalls.has(endpoint), `app.js never calls ${endpoint}`);
}

ok(/const API = '\/api'/.test(app), "API base should be '/api' — see the note in HttpApi.h about mount-point shadowing");

/* Everything must go through the one helper, so the base path is honoured. */
const rawFetches = [...app.matchAll(/fetch\(/g)].length;
ok(rawFetches === 1, `app.js should have exactly one fetch() call, inside get(); found ${rawFetches}`);

/* ─── storage safety ─────────────────────────────────────────────────────── */

section('Storage safety');

/* localStorage throws outright in some contexts, so every access needs a
   try/catch or the whole page dies on load. */
const storageHits = [...app.matchAll(/localStorage\.[gs]etItem/g)].length;
const guardedHits = [...app.matchAll(/try \{[^}]*localStorage\.[gs]etItem[^}]*\}\s*catch/g)].length;

ok(storageHits > 0, 'expected the theme to be remembered in localStorage');
ok(storageHits === guardedHits, `${storageHits} localStorage accesses but only ${guardedHits} are in a try/catch`);

/* ─── report ─────────────────────────────────────────────────────────────── */

console.log(`\n${checks - failures} / ${checks} assertions passed`);

if (failures > 0) {
  console.log(`${failures} FAILED`);
  process.exit(1);
}
