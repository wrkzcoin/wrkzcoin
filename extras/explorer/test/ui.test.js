'use strict';

/* End-to-end exercise of the four wallet tools.

   There is no headless browser here, so this file stands up a minimal DOM shim
   — enough surface for app.js to boot and for the tool buttons to be clicked —
   and then drives the real handlers. It is not a browser and does not check
   layout; what it does check is the wiring that a syntax check and the crypto
   suite both miss: that clicking Generate actually renders a seed grid, that a
   seed copied out of the rendered page imports back to the same address, and
   that bad input reaches the error status rather than throwing.

   Run with:  node test/ui.test.js       (from extras/explorer) */

const fs = require('fs');
const vm = require('vm');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(ROOT, 'index.html'), 'utf8');

const ids = [...html.matchAll(/\bid="([A-Za-z0-9_-]+)"/g)].map(m => m[1]);

function makeEl(id, tag) {
  const el = {
    id, tagName: (tag || 'div').toUpperCase(),
    _listeners: {}, _classes: new Set(), dataset: {},
    innerHTML: '', textContent: '', value: '', hidden: false,
    style: {}, children: [], parentNode: null,
    classList: {
      add: (...c) => c.forEach(x => el._classes.add(x)),
      remove: (...c) => c.forEach(x => el._classes.delete(x)),
      toggle: (c, on) => (on ? el._classes.add(c) : el._classes.delete(c)),
      contains: c => el._classes.has(c),
    },
    addEventListener: (t, fn) => { (el._listeners[t] = el._listeners[t] || []).push(fn); },
    setAttribute: (k, v) => { el[k] = v; },
    getAttribute: k => el[k],
    removeAttribute: k => { delete el[k]; },
    contains: () => false,
    closest: sel => (sel === '.copy-btn' && el._classes.has('copy-btn') ? el : null),
    select: () => {},
    appendChild: c => { el.children.push(c); c.parentNode = el; return c; },
    removeChild: c => { el.children = el.children.filter(x => x !== c); return c; },
    click() { (el._listeners.click || []).forEach(fn => fn({ target: el, stopPropagation() {} })); },
  };
  /* Real elements keep className and classList in sync; the app sets both. */
  Object.defineProperty(el, 'className', {
    get: () => [...el._classes].join(' '),
    set: v => { el._classes = new Set(String(v).split(/\s+/).filter(Boolean)); },
  });
  return el;
}

const registry = new Map();
for (const id of ids) registry.set(id, makeEl(id));

const document = {
  documentElement: makeEl('html'),
  body: makeEl('body'),
  _listeners: {},
  getElementById: id => registry.get(id) || null,
  /* app.js reads its own <script> tag to recover the cache-busting stamp. */
  querySelector: sel => {
    if (sel === 'script[src*="app.js"]') {
      const tag = makeEl('', 'script');
      tag.src = 'https://explorer.example/app.js?v=TESTSTAMP';
      return tag;
    }
    return null;
  },
  querySelectorAll: () => [],
  createElement: tag => makeEl('', tag),
  addEventListener: (t, fn) => { (document._listeners[t] = document._listeners[t] || []).push(fn); },
  execCommand: () => true,
};

const store = {};
const sandbox = {
  console,
  document,
  localStorage: {
    getItem: k => (k in store ? store[k] : null),
    setItem: (k, v) => { store[k] = String(v); },
  },
  location: { hash: '#/paper' },
  navigator: {},
  isSecureContext: false,
  crypto: require('crypto').webcrypto,
  setTimeout, clearTimeout, setInterval, clearInterval,
  fetch: () => Promise.reject(new Error('network disabled in smoke test')),
  TextEncoder, TextDecoder, URL,
  print: () => { sandbox.__printed = true; },
};
sandbox.addEventListener = () => {};
sandbox.window = sandbox;
sandbox.globalThis = sandbox;
vm.createContext(sandbox);

vm.runInContext(fs.readFileSync(path.join(ROOT, 'vendor/wrkz-crypto.js'), 'utf8'), sandbox,
  { filename: 'wrkz-crypto.js' });
vm.runInContext(fs.readFileSync(path.join(ROOT, 'app.js'), 'utf8'), sandbox,
  { filename: 'app.js' });

let pass = 0, fail = 0;
const check = (name, ok, detail) => {
  if (ok) { pass++; console.log(`  ok   ${name}`); }
  else { fail++; console.log(`  FAIL ${name}${detail ? `\n         ${detail}` : ''}`); }
};

// Boot the app the way DOMContentLoaded would.
(document._listeners.DOMContentLoaded || []).forEach(fn => fn());
check('app.js boots without throwing', true);

const el = id => registry.get(id);
const LIVE = 'WrkzTF4cNAHbphuhWvyYjjE4qfVL3FCWTVpa11jSPFUqcuhuQcVa9e9ffSTutP6zSs7HUVTdDZd1aH8HCpxn3Sy52M2dfahwSi';

