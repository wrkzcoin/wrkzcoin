/* Guards the one thing that will silently drift.
 *
 * extras/netmon/style.css carries a copy of the block explorer's theme tokens
 * so the two pages read as one product. Nothing at runtime links them, so the
 * moment somebody tunes a colour in one file they diverge. This diffs the
 * :root blocks of both and fails on any difference.
 *
 *   node test/tokens.test.js
 */

'use strict';

const fs = require('fs');
const path = require('path');

const MINE = path.join(__dirname, '..', 'style.css');
const THEIRS = path.join(__dirname, '..', '..', 'explorer', 'style.css');

let checks = 0;
let failures = 0;

function ok(condition, message) {
  checks++;
  if (!condition) {
    failures++;
    console.log(`  FAIL  ${message}`);
  }
}

/* Pulls `:root[data-theme="<name>"] { ... }` and returns it as an ordered list
   of "--token: value" strings, whitespace normalised. */
function tokenBlock(css, theme) {
  const start = css.indexOf(`:root[data-theme="${theme}"]`);

  if (start === -1) {
    return null;
  }

  const open = css.indexOf('{', start);
  const close = css.indexOf('}', open);

  if (open === -1 || close === -1) {
    return null;
  }

  return css
    .slice(open + 1, close)
    .split(/[;\n]/)
    .map((line) => line.replace(/\/\*[^]*?\*\//g, '').trim())
    .filter((line) => line.startsWith('--') || line.startsWith('color-scheme'))
    .map((line) => line.replace(/\s+/g, ' '));
}

if (!fs.existsSync(THEIRS)) {
  console.log(`\nSKIP  ${THEIRS} is not present; nothing to compare against.`);
  process.exit(0);
}

const mine = fs.readFileSync(MINE, 'utf8');
const theirs = fs.readFileSync(THEIRS, 'utf8');

for (const theme of ['dark', 'light']) {
  console.log(`\n:root[data-theme="${theme}"]`);

  const a = tokenBlock(mine, theme);
  const b = tokenBlock(theirs, theme);

  ok(a !== null, `extras/netmon/style.css has no ${theme} token block`);
  ok(b !== null, `extras/explorer/style.css has no ${theme} token block`);

  if (!a || !b) {
    continue;
  }

  ok(a.length === b.length,
    `${theme}: netmon declares ${a.length} tokens, explorer declares ${b.length}`);

  const mineMap = new Map(a.map((line) => {
    const i = line.indexOf(':');
    return [line.slice(0, i).trim(), line.slice(i + 1).trim()];
  }));

  const theirsMap = new Map(b.map((line) => {
    const i = line.indexOf(':');
    return [line.slice(0, i).trim(), line.slice(i + 1).trim()];
  }));

  for (const [token, value] of theirsMap) {
    if (!mineMap.has(token)) {
      ok(false, `${theme}: explorer defines ${token} and netmon does not`);
      continue;
    }

    ok(mineMap.get(token) === value,
      `${theme}: ${token} is "${mineMap.get(token)}" here but "${value}" in the explorer`);
  }

  for (const token of mineMap.keys()) {
    ok(theirsMap.has(token), `${theme}: netmon defines ${token}, which the explorer does not`);
  }
}

console.log(`\n${checks - failures} / ${checks} assertions passed`);

if (failures > 0) {
  console.log(`${failures} FAILED — reconcile the two :root blocks, do not "fix" one side only.`);
  process.exit(1);
}