console.log('\n— paper wallet —');
el('paperGenBtn').click();
const paper = el('paperResult').innerHTML;
check('renders a result', paper.length > 0);
check('shows an address field', paper.includes('Address'));
check('shows a 25-word seed grid', (paper.match(/seed-word/g) || []).length === 25);
check('shows both private keys',
  paper.includes('Private Spend Key') && paper.includes('Private View Key'));
check('print button revealed', el('paperPrintBtn').hidden === false);
check('status is ok', el('paperStatus')._classes.has('ok'), el('paperStatus').textContent);
const addrMatch = paper.match(/Wrkz[1-9A-HJ-NP-Za-km-z]{94}/);
check('generated address is well formed', !!addrMatch, paper.slice(0, 200));
el('paperPrintBtn').click();
check('print button calls window.print', sandbox.__printed === true);

console.log('\n— import from seed —');
const seedMatch = paper.match(/data-copy="((?:[a-z]+ ){24}[a-z]+)"/);
check('seed is copyable from the rendered page', !!seedMatch);
el('importInput').value = seedMatch[1];
el('importBtn').click();
const imported = el('importResult').innerHTML;
check('import renders a result', imported.length > 0);
check('imported address matches the generated one',
  imported.includes(addrMatch[0]), 'seed did not round-trip through the UI');
check('import status ok', el('importStatus')._classes.has('ok'));

el('importInput').value = 'not a valid seed';
el('importBtn').click();
check('bad seed reports an error', el('importStatus')._classes.has('err'));
check('bad seed clears the result', el('importResult').innerHTML === '');

el('importClearBtn').click();
check('clear empties the input', el('importInput').value === '');

console.log('\n— integrated address —');
el('intRand16').click();
check('random 16-char payment ID', /^[0-9a-f]{16}$/.test(el('intPid').value), el('intPid').value);
el('intRand64').click();
check('random 64-char payment ID', /^[0-9a-f]{64}$/.test(el('intPid').value));

el('intAddr').value = LIVE;
el('intPid').value = 'a1b2c3d4e5f60718';
el('intBtn').click();
const intOut = el('intResult').innerHTML;
check('integrated result rendered', intOut.length > 0);
check('marked as a short payment ID', intOut.includes('Short payment ID'));
check('reports 120 characters', intOut.includes('120 characters'));
check('echoes the payment ID', intOut.includes('a1b2c3d4e5f60718'));
check('shows the original standard address', intOut.includes(LIVE));
check('integrated status ok', el('intStatus')._classes.has('ok'));

const intAddrMatch = intOut.match(/Wrkz[1-9A-HJ-NP-Za-km-z]{116}/);
check('produced a 120-char integrated address', !!intAddrMatch);

el('intPid').value = 'nothex!!';
el('intBtn').click();
check('bad payment ID reports an error', el('intStatus')._classes.has('err'),
  el('intStatus').textContent);

console.log('\n— decode address —');
el('decAddr').value = LIVE;
el('decBtn').click();
let decOut = el('decResult').innerHTML;
check('standard address decodes', decOut.includes('Standard address'));
check('shows the prefix', decOut.includes('999730'));
check('shows both public keys',
  decOut.includes('Public Spend Key') && decOut.includes('Public View Key'));
check('decode status ok', el('decStatus')._classes.has('ok'));

el('decAddr').value = intAddrMatch[0];
el('decBtn').click();
decOut = el('decResult').innerHTML;
check('short integrated address decodes',
  decOut.includes('Integrated address (short payment ID)'), decOut.slice(0, 300));
check('extracts the payment ID', decOut.includes('a1b2c3d4e5f60718'));
check('extracts the base address', decOut.includes(LIVE));

el('decAddr').value = LIVE.slice(0, 97) + 'X';
el('decBtn').click();
check('corrupt address reports an error', el('decStatus')._classes.has('err'),
  el('decStatus').textContent);

el('decClearBtn').click();
check('clear empties the decoder', el('decAddr').value === '' && el('decResult').innerHTML === '');

console.log('\n— cache-busting stamp —');
check('versioned() stamps a runtime-loaded asset',
  sandbox.versioned('vendor/TurtleCoinUtils.js') === 'vendor/TurtleCoinUtils.js?v=TESTSTAMP',
  sandbox.versioned && sandbox.versioned('vendor/TurtleCoinUtils.js'));

console.log('\n— tools menu —');
el('navToolsBtn').click();
check('menu opens', el('navToolsList').hidden === false);
check('aria-expanded set', el('navToolsBtn').getAttribute('aria-expanded') === 'true');
el('navToolsBtn').click();
check('menu closes', el('navToolsList').hidden === true);

console.log(`\n${pass} passed, ${fail} failed`);
process.exit(fail === 0 ? 0 : 1);
