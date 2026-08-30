'use strict';

/* ═══════════════════════════════════════════════════════════════════════════
   WrkzCoin Block Explorer — app.js
   Pure vanilla JS, no dependencies. Talks directly to the daemon JSON-RPC.

   Required daemon flags:
     --daemon-mode explorer
     --rpc-access-control-origins '*'
   ═══════════════════════════════════════════════════════════════════════════ */

// ─── COIN CONFIG ─────────────────────────────────────────────────────────────

const COIN = {
  name:        'WrkzCoin',
  ticker:      'WRKZ',
  decimals:    2,           // CRYPTONOTE_DISPLAY_DECIMAL_POINT = 2  →  atomic / 100
  blockTime:   60,          // DIFFICULTY_TARGET = 60 seconds
  defaultPort: 17856,
  addressPrefix: 999730,
};

// ─── STATE ────────────────────────────────────────────────────────────────────

// Default: use the /api path proxied by nginx to localhost:17856.
// Override in the settings bar if connecting directly to the daemon.
let daemonUrl    = localStorage.getItem('daemonUrl') || '/api';
let theme        = localStorage.getItem('theme') || 'dark';
let refreshTimer = null;
let countdownTimer = null;
let countdownSec   = 30;
let turtleCoinUtilsPromise = null;

// Block list pagination state
let blockPageTop    = null;  // top block height currently shown (null = unset, use latest)
let blockChainHeight = 0;    // latest chain height from /info
let blockPageSize   = 30;    // dynamically updated from actual response length

// ─── THEME ────────────────────────────────────────────────────────────────────

function applyTheme(t) {
  theme = t;
  document.documentElement.setAttribute('data-theme', t);
  localStorage.setItem('theme', t);
  $('iconMoon').classList.toggle('hidden', t === 'light');
  $('iconSun').classList.toggle('hidden',  t === 'dark');
}

function toggleTheme() {
  applyTheme(theme === 'dark' ? 'light' : 'dark');
}

// ─── DOM HELPERS ──────────────────────────────────────────────────────────────

function $(id)           { return document.getElementById(id); }
function setText(id, v)  { const e = $(id); if (e) e.textContent = v; }
function setHtml(id, v)  { const e = $(id); if (e) e.innerHTML  = v; }

function escHtml(s) {
  if (s == null) return '';
  return String(s)
    .replace(/&/g,  '&amp;')
    .replace(/</g,  '&lt;')
    .replace(/>/g,  '&gt;')
    .replace(/"/g,  '&quot;')
    .replace(/'/g,  '&#39;');
}

// ─── API ──────────────────────────────────────────────────────────────────────

async function apiGet(path) {
  const url = daemonUrl.replace(/\/$/, '') + path;
  const res  = await fetch(url, { mode: 'cors' });
  if (!res.ok) throw new Error(`HTTP ${res.status} on GET ${path}`);
  return res.json();
}

async function apiPost(method, params) {
  const url = daemonUrl.replace(/\/$/, '') + '/json_rpc';
  const res  = await fetch(url, {
    method:  'POST',
    mode:    'cors',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify({ jsonrpc: '2.0', id: '0', method, params: params || {} }),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status} on ${method}`);
  const data = await res.json();
  if (data.error) throw new Error(data.error.message || JSON.stringify(data.error));
  return data.result;
}

// Convenience wrappers
const api = {
  info:              ()      => apiGet('/info'),
  height:            ()      => apiGet('/height'),
  peers:             ()      => apiGet('/peers'),
  getBlockCount:     ()      => apiPost('getblockcount'),
  getLastBlockHeader:()      => apiPost('getlastblockheader'),
  getBlockHeaderByHeight: h  => apiPost('getblockheaderbyheight', { height: Number(h) }),
  getBlockHeaderByHash:   h  => apiPost('getblockheaderbyhash',   { hash: h }),
  // Explorer-mode (requires --daemon-mode explorer)
  getBlocksByHeight:  h      => apiPost('f_blocks_list_json',          { height: Number(h) }),
  getBlockByHash:     h      => apiPost('f_block_json',                { hash: h }),
  getTxByHash:        h      => apiPost('f_transaction_json',          { hash: h }),
  getMempool:         ()     => apiPost('f_on_transactions_pool_json'),
  // Long (plaintext) payment IDs only. Short ones are encrypted to their
  // receiver, so the daemon has nothing stable to index and rejects them.
  getTxsByPaymentId:  p      => apiPost('f_transactions_by_payment_id_json', { paymentId: p }),
};

// ─── FORMATTING ───────────────────────────────────────────────────────────────

function formatAmount(atomic) {
  if (atomic == null) return '—';
  // Handle string values (e.g. alreadyGeneratedCoins)
  const n   = typeof atomic === 'string' ? parseInt(atomic, 10) : Number(atomic);
  if (isNaN(n)) return '—';
  const factor = Math.pow(10, COIN.decimals);
  const whole  = Math.floor(n / factor);
  const frac   = String(n % factor).padStart(COIN.decimals, '0');
  return `${whole.toLocaleString()}.${frac}`;
}

function formatHashrate(hps) {
  if (hps == null) return '—';
  const h = Number(hps);
  if (h >= 1e12) return `${(h / 1e12).toFixed(2)} TH/s`;
  if (h >= 1e9)  return `${(h / 1e9).toFixed(2)} GH/s`;
  if (h >= 1e6)  return `${(h / 1e6).toFixed(2)} MH/s`;
  if (h >= 1e3)  return `${(h / 1e3).toFixed(2)} KH/s`;
  return `${h} H/s`;
}

function formatSize(bytes) {
  if (bytes == null) return '—';
  const b = Number(bytes);
  if (b >= 1048576) return `${(b / 1048576).toFixed(2)} MB`;
  if (b >= 1024)    return `${(b / 1024).toFixed(2)} KB`;
  return `${b} B`;
}

function formatCompactNumber(value) {
  if (value == null || Number.isNaN(Number(value))) return '—';
  return new Intl.NumberFormat(undefined, {
    notation: 'compact',
    maximumFractionDigits: 1,
  }).format(Number(value));
}

function formatChartMetric(label, value) {
  switch (label) {
    case 'Difficulty':
      return Number(value || 0).toLocaleString();
    case 'Block Size':
      return formatSize(value);
    case 'Tx Count':
      return Number(value || 0).toLocaleString();
    default:
      return String(value ?? '—');
  }
}

function buildCombinedTrendChartSvg(series) {
  const width = 360;
  const height = 104;
  const padX = 12;
  const padTop = 10;
  const padBottom = 12;
  const innerWidth = width - (padX * 2);
  const innerHeight = height - padTop - padBottom;
  const gridLines = [0.25, 0.5, 0.75].map(step => {
    const y = padTop + (innerHeight * step);
    return `<line x1="${padX}" y1="${y}" x2="${width - padX}" y2="${y}"></line>`;
  }).join('');
  const lines = series.map(item => {
    const normalizedValues = item.values.length ? item.values : [0];
    const min = Math.min(...normalizedValues);
    const max = Math.max(...normalizedValues);
    const span = Math.max(max - min, 1);
    const points = normalizedValues.map((value, index) => {
      const x = padX + ((innerWidth / Math.max(normalizedValues.length - 1, 1)) * index);
      const ratio = normalizedValues.length === 1 ? 0.5 : (value - min) / span;
      const y = padTop + innerHeight - (ratio * innerHeight);
      return [Number(x.toFixed(2)), Number(y.toFixed(2))];
    });

    const linePath = points.map(([x, y], index) => `${index === 0 ? 'M' : 'L'}${x} ${y}`).join(' ');
    const dots = points.map(([x, y], index) => {
      const latest = index === points.length - 1;
      return `<circle class="network-chart-dot" cx="${x}" cy="${y}" r="${latest ? 3.5 : 2.5}" fill="${item.color}" stroke="rgba(15,23,42,0.8)"></circle>`;
    }).join('');

    return `<path class="network-chart-line" d="${linePath}" stroke="${item.color}"></path>${dots}`;
  }).join('');

  return `
    <svg viewBox="0 0 ${width} ${height}" preserveAspectRatio="none" aria-hidden="true">
      <g class="network-chart-grid">${gridLines}</g>
      <line class="network-chart-axis" x1="${padX}" y1="${height - padBottom}" x2="${width - padX}" y2="${height - padBottom}"></line>
      ${lines}
    </svg>
  `;
}

function timeAgo(ts) {
  const diff = Math.floor(Date.now() / 1000) - Number(ts);
  if (diff < 0)    return 'just now';
  if (diff < 60)   return `${diff}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ${diff % 60}s ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ${Math.floor((diff % 3600) / 60)}m ago`;
  return `${Math.floor(diff / 86400)}d ago`;
}

function formatTs(ts) {
  return new Date(Number(ts) * 1000).toLocaleString(undefined, {
    year: 'numeric', month: 'short', day: 'numeric',
    hour: '2-digit', minute: '2-digit', second: '2-digit',
  });
}

function shortHash(h, head = 12, tail = 8) {
  if (!h || h.length <= head + tail + 3) return h || '—';
  return `${h.slice(0, head)}…${h.slice(-tail)}`;
}

function isZeroHash(h) {
  return !h || /^0+$/.test(h);
}

// ─── ERROR HANDLING ───────────────────────────────────────────────────────────

function classifyError(err, context) {
  const msg = err?.message || String(err);

  if (/Failed to fetch|NetworkError|ERR_CONNECTION|ECONNREFUSED|Load failed/i.test(msg)) {
    const isProxy = daemonUrl.startsWith('/');
    const hint = isProxy
      ? `Check that nginx is running and <code>${escHtml(daemonUrl)}</code> is proxied to the daemon on port ${COIN.defaultPort}.`
      : `Is the daemon running at <code>${escHtml(daemonUrl)}</code>? You may also need <code>--rpc-access-control-origins '*'</code>.`;
    return `Cannot reach daemon. ${hint}`;
  }
  if (/403|Forbidden|RpcMode/i.test(msg)) {
    return `This feature requires explorer mode. Start the daemon with `
         + `<code>--daemon-mode explorer</code>.`;
  }
  if (/404/i.test(msg)) {
    return context
      ? `${context} not found.`
      : 'Not found.';
  }
  return context
    ? `Failed to load ${context}: ${escHtml(msg)}`
    : escHtml(msg);
}

function showError(html) {
  $('errorMsg').innerHTML = html;
  $('errorBar').style.display = 'flex';
  hideLoading();
}

function clearError() {
  $('errorBar').style.display = 'none';
}

function showLoading() {
  $('loading').style.display = 'flex';
}

function hideLoading() {
  $('loading').style.display = 'none';
}

function setConnStatus() { /* UI element removed */ }

/* The cache-busting stamp index.html put on our own <script> tag. Reused for
   assets loaded at runtime so there is only ever one value to bump. */
const ASSET_VERSION = (() => {
  const tag = document.querySelector('script[src*="app.js"]');
  const match = tag && tag.src.match(/[?&]v=([^&]*)/);
  return match ? match[1] : '';
})();

function versioned(path) {
  return ASSET_VERSION ? `${path}?v=${encodeURIComponent(ASSET_VERSION)}` : path;
}

function ensureTurtleCoinUtils() {
  if (window.TurtleCoinUtils) return Promise.resolve(window.TurtleCoinUtils);
  if (turtleCoinUtilsPromise) return turtleCoinUtilsPromise;

  turtleCoinUtilsPromise = new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = versioned('vendor/TurtleCoinUtils.js');
    script.async = true;
    script.onload = () => {
      if (window.TurtleCoinUtils) {
        resolve(window.TurtleCoinUtils);
      } else {
        reject(new Error('Crypto utility library loaded but is unavailable.'));
      }
    };
    script.onerror = () => reject(new Error('Failed to load crypto utility library.'));
    document.body.appendChild(script);
  }).catch(err => {
    turtleCoinUtilsPromise = null;
    throw err;
  });

  return turtleCoinUtilsPromise;
}

// ─── ROUTING ──────────────────────────────────────────────────────────────────

async function route() {
  stopRefresh();
  clearError();

  const raw   = window.location.hash.slice(1) || '/';  // e.g. "/block/abc123"
  const parts = raw.split('/').filter(Boolean);         // ["block","abc123"]
  const page  = parts[0] || 'home';

  // Highlight nav link
  closeToolsMenu();
  document.querySelectorAll('.nav-link, .nav-menu-item').forEach(el => el.classList.remove('active'));
  if (page === 'home'    || page === '')      { $('navHome')?.classList.add('active'); }
  if (page === 'mempool')                     { $('navMempool')?.classList.add('active'); }
  if (TOOL_PAGES.includes(page)) {
    $('navToolsBtn')?.classList.add('active');
    document.querySelector(`.nav-menu-item[href="#/${page}"]`)?.classList.add('active');
  }

  switch (page) {
    case 'home':       return showHome();
    case 'block':      return showBlock(parts[1] || '');
    case 'tx':         return showTx(parts[1] || '');
    case 'paymentid':  return showPaymentId(parts[1] || '');
    case 'mempool':    return showMempool();
    case 'paper':      return showPaperWallet();
    case 'import':     return showImportSeed();
    case 'integrated': return showIntegratedAddress();
    case 'decode':     return showDecodeAddress(parts[1] || '');
    default:
      showError(`Unknown route: <code>${escHtml(page)}</code>`);
  }
}

// ─── TOOLS MENU ───────────────────────────────────────────────────────────────

const TOOL_PAGES = ['paper', 'import', 'integrated', 'decode'];

function closeToolsMenu() {
  const menu = $('navTools');
  if (!menu) return;
  menu.dataset.open = 'false';
  $('navToolsList').hidden = true;
  $('navToolsBtn').setAttribute('aria-expanded', 'false');
}

function toggleToolsMenu() {
  const menu = $('navTools');
  if (!menu) return;
  const open = menu.dataset.open !== 'true';
  menu.dataset.open = String(open);
  $('navToolsList').hidden = !open;
  $('navToolsBtn').setAttribute('aria-expanded', String(open));
}

function initToolsMenu() {
  const btn = $('navToolsBtn');
  if (!btn) return;

  btn.addEventListener('click', e => { e.stopPropagation(); toggleToolsMenu(); });

  document.addEventListener('click', e => {
    if (!$('navTools')?.contains(e.target)) closeToolsMenu();
  });

  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') closeToolsMenu();
  });
}

// ─── AUTO REFRESH ─────────────────────────────────────────────────────────────

function startRefresh(fn, seconds) {
  stopRefresh();
  countdownSec = seconds;
  updateCountdown();
  countdownTimer = setInterval(() => {
    countdownSec--;
    updateCountdown();
    if (countdownSec <= 0) {
      countdownSec = seconds;
      fn();
    }
  }, 1000);
}

function stopRefresh() {
  clearInterval(refreshTimer);
  clearInterval(countdownTimer);
  refreshTimer = countdownTimer = null;
  const el = $('refreshCountdown');
  if (el) el.textContent = '';
}

function updateCountdown() {
  const el = $('refreshCountdown');
  if (el) el.textContent = `Refresh in ${countdownSec}s`;
}

// ─── SEARCH ───────────────────────────────────────────────────────────────────

async function doSearch(query) {
  query = (query || '').trim();
  if (!query) return;

  showLoading();
  clearError();

  // Pure integer → block height
  if (/^\d+$/.test(query)) {
    try {
      const r = await api.getBlockHeaderByHeight(parseInt(query, 10));
      const hash = r?.block_header?.hash;
      if (hash) { window.location.hash = `#/block/${hash}`; return; }
    } catch (e) { /* fall through */ }
  }

  // Looks like an address → hand it to the decoder, which validates the checksum
  if (window.WrkzCrypto
      && (query.length === window.WrkzCrypto.STANDARD_ADDRESS_LENGTH
       || query.length === window.WrkzCrypto.INTEGRATED_ADDRESS_LENGTH
       || query.length === window.WrkzCrypto.INTEGRATED_ADDRESS_LENGTH_LONG)) {
    try {
      window.WrkzCrypto.decodeAddress(query);
      hideLoading();
      window.location.hash = `#/decode/${encodeURIComponent(query)}`;
      return;
    } catch (e) { /* not a valid address, keep looking */ }
  }

  // 16-char hex → an encrypted (short) payment ID. Nothing can look one up,
  // so route to the page that explains why rather than reporting not-found.
  if (/^[0-9a-fA-F]{16}$/.test(query)) {
    hideLoading();
    window.location.hash = `#/paymentid/${query.toLowerCase()}`;
    return;
  }

  // 64-char hex is ambiguous: a TX hash, a block hash and a long payment ID
  // all look identical. Try the two that identify exactly one thing first,
  // and fall back to the payment ID, which names a set rather than a single
  // transaction.
  if (/^[0-9a-fA-F]{64}$/.test(query)) {
    // Try transaction
    try {
      await api.getTxByHash(query);
      window.location.hash = `#/tx/${query}`;
      return;
    } catch (e) { /* try block */ }

    // Try block by hash
    try {
      await api.getBlockByHash(query);
      window.location.hash = `#/block/${query}`;
      return;
    } catch (e) { /* try payment id */ }

    // Try payment ID last
    try {
      const result = await api.getTxsByPaymentId(query);
      if (result?.transactionHashes?.length > 0) {
        hideLoading();
        window.location.hash = `#/paymentid/${query.toLowerCase()}`;
        return;
      }
    } catch (e) { /* not found */ }
  }

  hideLoading();
  showError(`No results found for: <code>${escHtml(query)}</code>. `
          + `Tip: enter a block height (number), a block or transaction hash or a payment ID `
          + `(64 hex chars), or a Wrkz address to decode.`);
}

// ─── HOME ─────────────────────────────────────────────────────────────────────

function setPage(name) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  const p = $(`page-${name}`);
  if (p) p.classList.add('active');
  /* The tool pages are static markup, so nothing else will clear an overlay
     left behind by a search that routed here. */
  if (TOOL_PAGES.includes(name)) hideLoading();
}

async function showHome() {
  blockPageTop = null; // reset to latest page whenever navigating home
  setPage('home');
  clearError();
  await loadHome();
  const refreshBtn = $('refreshBtn');
  if (refreshBtn) refreshBtn.onclick = () => {
    countdownSec = 30;
    blockPageTop = null; // jump back to latest on manual refresh
    loadHome();
  };
  startRefresh(() => {
    blockPageTop = null; // keep auto-refresh pinned to the latest recent blocks
    loadHome();
  }, 30);
}

async function loadHome() {
  try {
    const info = await api.info();
    renderStats(info);
    updateLatestBlockStats(info.height).catch(() => {
      setText('statCirculating', '—');
      setText('statBlockReward', '—');
    });
    blockChainHeight = info.height;
    setConnStatus(true, '● Connected');
    clearError();

    // If no page is set yet, start at the latest block
    if (blockPageTop === null) {
      blockPageTop = info.height - 1;
    }
    await loadBlockPage(blockPageTop);
  } catch (err) {
    setConnStatus(false, '● Disconnected');
    showError(classifyError(err));
  }
}

async function updateLatestBlockStats(chainHeight) {
  const latestHeight = Math.max(0, Number(chainHeight || 0) - 1);
  const header = await api.getBlockHeaderByHeight(latestHeight);
  const hash = header?.block_header?.hash;
  if (!hash) {
    setText('statCirculating', '—');
    setText('statBlockReward', '—');
    return;
  }

  const result = await api.getBlockByHash(hash);
  const circulating = result?.block?.alreadyGeneratedCoins;
  const reward = result?.block?.reward;
  setText(
    'statCirculating',
    circulating == null ? '—' : `${formatAmount(circulating)} ${COIN.ticker}`
  );
  setText(
    'statBlockReward',
    reward == null ? '—' : `${formatAmount(reward)} ${COIN.ticker}`
  );
}

async function loadBlockPage(topHeight) {
  try {
    const blocksResult = await api.getBlocksByHeight(topHeight);
    const blocks = blocksResult?.blocks || [];
    if (blocks.length > 0) blockPageSize = blocks.length;
    blockPageTop = topHeight;
    renderBlockList(blocks);
    renderNetworkChart(blocks);
    renderBlockPagination();
  } catch (err) {
    showError(classifyError(err));
    renderNetworkChart([]);
    renderBlockPagination();
  }
}

function renderStats(info) {
  const totalConn = (info.incoming_connections_count || 0) + (info.outgoing_connections_count || 0);

  setText('statHeight',     (info.height ?? 0).toLocaleString());
  setText('statDifficulty', (info.difficulty ?? 0).toLocaleString());
  setText('statHashrate',   formatHashrate(info.hashrate));
  setText('statTxCount',    (info.tx_count ?? 0).toLocaleString());
  setText('statConnections',totalConn.toString());
  setText('statPool',       (info.tx_pool_size ?? 0).toString());
  setText('statAltBlocks',  (info.alt_blocks_count ?? 0).toString());

  const syncEl = $('statSynced');
  if (syncEl) {
    if (info.synced) {
      syncEl.textContent = '✓ Synced';
      syncEl.className   = 'badge badge-green';
    } else {
      const pct = info.network_height
        ? Math.floor((info.height / info.network_height) * 100)
        : 0;
      syncEl.textContent = `Syncing ${pct}%`;
      syncEl.className   = 'badge badge-yellow';
    }
  }
}

function renderBlockList(blocks) {
  const tbody = $('blockTableBody');
  if (!tbody) return;

  if (!blocks.length) {
    tbody.innerHTML = '<tr><td colspan="6" class="empty-cell">No blocks returned — check daemon connection.</td></tr>';
    return;
  }

  tbody.innerHTML = blocks.map(b => /* html */ `
    <tr>
      <td><a href="#/block/${escHtml(b.hash)}">${Number(b.height).toLocaleString()}</a></td>
      <td class="mono hash-cell">
        <a href="#/block/${escHtml(b.hash)}">${escHtml(b.hash)}</a>
      </td>
      <td>${timeAgo(b.timestamp)}</td>
      <td>${b.tx_count ?? 0}</td>
      <td>${Number(b.difficulty).toLocaleString()}</td>
      <td>${formatSize(b.cumul_size)}</td>
    </tr>
  `).join('');
}

function renderNetworkChart(blocks) {
  const heightEl = $('networkChartHeight');
  const metricsEl = $('networkMetrics');
  const listEl = $('networkChartList');
  if (!heightEl || !metricsEl || !listEl) return;

  if (!blocks.length) {
    heightEl.textContent = '—';
    metricsEl.innerHTML = `
      <span class="badge badge-accent">Difficulty —</span>
      <span class="badge badge-yellow">Block Size —</span>
      <span class="badge badge-green">Tx Count —</span>
      <span class="badge badge-red">Block Reward —</span>
    `;
    listEl.innerHTML = `
      <div class="network-chart-card">
        <div class="network-chart-meta">
          <span class="network-chart-label">Combined Network View</span>
          <span class="network-chart-value">—</span>
        </div>
        <div class="network-chart-plot"></div>
        <div class="network-chart-legend"></div>
      </div>
    `;
    return;
  }

  const latest = blocks[0];
  const latestReward = latest.reward == null ? '—' : `${formatAmount(latest.reward)} ${COIN.ticker}`;
  heightEl.textContent = `#${Number(latest.height).toLocaleString()}`;
  metricsEl.innerHTML = `
    <span class="badge badge-accent">Difficulty ${formatCompactNumber(latest.difficulty)}</span>
    <span class="badge badge-yellow">Block Size ${formatSize(latest.cumul_size)}</span>
    <span class="badge badge-green">Tx Count ${(latest.tx_count ?? 0).toLocaleString()}</span>
    <span class="badge badge-red">Block Reward ${latestReward}</span>
  `;

  const chartBlocks = [...blocks.slice(0, 12)].reverse();
  const series = [
    {
      label: 'Difficulty',
      value: Number(latest.difficulty || 0),
      color: '#7c83eb',
      values: chartBlocks.map(b => Number(b.difficulty || 0)),
    },
    {
      label: 'Block Size',
      value: Number(latest.cumul_size || 0),
      color: '#f2ba2e',
      values: chartBlocks.map(b => Number(b.cumul_size || 0)),
    },
    {
      label: 'Tx Count',
      value: Number(latest.tx_count || 0),
      color: '#3ecf9a',
      values: chartBlocks.map(b => Number(b.tx_count || 0)),
    },
  ];

  const legendHtml = series.map(item => `
    <span class="network-legend-item">
      <span class="network-legend-swatch" style="background:${item.color}"></span>
      <span>${item.label}</span>
      <strong>${formatChartMetric(item.label, item.value)}</strong>
    </span>
  `).join('');

  listEl.innerHTML = `
    <div class="network-chart-card">
      <div class="network-chart-meta">
        <span class="network-chart-label">Combined Network View</span>
        <span class="network-chart-value">Last ${chartBlocks.length} blocks</span>
      </div>
      <div class="network-chart-plot">
        ${buildCombinedTrendChartSvg(series)}
      </div>
      <div class="network-chart-legend">${legendHtml}</div>
    </div>
  `;
}

function renderBlockPagination() {
  const el = $('blockPagination');
  if (!el) return;

  if (blockChainHeight === 0 || blockPageTop === null) {
    el.style.display = 'none';
    return;
  }

  const atNewest = blockPageTop >= blockChainHeight - 1;
  // Daemon crashes when height < blockPageSize (tries to access block at index -1).
  // Safest "last" page: height = blockPageSize (returns blocks blockPageSize…1, avoids underflow).
  const lastTop  = blockPageSize;
  const atOldest = blockPageTop <= lastTop;

  const newerTop = Math.min(blockChainHeight - 1, blockPageTop + blockPageSize);
  const olderTop = Math.max(lastTop, blockPageTop - blockPageSize);
  const rangeFrom = Math.max(1, blockPageTop - blockPageSize + 1).toLocaleString();
  const rangeTo   = blockPageTop.toLocaleString();

  el.style.display = 'flex';
  el.innerHTML = `
    <button class="btn btn-sm btn-outline" data-goto="${blockChainHeight - 1}" ${atNewest ? 'disabled' : ''}>⇤ First</button>
    <button class="btn btn-sm btn-outline" data-goto="${newerTop}" ${atNewest ? 'disabled' : ''}>‹ Newer</button>
    <span class="pagination-info">${rangeFrom} – ${rangeTo}</span>
    <button class="btn btn-sm btn-outline" data-goto="${olderTop}" ${atOldest ? 'disabled' : ''}>Older ›</button>
    <button class="btn btn-sm btn-outline" data-goto="${lastTop}" ${atOldest ? 'disabled' : ''}>Last ⇥</button>
  `;

  el.querySelectorAll('button[data-goto]').forEach(btn => {
    btn.addEventListener('click', () => {
      loadBlockPage(parseInt(btn.dataset.goto, 10));
    });
  });
}

// ─── BLOCK DETAIL ─────────────────────────────────────────────────────────────

async function showBlock(hashOrHeight) {
  setPage('block');
  clearError();
  showLoading();
  setHtml('blockDetail', '');

  try {
    let hash = hashOrHeight;

    // Numeric height → resolve to hash first
    if (/^\d+$/.test(hash)) {
      const r = await api.getBlockHeaderByHeight(parseInt(hash, 10));
      hash = r?.block_header?.hash;
      if (!hash) throw new Error('Block not found at that height.');
    }

    // Try full explorer detail first; fall back to basic header on 500
    try {
      const result = await api.getBlockByHash(hash);
      const b = result?.block;
      if (!b) throw new Error('Invalid block response from daemon.');
      renderBlockDetail(b);
    } catch (detailErr) {
      const msg = detailErr?.message || '';
      if (/500/.test(msg)) {
        // Daemon couldn't produce full details — show header-only view
        const fallback = await api.getBlockHeaderByHash(hash);
        const bh = fallback?.block_header;
        if (!bh) throw new Error('Block not found.');
        renderBlockHeaderFallback(bh);
      } else {
        throw detailErr;
      }
    }

    clearError();
  } catch (err) {
    showError(classifyError(err, 'block'));
  }

  hideLoading();
}

// Shown when f_block_json returns 500 — uses the lighter getblockheaderbyhash fallback
function renderBlockHeaderFallback(bh) {
  const isOrphan = !!bh.orphan_status;
  const prevHashLink = !isZeroHash(bh.prev_hash)
    ? `<span class="hash-full"><a href="#/block/${escHtml(bh.prev_hash)}">${escHtml(bh.prev_hash)}</a></span>`
    : '<span style="color:var(--text-muted)">Genesis</span>';

  const prevH = Number(bh.height) > 0 ? Number(bh.height) - 1 : null;
  const nextH = Number(bh.depth)  > 0 ? Number(bh.height) + 1 : null;

  $('blockDetail').innerHTML = /* html */ `
    <div class="breadcrumb">
      <a href="#/">Home</a>
      <span class="breadcrumb-sep">/</span>
      <span>Block #${Number(bh.height).toLocaleString()}</span>
    </div>

    <div class="detail-heading">
      <h2>Block #${Number(bh.height).toLocaleString()}</h2>
      ${isOrphan
        ? '<span class="badge badge-red">Orphan</span>'
        : '<span class="badge badge-green">Confirmed</span>'}
      <span class="badge badge-yellow" title="Full block detail unavailable for this block">Partial data</span>
    </div>

    <div class="nav-arrows">
      ${prevH !== null
        ? `<a class="btn btn-outline btn-sm" href="#/block/${prevH}">← Prev</a>`
        : `<span class="btn btn-outline btn-sm" style="opacity:.35;pointer-events:none">← Prev</span>`}
      <a class="btn btn-outline btn-sm" href="#/">↑ All Blocks</a>
      ${nextH !== null
        ? `<a class="btn btn-outline btn-sm" href="#/block/${nextH}">Next →</a>`
        : `<span class="btn btn-outline btn-sm" style="opacity:.35;pointer-events:none">Next →</span>`}
    </div>

    <div class="detail-card">
      <h3>Block Header</h3>
      <table class="kv-table">
        <tr><th>Hash</th>          <td><span class="hash-full">${escHtml(bh.hash)}</span></td></tr>
        <tr><th>Height</th>        <td>${Number(bh.height).toLocaleString()}</td></tr>
        <tr><th>Timestamp</th>     <td>${formatTs(bh.timestamp)} <span style="color:var(--text-muted);font-size:.82em">(${timeAgo(bh.timestamp)})</span></td></tr>
        <tr><th>Previous Block</th><td class="break">${prevHashLink}</td></tr>
        <tr><th>Difficulty</th>    <td>${Number(bh.difficulty).toLocaleString()}</td></tr>
        <tr><th>Reward</th>        <td>${formatAmount(bh.reward)} ${COIN.ticker}</td></tr>
        <tr><th>Depth</th>         <td>${Number(bh.depth).toLocaleString()} confirmations</td></tr>
        <tr><th>Nonce</th>         <td class="mono">${bh.nonce}</td></tr>
        <tr><th>Version</th>       <td>${bh.major_version}.${bh.minor_version}</td></tr>
      </table>
    </div>

    <div class="alert" style="display:flex;margin-top:1rem;background:var(--yellow-dim);border:1px solid rgba(251,191,36,.25);color:var(--yellow)">
      <svg viewBox="0 0 20 20" fill="currentColor" width="16" height="16" style="flex-shrink:0">
        <path fill-rule="evenodd" d="M8.257 3.099c.765-1.36 2.722-1.36 3.486 0l5.58 9.92c.75 1.334-.213 2.98-1.742 2.98H4.42c-1.53 0-2.493-1.646-1.743-2.98l5.58-9.92zM11 13a1 1 0 11-2 0 1 1 0 012 0zm-1-8a1 1 0 00-1 1v3a1 1 0 002 0V6a1 1 0 00-1-1z" clip-rule="evenodd"/>
      </svg>
      <span style="margin-left:.5rem">Full block details are unavailable for this block (daemon returned an internal error). Only the block header is shown.</span>
    </div>
  `;
}

function renderBlockDetail(b) {
  const isOrphan  = !!b.orphan_status;
  const statusBadge = isOrphan
    ? '<span class="badge badge-red">Orphan</span>'
    : '<span class="badge badge-green">Confirmed</span>';

  const prevHashLink = !isZeroHash(b.prev_hash)
    ? `<span class="hash-full"><a href="#/block/${escHtml(b.prev_hash)}">${escHtml(b.prev_hash)}</a></span>`
    : '<span class="text-muted">Genesis</span>';

  const txRows = (b.transactions || []).map(tx => /* html */ `
    <tr>
      <td class="mono">
        <a href="#/tx/${escHtml(tx.hash)}">${escHtml(tx.hash)}</a>
      </td>
      <td>${formatAmount(tx.fee)} ${COIN.ticker}</td>
      <td>${formatAmount(tx.amount_out)} ${COIN.ticker}</td>
      <td>${formatSize(tx.size)}</td>
    </tr>
  `).join('');

  const txSection = b.transactions && b.transactions.length > 0
    ? /* html */ `
      <div class="detail-card full mt">
        <h3>Transactions (${b.transactions.length})</h3>
        <div class="table-wrap">
          <table class="data-table">
            <thead>
              <tr>
                <th>Hash</th>
                <th>Fee</th>
                <th>Amount Out</th>
                <th>Size</th>
              </tr>
            </thead>
            <tbody>${txRows}</tbody>
          </table>
        </div>
      </div>`
    : `<div class="detail-card full mt">
         <h3>Transactions</h3>
         <p style="color:var(--text-muted);padding:0.5rem 0;font-size:0.875rem;">
           Only coinbase transaction (no transfers).
         </p>
       </div>`;

  const prevH = Number(b.height) > 0 ? Number(b.height) - 1 : null;
  const nextH = Number(b.depth) > 0  ? Number(b.height) + 1 : null;

  const navArrows = /* html */ `
    <div class="nav-arrows">
      ${prevH !== null
        ? `<a class="btn btn-outline btn-sm" href="#/block/${prevH}">← Prev</a>`
        : `<span class="btn btn-outline btn-sm" style="opacity:.35;pointer-events:none">← Prev</span>`}
      <a class="btn btn-outline btn-sm" href="#/">↑ All Blocks</a>
      ${nextH !== null
        ? `<a class="btn btn-outline btn-sm" href="#/block/${nextH}">Next →</a>`
        : `<span class="btn btn-outline btn-sm" style="opacity:.35;pointer-events:none">Next →</span>`}
    </div>`;

  $('blockDetail').innerHTML = /* html */ `
    <div class="breadcrumb">
      <a href="#/">Home</a>
      <span class="breadcrumb-sep">/</span>
      <span>Block #${Number(b.height).toLocaleString()}</span>
    </div>

    <div class="detail-heading">
      <h2>Block #${Number(b.height).toLocaleString()}</h2>
      ${statusBadge}
    </div>

    ${navArrows}

    <div class="detail-grid">
      <div class="detail-card">
        <h3>Overview</h3>
        <table class="kv-table">
          <tr>
            <th>Hash</th>
            <td><span class="hash-full">${escHtml(b.hash)}</span></td>
          </tr>
          <tr><th>Height</th>         <td>${Number(b.height).toLocaleString()}</td></tr>
          <tr><th>Timestamp</th>      <td>${formatTs(b.timestamp)} <span style="color:var(--text-muted);font-size:.82em">(${timeAgo(b.timestamp)})</span></td></tr>
          <tr><th>Previous Block</th> <td class="break">${prevHashLink}</td></tr>
          <tr><th>Nonce</th>          <td class="mono">${b.nonce}</td></tr>
          <tr><th>Version</th>        <td>${b.major_version}.${b.minor_version}</td></tr>
          <tr><th>Depth</th>          <td>${Number(b.depth).toLocaleString()} confirmations</td></tr>
          <tr><th>Status</th>         <td>${isOrphan ? 'Orphan (not on main chain)' : 'On main chain'}</td></tr>
        </table>
      </div>

      <div class="detail-card">
        <h3>Mining &amp; Rewards</h3>
        <table class="kv-table">
          <tr><th>Difficulty</th>      <td>${Number(b.difficulty).toLocaleString()}</td></tr>
          <tr><th>Block Reward</th>    <td>${formatAmount(b.reward)} ${COIN.ticker}</td></tr>
          <tr><th>Base Reward</th>     <td>${formatAmount(b.baseReward)} ${COIN.ticker}</td></tr>
          <tr><th>Total Fees</th>      <td>${formatAmount(b.totalFeeAmount)} ${COIN.ticker}</td></tr>
          <tr><th>Block Size</th>      <td>${formatSize(b.blockSize)}</td></tr>
          <tr><th>TX Data Size</th>    <td>${formatSize(b.transactionsCumulativeSize)}</td></tr>
          <tr><th>Median Size</th>     <td>${formatSize(b.sizeMedian)}</td></tr>
          <tr><th>Circulating</th>     <td>${formatAmount(b.alreadyGeneratedCoins)} ${COIN.ticker}</td></tr>
          <tr><th>Total TXs Ever</th>  <td>${Number(b.alreadyGeneratedTransactions).toLocaleString()}</td></tr>
        </table>
      </div>
    </div>

    ${txSection}
  `;
}

// ─── TRANSACTION DETAIL ───────────────────────────────────────────────────────

async function showTx(hash) {
  setPage('tx');
  clearError();
  showLoading();
  setHtml('txDetail', '');

  try {
    try {
      const result = await api.getTxByHash(hash);
      if (!result?.txDetails) throw new Error('Invalid transaction response.');
      renderTxDetail(result);
    } catch (txErr) {
      const msg = txErr?.message || '';
      if (/500/.test(msg)) {
        // Confirmed TX unavailable — check if it's still in the mempool
        const poolResult = await api.getMempool();
        const poolTxs    = poolResult?.transactions || [];
        const poolEntry  = poolTxs.find(t => t.hash === hash);
        if (poolEntry) {
          renderPendingTx(poolEntry);
        } else {
          // Not in pool either — generic unavailable notice
          renderTxUnavailable(hash);
        }
      } else {
        throw txErr;
      }
    }
    clearError();
  } catch (err) {
    showError(classifyError(err, 'transaction'));
  }

  hideLoading();
}

// Shown for mempool transactions that haven't been confirmed yet
function renderPendingTx(tx) {
  $('txDetail').innerHTML = /* html */ `
    <div class="breadcrumb">
      <a href="#/">Home</a>
      <span class="breadcrumb-sep">/</span>
      <a href="#/mempool">Mempool</a>
      <span class="breadcrumb-sep">/</span>
      <span>Transaction</span>
    </div>

    <div class="detail-heading">
      <h2>Transaction</h2>
      <span class="badge badge-yellow">⏳ Pending</span>
    </div>

    <div class="detail-grid">
      <div class="detail-card">
        <h3>Overview</h3>
        <table class="kv-table">
          <tr><th>Hash</th>       <td><span class="hash-full">${escHtml(tx.hash)}</span></td></tr>
          <tr><th>Fee</th>        <td>${formatAmount(tx.fee)} ${COIN.ticker}</td></tr>
          <tr><th>Amount Out</th> <td>${formatAmount(tx.amount_out)} ${COIN.ticker}</td></tr>
          <tr><th>TX Size</th>    <td>${formatSize(tx.size)}</td></tr>
        </table>
      </div>

      <div class="detail-card">
        <h3>Status</h3>
        <table class="kv-table">
          <tr><th>Status</th>       <td><span class="badge badge-yellow">Unconfirmed</span></td></tr>
          <tr><th>Included In</th>  <td style="color:var(--text-muted)">Not yet in a block</td></tr>
          <tr><th>Confirmations</th><td style="color:var(--text-muted)">0</td></tr>
        </table>
      </div>
    </div>

    <div class="alert" style="display:flex;margin-top:1rem;background:var(--yellow-dim);border:1px solid rgba(251,191,36,.25);color:var(--yellow)">
      <svg viewBox="0 0 20 20" fill="currentColor" width="16" height="16" style="flex-shrink:0">
        <path fill-rule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zm1-12a1 1 0 10-2 0v4a1 1 0 00.293.707l2.828 2.829a1 1 0 101.415-1.415L11 9.586V6z" clip-rule="evenodd"/>
      </svg>
      <span style="margin-left:.5rem">
        This transaction is in the <strong>mempool</strong> (unconfirmed). Full detail is only available after it is included in a block.
        <a href="#/mempool" style="margin-left:.5rem">View mempool →</a>
      </span>
    </div>
  `;
}

// Shown when tx is not found in confirmed chain or mempool
function renderTxUnavailable(hash) {
  $('txDetail').innerHTML = /* html */ `
    <div class="breadcrumb">
      <a href="#/">Home</a>
      <span class="breadcrumb-sep">/</span>
      <span>Transaction</span>
    </div>

    <div class="detail-heading">
      <h2>Transaction</h2>
      <span class="badge badge-red">Unavailable</span>
    </div>

    <div class="detail-card">
      <h3>Hash</h3>
      <span class="hash-full">${escHtml(hash)}</span>
    </div>

    <div class="alert" style="display:flex;margin-top:1rem;background:var(--red-dim);border:1px solid rgba(248,113,113,.25);color:var(--red)">
      <svg viewBox="0 0 20 20" fill="currentColor" width="16" height="16" style="flex-shrink:0">
        <path fill-rule="evenodd" d="M18 10a8 8 0 11-16 0 8 8 0 0116 0zm-7 4a1 1 0 11-2 0 1 1 0 012 0zm-1-9a1 1 0 00-1 1v4a1 1 0 102 0V6a1 1 0 00-1-1z" clip-rule="evenodd"/>
      </svg>
      <span style="margin-left:.5rem">
        This transaction is not found in the confirmed chain or the mempool. It may have been evicted from the pool, or the node database may be missing data for this block.
      </span>
    </div>
  `;
}

function renderTxDetail(result) {
  const { txDetails, block, tx } = result;

  const ZERO_PAYMENT_ID = '0000000000000000000000000000000000000000000000000000000000000000';
  let paymentId = '<span style="color:var(--text-muted)">None</span>';

  if (txDetails.paymentId && txDetails.paymentId !== ZERO_PAYMENT_ID) {
    // A short payment ID is encrypted against the shared secret between the
    // sender and the receiver, so what the daemon hands us is ciphertext. Show
    // it, but never present it as a readable payment ID - only the two parties
    // to the transaction can recover the plaintext.
    paymentId = `<span class="hash-full">${escHtml(txDetails.paymentId)}</span>`;

    if (txDetails.paymentIdEncrypted) {
      paymentId += ' <span class="badge badge-accent" title="Encrypted to the receiver.'
        + ' Only the sender and receiver can read the real value.">Encrypted</span>';
    }
  }
  const txPublicKey = tx?.publicKey || txDetails?.extra?.publicKey || '';
  const txNonce = tx?.nonce || '';

  // CryptoNote serializes as "vin" / "vout" (not "inputs" / "outputs")
  // Type tags serialize as integers: 255 (0xff) = coinbase, 2 (0x02) = key input
  const inputs = tx?.vin || tx?.inputs || [];
  let inputsHtml = '';
  if (inputs.length) {
    const rows = inputs.map((inp, i) => {
      const tag = inp.type;
      const isCoinbase = tag === 0xff || tag === 255 || tag === 'ff'
                      || (inp.value && inp.value.height !== undefined && !inp.value.k_image);
      if (isCoinbase) {
        return /* html */ `<tr>
          <td>${i}</td>
          <td><span class="badge badge-accent">Coinbase</span></td>
          <td>—</td>
          <td class="mono">Height: ${inp.value?.height ?? '—'}</td>
        </tr>`;
      }
      const amount = inp.value?.amount ?? 0;
      const kimage = inp.value?.k_image ?? inp.value?.keyImage ?? '';
      return /* html */ `<tr>
        <td>${i}</td>
        <td><span class="badge badge-accent">Key</span></td>
        <td>${formatAmount(amount)} ${COIN.ticker}</td>
        <td class="mono hash-cell">${escHtml(kimage) || '—'}</td>
      </tr>`;
    }).join('');

    inputsHtml = /* html */ `
      <div class="table-wrap">
        <table class="data-table">
          <thead><tr><th>#</th><th>Type</th><th>Amount</th><th>Key Image</th></tr></thead>
          <tbody>${rows}</tbody>
        </table>
      </div>`;
  } else {
    inputsHtml = '<p style="color:var(--text-muted);font-size:.875rem;padding:0.5rem 0">Input data not available from daemon.</p>';
  }

  // CryptoNote serializes outputs as "vout" with target.type=2, target.data.key
  const outputs = tx?.vout || tx?.outputs || [];
  let outputsHtml = '';
  if (outputs.length) {
    const rows = outputs.map((out, i) => {
      const key = out.target?.data?.key ?? out.target?.key ?? '';
      return /* html */ `<tr data-out-idx="${i}">
        <td>${i}</td>
        <td>${formatAmount(out.amount)} ${COIN.ticker}</td>
        <td class="mono hash-cell">${escHtml(key) || '—'}</td>
      </tr>`;
    }).join('');

    outputsHtml = /* html */ `
      <div class="table-wrap">
        <table class="data-table">
          <thead><tr><th>#</th><th>Amount</th><th>Key</th></tr></thead>
          <tbody>${rows}</tbody>
        </table>
      </div>`;
  } else {
    outputsHtml = '<p style="color:var(--text-muted);font-size:.875rem;padding:0.5rem 0">Output data not available from daemon.</p>';
  }

  $('txDetail').innerHTML = /* html */ `
    <div class="breadcrumb">
      <a href="#/">Home</a>
      <span class="breadcrumb-sep">/</span>
      ${block?.hash
        ? `<a href="#/block/${escHtml(block.hash)}">Block #${Number(block.height).toLocaleString()}</a>`
        : '<span>Block</span>'}
      <span class="breadcrumb-sep">/</span>
      <span>Transaction</span>
    </div>

    <div class="detail-heading">
      <h2>Transaction</h2>
    </div>

    <div class="detail-grid">
      <div class="detail-card">
        <h3>Overview</h3>
        <table class="kv-table">
          <tr>
            <th>Hash</th>
            <td><span class="hash-full">${escHtml(txDetails.hash)}</span></td>
          </tr>
          <tr><th>Payment ID</th>  <td>${paymentId}</td></tr>
          <tr><th>TX Public Key</th><td class="mono hash-cell">${escHtml(txPublicKey) || 'â€”'}</td></tr>
          <tr><th>Nonce</th>       <td class="mono hash-cell">${escHtml(txNonce) || 'â€”'}</td></tr>
          <tr><th>Fee</th>         <td>${formatAmount(txDetails.fee)} ${COIN.ticker}</td></tr>
          <tr><th>Amount Out</th>  <td>${formatAmount(txDetails.amount_out)} ${COIN.ticker}</td></tr>
          <tr><th>Ring Size</th>   <td>${txDetails.mixin ?? '—'} + 1</td></tr>
          <tr><th>TX Size</th>     <td>${formatSize(txDetails.size)}</td></tr>
        </table>
      </div>

      <div class="detail-card">
        <h3>Included In Block</h3>
        <table class="kv-table">
          ${block ? /* html */ `
          <tr>
            <th>Height</th>
            <td><a href="#/block/${escHtml(block.hash)}">${Number(block.height).toLocaleString()}</a></td>
          </tr>
          <tr>
            <th>Hash</th>
            <td><span class="hash-full"><a href="#/block/${escHtml(block.hash)}">${escHtml(block.hash)}</a></span></td>
          </tr>
          <tr><th>Timestamp</th>   <td>${formatTs(block.timestamp)}</td></tr>
          <tr><th>Age</th>         <td>${timeAgo(block.timestamp)}</td></tr>
          <tr><th>Difficulty</th>  <td>${Number(block.difficulty).toLocaleString()}</td></tr>
          ` : '<tr><td colspan="2" style="color:var(--text-muted)">Block data unavailable</td></tr>'}
        </table>
      </div>
    </div>

    <div class="detail-card full mt">
      <h3>Inputs (${inputs.length})</h3>
      ${inputsHtml}
    </div>

    <div class="detail-card full mt">
      <h3>Outputs (${outputs.length})</h3>
      ${outputsHtml}
    </div>

    ${renderCheckTxCard()}
  `;
  initCheckTxEvents({ tx, txDetails });
}

// ─── PAYMENT ID ───────────────────────────────────────────────────────────────

async function showPaymentId(paymentId) {
  setPage('paymentid');
  clearError();
  showLoading();
  setHtml('paymentIdDetail', '');

  paymentId = (paymentId || '').trim().toLowerCase();

  /* A short payment ID is encrypted against the shared secret between sender
     and receiver, so the bytes on chain differ in every transaction it appears
     in. There is nothing stable to index, and saying so is far more use than
     an empty result that reads as "never used". */
  if (paymentId.length === 16) {
    setHtml('paymentIdDetail', /* html */ `
      <div class="detail-heading"><h2>Payment ID</h2></div>
      <div class="detail-card full">
        <div class="tool-badges">
          <span class="badge badge-accent">Encrypted</span>
          <span class="badge">16 characters</span>
        </div>
        ${field('Payment ID', paymentId)}
        <p style="color:var(--text-muted);padding:0.5rem 0;font-size:0.875rem;">
          Short payment IDs are encrypted to the receiver, so they cannot be
          looked up. The same payment ID produces different bytes in every
          transaction, and only the sender and the receiver hold the key needed
          to read it. Open the transaction directly, or use a wallet that owns
          one side of it.
        </p>
      </div>`);
    hideLoading();
    return;
  }

  if (!/^[0-9a-f]{64}$/.test(paymentId)) {
    hideLoading();
    showError(`<code>${escHtml(paymentId)}</code> is not a payment ID. `
            + `A long payment ID is 64 hexadecimal characters.`);
    return;
  }

  try {
    const result = await api.getTxsByPaymentId(paymentId);
    renderPaymentIdResults(paymentId, result);
    clearError();
  } catch (err) {
    showError(classifyError(err, 'payment ID'));
  }

  hideLoading();
}

function renderPaymentIdResults(paymentId, result) {
  const hashes = result?.transactionHashes || [];
  const total = result?.totalCount ?? hashes.length;

  if (hashes.length === 0) {
    setHtml('paymentIdDetail', /* html */ `
      <div class="detail-heading"><h2>Payment ID</h2></div>
      <div class="detail-card full">
        ${field('Payment ID', paymentId)}
        <p style="color:var(--text-muted);padding:0.5rem 0;font-size:0.875rem;">
          No transactions carry this payment ID. Note that only long, plaintext
          payment IDs can be found this way.
        </p>
      </div>`);
    return;
  }

  const rows = hashes.map(hash => /* html */ `
    <tr>
      <td class="mono"><a href="#/tx/${escHtml(hash)}">${escHtml(hash)}</a></td>
    </tr>
  `).join('');

  const truncatedNote = result?.truncated
    ? `<p style="color:var(--text-muted);padding:0.5rem 0;font-size:0.875rem;">Showing the first ${hashes.length} of ${total} transactions.
       This payment ID has been reused enough that the node capped the answer.</p>`
    : '';

  setHtml('paymentIdDetail', /* html */ `
    <div class="detail-heading"><h2>Payment ID</h2></div>
    <div class="detail-card full">
      <div class="tool-badges">
        <span class="badge badge-accent">Plaintext</span>
        <span class="badge">${total} transaction${total === 1 ? '' : 's'}</span>
      </div>
      ${field('Payment ID', paymentId)}
    </div>
    <div class="detail-card full mt">
      <h3>Transactions (${hashes.length})</h3>
      <div class="table-wrap">
        <table class="data-table">
          <thead><tr><th>Hash</th></tr></thead>
          <tbody>${rows}</tbody>
        </table>
      </div>
      ${truncatedNote}
    </div>`);
}

// ─── MEMPOOL ──────────────────────────────────────────────────────────────────

async function showMempool() {
  setPage('mempool');
  clearError();
  showLoading();
  setHtml('mempoolDetail', '');

  try {
    const result = await api.getMempool();
    const txs    = result?.transactions || [];
    renderMempool(txs);
    clearError();
  } catch (err) {
    showError(classifyError(err, 'mempool'));
    // Render empty state even on error
    $('mempoolDetail').innerHTML = '<div class="detail-heading"><h2>Transaction Pool</h2></div>';
  }

  hideLoading();

  // Auto-refresh mempool every 15s
  startRefresh(async () => {
    try {
      const result = await api.getMempool();
      renderMempool(result?.transactions || []);
      clearError();
    } catch (err) {
      showError(classifyError(err, 'mempool'));
    }
  }, 15);
}

function renderMempool(txs) {
  const totalFee = txs.reduce((acc, tx) => acc + Number(tx.fee || 0), 0);
  const totalSize = txs.reduce((acc, tx) => acc + Number(tx.size || 0), 0);

  const rows = txs.map(tx => /* html */ `
    <tr>
      <td class="mono">
        <a href="#/tx/${escHtml(tx.hash)}" title="${escHtml(tx.hash)}">${escHtml(tx.hash)}</a>
      </td>
      <td>${formatAmount(tx.fee)} ${COIN.ticker}</td>
      <td>${formatAmount(tx.amount_out)} ${COIN.ticker}</td>
      <td>${formatSize(tx.size)}</td>
    </tr>
  `).join('');

  $('mempoolDetail').innerHTML = /* html */ `
    <div class="detail-heading">
      <h2>Transaction Pool</h2>
      <span class="badge ${txs.length ? 'badge-yellow' : 'badge-green'}">
        ${txs.length} pending
      </span>
    </div>

    <div class="pool-banner">
      <div class="pool-stat">
        <div class="pool-stat-label">Pending Transactions</div>
        <div class="pool-stat-value">${txs.length}</div>
      </div>
      <div class="pool-stat" style="margin-left:2rem">
        <div class="pool-stat-label">Total Fees</div>
        <div class="pool-stat-value">${formatAmount(totalFee)} <small style="font-size:.6em;color:var(--text-muted)">${COIN.ticker}</small></div>
      </div>
      <div class="pool-stat" style="margin-left:2rem">
        <div class="pool-stat-label">Total Size</div>
        <div class="pool-stat-value">${formatSize(totalSize)}</div>
      </div>
    </div>

    ${txs.length === 0
      ? /* html */ `<div class="table-wrap">
          <table class="data-table">
            <thead><tr><th>Hash</th><th>Fee</th><th>Amount Out</th><th>Size</th></tr></thead>
            <tbody><tr><td colspan="4" class="empty-cell">
              Pool is empty — no pending transactions.
            </td></tr></tbody>
          </table>
        </div>`
      : /* html */ `<div class="table-wrap">
          <table class="data-table">
            <thead>
              <tr>
                <th>Hash</th>
                <th>Fee</th>
                <th>Amount Out</th>
                <th>Size</th>
              </tr>
            </thead>
            <tbody>${rows}</tbody>
          </table>
        </div>`
    }
  `;
}

// ─── CHECK TRANSACTION ────────────────────────────────────────────────────────
// Client-side CryptoNote output verification (never sends keys to server)

const _B58 = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';

function _cnDecodeBlock(s, outLen) {
  let n = 0n;
  for (const c of s) {
    const i = _B58.indexOf(c);
    if (i < 0) throw new Error('Invalid base58 char: ' + c);
    n = n * 58n + BigInt(i);
  }
  const out = new Uint8Array(outLen);
  for (let i = outLen - 1; i >= 0; i--) { out[i] = Number(n & 0xffn); n >>= 8n; }
  return out;
}

function cnB58Decode(str) {
  // TAIL maps (tail encoded chars → decoded bytes) per Monero base58 spec:
  // encoded_block_sizes = {0,2,3,5,6,7,9,10,11} → reverse: indices 1,4,8 are invalid
  const FE = 11, FD = 8, TAIL = [0, 0, 1, 2, 0, 3, 4, 5, 0, 6, 7];
  const full = Math.floor(str.length / FE), tail = str.length % FE;
  if (tail > 0 && TAIL[tail] === 0) throw new Error(`Invalid base58 address length (${str.length} chars).`);
  const out  = new Uint8Array(full * FD + TAIL[tail]);
  let off = 0;
  for (let i = 0; i < full; i++) {
    out.set(_cnDecodeBlock(str.slice(i * FE, (i+1) * FE), FD), off);
    off += FD;
  }
  if (tail > 0) out.set(_cnDecodeBlock(str.slice(full * FE), TAIL[tail]), off);
  return out;
}

function parseCnAddress(addr) {
  let bytes;
  try { bytes = cnB58Decode(addr); } catch (e) { throw new Error('Invalid address (base58 decode failed).'); }
  // Layout: varint(prefix) + spendKey(32) + viewKey(32) + checksum(4)
  let pfxLen = 0;
  while (pfxLen < bytes.length && (bytes[pfxLen] & 0x80)) pfxLen++;
  pfxLen++;
  if (bytes.length < pfxLen + 68) throw new Error('Invalid address length.');
  return { spendKey: bytes.slice(pfxLen, pfxLen + 32), viewKey: bytes.slice(pfxLen + 32, pfxLen + 64) };
}

function hexToU8(hex) {
  const out = new Uint8Array(hex.length >> 1);
  for (let i = 0; i < hex.length; i += 2) out[i >> 1] = parseInt(hex.slice(i, i+2), 16);
  return out;
}

function u8ToHex(u8) {
  return Array.from(u8).map(b => b.toString(16).padStart(2,'0')).join('');
}

// Interpret bytes as little-endian unsigned integer (CryptoNote scalar encoding)
function leBytesToBigInt(b) {
  let n = 0n;
  for (let i = 0; i < b.length; i++) n |= BigInt(b[i]) << (BigInt(i) * 8n);
  return n;
}

function varintEncode(n) {
  const out = [];
  let v = n;
  while (v > 0x7f) { out.push((v & 0x7f) | 0x80); v >>>= 7; }
  out.push(v & 0x7f);
  return new Uint8Array(out);
}

function parseTxPubKey(extra) {
  if (!extra) return null;
  if (typeof extra === 'string' && /^[0-9a-fA-F]{64}$/.test(extra) && !/^0+$/.test(extra)) {
    return hexToU8(extra);
  }
  if (typeof extra === 'object' && !Array.isArray(extra)) {
    if (typeof extra.publicKey === 'string' && /^[0-9a-fA-F]{64}$/.test(extra.publicKey) && !/^0+$/.test(extra.publicKey)) {
      return hexToU8(extra.publicKey);
    }
    if (extra.raw) return parseTxPubKey(extra.raw);
    return null;
  }
  const bytes = Array.isArray(extra) ? extra
    : typeof extra === 'string' && /^[0-9a-fA-F]+$/.test(extra) ? Array.from(hexToU8(extra))
    : null;
  if (!bytes) return null;
  for (let i = 0; i < bytes.length; i++) {
    if (bytes[i] === 0x01 && i + 32 < bytes.length) return new Uint8Array(bytes.slice(i+1, i+33));
  }
  return null;
}

function getTxPubKeyBytes(txContext) {
  const candidates = [
    txContext?.txDetails?.extra?.publicKey,
    txContext?.tx?.extra?.publicKey,
    txContext?.extra?.publicKey,
    txContext?.txDetails?.extra,
    txContext?.tx?.extra,
    txContext?.extra,
    txContext?.txDetails?.extra?.raw,
    txContext?.tx?.extra?.raw,
    txContext?.tx?.publicKey,
    txContext?.publicKey,
  ];
  for (const extra of candidates) {
    const txPubKey = parseTxPubKey(extra);
    if (txPubKey) return txPubKey;
  }

  return null;
}

async function runCheckTx(txContext, address, keyHex, keyType) {
  await ensureTurtleCoinUtils();
  if (!window._cnUtils) {
    window._cnUtils = new window.TurtleCoinUtils.CryptoNote({
      coinUnitPlaces: COIN.decimals,
      addressPrefix: COIN.addressPrefix,
    });
  }

  if (!/^[0-9a-fA-F]{64}$/.test(keyHex)) throw new Error('Key must be exactly 64 hex characters.');

  let decodedAddress;
  try {
    decodedAddress = window._cnUtils.decodeAddress(address);
  } catch (err) {
    throw new Error('Invalid recipient address.');
  }
  if (!decodedAddress) throw new Error('Invalid recipient address.');

  const txPubBytes = getTxPubKeyBytes(txContext);
  const txPubHex = txPubBytes ? u8ToHex(txPubBytes) : '';
  const outputs = txContext?.tx?.vout || txContext?.tx?.outputs || [];
  const matched = [];

  for (let i = 0; i < outputs.length; i++) {
    const outKey = outputs[i]?.target?.data?.key ?? outputs[i]?.target?.key ?? '';
    if (!outKey) continue;

    const output = { index: i, key: outKey };
    let owned = false;

    if (txPubHex) {
      owned = window._cnUtils.isOurTransactionOutput(txPubHex, output, keyHex, decodedAddress.publicSpendKey);
    }

    if (!owned) {
      owned = window._cnUtils.isOurTransactionOutput(
        decodedAddress.publicViewKey,
        output,
        keyHex,
        decodedAddress.publicSpendKey
      );
    }

    if (owned) matched.push(i);
  }

  return matched;
}

function renderCheckTxCard() {
  return /* html */ `
    <div class="detail-card full mt">
      <h3>Check Transaction</h3>
      <p style="color:var(--text-muted);font-size:0.82rem;margin-bottom:1rem">
        Verify which outputs in this transaction belong to a specific address.
        All cryptographic operations run entirely in your browser — your private key is never transmitted.
      </p>
      <div class="check-form">
        <div class="check-row">
          <label class="check-label" for="ckAddr">Recipient Address</label>
          <input id="ckAddr" class="check-input" type="text" placeholder="WrkZ… address" spellcheck="false" autocomplete="off">
        </div>
        <div class="check-row">
          <label class="check-label" for="ckKey">Private Key</label>
          <input id="ckKey" class="check-input" type="text" placeholder="64-char private view key or tx private key" spellcheck="false" autocomplete="off">
        </div>
        <div style="display:flex;align-items:center;gap:0.75rem;flex-wrap:wrap">
          <button id="ckBtn" class="btn btn-primary">Check Transaction</button>
          <span id="ckStatus" style="font-size:0.82rem"></span>
        </div>
      </div>
      <div id="ckResult" style="margin-top:1rem"></div>
    </div>`;
}

function initCheckTxEvents(txContext) {
  const btn = $('ckBtn');
  if (!btn) return;
  const statusEl = $('ckStatus');

  btn.disabled = true;
  if (statusEl) {
    statusEl.style.color = 'var(--text-muted)';
    statusEl.textContent = 'Loading crypto tools…';
  }

  ensureTurtleCoinUtils()
    .then(() => {
      btn.disabled = false;
      if (statusEl) statusEl.textContent = '';
    })
    .catch(err => {
      if (statusEl) {
        statusEl.style.color = 'var(--red)';
        statusEl.textContent = err.message;
      }
    });

  btn.addEventListener('click', async () => {
    const addr    = ($('ckAddr')?.value || '').trim();
    const keyHex  = ($('ckKey')?.value  || '').trim();
    const keyType = 'auto';
    const resultEl = $('ckResult');

    const setErr = msg => { statusEl.style.color = 'var(--red)'; statusEl.textContent = msg; };

    if (!addr)   { setErr('Please enter an address.'); return; }
    if (!keyHex) { setErr('Please enter a private key.'); return; }

    statusEl.style.color = 'var(--text-muted)';
    statusEl.textContent = 'Computing…';
    resultEl.innerHTML = '';
    btn.disabled = true;

    // Clear previous highlights
    document.querySelectorAll('[data-out-idx]').forEach(r => r.classList.remove('output-matched','output-nomatch'));

    try {
      const matched = await runCheckTx(txContext, addr, keyHex, keyType);
      statusEl.textContent = '';

      const outputs = txContext?.tx?.vout || txContext?.tx?.outputs || [];
      if (matched.length > 0) {
        matched.forEach(idx => {
          document.querySelector(`[data-out-idx="${idx}"]`)?.classList.add('output-matched');
        });
        document.querySelectorAll('[data-out-idx]').forEach(r => {
          if (!matched.includes(parseInt(r.dataset.outIdx, 10))) r.classList.add('output-nomatch');
        });
        const total = matched.reduce((s, i) => s + Number(outputs[i]?.amount || 0), 0);
        resultEl.innerHTML = `<div class="check-result-ok">Found <strong>${formatAmount(total)} ${COIN.ticker}</strong></div>`;
      } else {
        resultEl.innerHTML = `<div class="check-result-none">No outputs matched this address / key combination.</div>`;
      }
    } catch (err) {
      setErr(err.message);
    } finally {
      btn.disabled = false;
    }
  });
}

/* ═══════════════════════════════════════════════════════════════════════════
   WALLET / ADDRESS TOOLS

   Everything below runs entirely in the browser against vendor/wrkz-crypto.js.
   No key material, seed, or address is ever sent to the daemon or anywhere else.
   ═══════════════════════════════════════════════════════════════════════════ */

function wrkzCrypto() {
  if (!window.WrkzCrypto) {
    throw new Error('vendor/wrkz-crypto.js failed to load, so these tools are unavailable.');
  }
  return window.WrkzCrypto;
}

function setToolStatus(id, msg, kind) {
  const el = $(id);
  if (!el) return;
  el.className = 'tool-status' + (kind ? ` ${kind}` : '');
  el.textContent = msg || '';
}

/* One labelled, copyable field. `secret` only changes the colour — it is a
   reminder to the reader, not a security control. */
function field(label, value, opts) {
  const { secret = false, copy = true } = opts || {};
  const safe = escHtml(value);
  return /* html */ `
    <div class="field">
      <div class="field-label">${escHtml(label)}</div>
      <div class="field-value${secret ? ' secret' : ''}">${safe}</div>
      ${copy ? `<button type="button" class="copy-btn" data-copy="${safe}">Copy</button>` : ''}
    </div>`;
}

function renderSeedGrid(mnemonic) {
  const words = mnemonic.split(' ');
  const cells = words.map((w, i) => /* html */ `
    <div class="seed-word">
      <span class="seed-index">${i + 1}</span>
      <span class="seed-text">${escHtml(w)}</span>
    </div>`).join('');

  return /* html */ `
    <div class="detail-card full mt">
      <h3>Mnemonic Seed — 25 words</h3>
      <div class="seed-grid">${cells}</div>
      <div class="tool-actions">
        <button type="button" class="copy-btn" data-copy="${escHtml(mnemonic)}">Copy seed</button>
      </div>
    </div>`;
}

/* The full key set, shared by the paper wallet and the seed importer. */
function renderKeySet(wallet) {
  return /* html */ `
    <div class="detail-card full">
      <h3>Address</h3>
      ${field('Address', wallet.address)}
    </div>

    ${renderSeedGrid(wallet.mnemonic)}

    <div class="detail-card full mt">
      <h3>Private Keys — keep these secret</h3>
      ${field('Private Spend Key', wallet.privateSpendKey, { secret: true })}
      ${field('Private View Key',  wallet.privateViewKey,  { secret: true })}
    </div>

    <div class="detail-card full mt">
      <h3>Public Keys</h3>
      ${field('Public Spend Key', wallet.publicSpendKey)}
      ${field('Public View Key',  wallet.publicViewKey)}
    </div>`;
}

/* Clipboard, with a fallback for pages served over plain http where
   navigator.clipboard is unavailable. */
async function copyText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(text);
    return;
  }
  const ta = document.createElement('textarea');
  ta.value = text;
  ta.setAttribute('readonly', '');
  ta.style.position = 'fixed';
  ta.style.opacity = '0';
  document.body.appendChild(ta);
  ta.select();
  try {
    if (!document.execCommand('copy')) throw new Error('Copy was rejected by the browser.');
  } finally {
    document.body.removeChild(ta);
  }
}

function initCopyButtons() {
  document.addEventListener('click', async e => {
    const btn = e.target.closest('.copy-btn');
    if (!btn || btn.dataset.busy === '1') return;

    /* Capture the label before it is replaced, so a second click while the
       "Copied" flash is still showing cannot make it stick. */
    const original = btn.textContent;
    btn.dataset.busy = '1';

    let ok = true;
    try {
      await copyText(btn.dataset.copy || '');
    } catch (err) {
      ok = false;
    }

    btn.textContent = ok ? 'Copied' : 'Copy failed';
    btn.classList.toggle('copied', ok);
    setTimeout(() => {
      btn.textContent = original;
      btn.classList.remove('copied');
      delete btn.dataset.busy;
    }, ok ? 1400 : 1800);
  });
}

function randomPaymentId(length) {
  const webcrypto = window.crypto || window.msCrypto;
  if (!webcrypto || typeof webcrypto.getRandomValues !== 'function') {
    throw new Error('This browser has no secure random number generator, '
                  + 'so a payment ID cannot be generated safely.');
  }
  const bytes = new Uint8Array(length / 2);
  webcrypto.getRandomValues(bytes);
  return wrkzCrypto().hex(bytes);
}

// ─── PAPER WALLET ─────────────────────────────────────────────────────────────

function showPaperWallet() {
  setPage('paper');
}

function initPaperWallet() {
  const btn = $('paperGenBtn');
  if (!btn) return;

  btn.addEventListener('click', () => {
    $('paperResult').innerHTML = '';
    $('paperPrintBtn').hidden = true;

    try {
      const wallet = wrkzCrypto().createWallet();
      $('paperResult').innerHTML = renderKeySet(wallet);
      $('paperPrintBtn').hidden = false;
      setToolStatus('paperStatus', 'New wallet generated. Nothing was sent anywhere.', 'ok');
    } catch (err) {
      setToolStatus('paperStatus', err.message, 'err');
    }
  });

  $('paperPrintBtn').addEventListener('click', () => window.print());
}

// ─── IMPORT FROM SEED ─────────────────────────────────────────────────────────

function showImportSeed() {
  setPage('import');
}

function initImportSeed() {
  const btn = $('importBtn');
  if (!btn) return;

  const run = () => {
    const input = $('importInput').value;
    $('importResult').innerHTML = '';

    if (!input.trim()) {
      setToolStatus('importStatus', 'Enter a mnemonic seed or a private spend key.', 'err');
      return;
    }

    try {
      const wallet = wrkzCrypto().importWallet(input);
      $('importResult').innerHTML = renderKeySet(wallet);
      setToolStatus('importStatus', 'Wallet recovered.', 'ok');
    } catch (err) {
      setToolStatus('importStatus', err.message, 'err');
    }
  };

  btn.addEventListener('click', run);

  $('importClearBtn').addEventListener('click', () => {
    $('importInput').value = '';
    $('importResult').innerHTML = '';
    setToolStatus('importStatus', '');
  });
}

// ─── INTEGRATED ADDRESS GENERATOR ─────────────────────────────────────────────

function showIntegratedAddress() {
  setPage('integrated');
}

function initIntegratedAddress() {
  const btn = $('intBtn');
  if (!btn) return;

  const setPid = length => {
    try {
      $('intPid').value = randomPaymentId(length);
      setToolStatus('intStatus', '');
    } catch (err) {
      setToolStatus('intStatus', err.message, 'err');
    }
  };

  $('intRand16').addEventListener('click', () => setPid(16));
  $('intRand64').addEventListener('click', () => setPid(64));

  btn.addEventListener('click', () => {
    const address   = $('intAddr').value.trim();
    const paymentId = $('intPid').value.trim();
    $('intResult').innerHTML = '';

    try {
      const W = wrkzCrypto();
      const integrated = W.createIntegratedAddress(address, paymentId);
      const decoded    = W.decodeAddress(integrated);

      $('intResult').innerHTML = /* html */ `
        <div class="detail-card full">
          <h3>Integrated Address</h3>
          <div class="tool-badges">
            <span class="badge badge-accent">${decoded.paymentIdType === 'short' ? 'Short' : 'Long'} payment ID</span>
            <span class="badge">${integrated.length} characters</span>
          </div>
          ${field('Integrated Address', integrated)}
          ${field('Payment ID', decoded.paymentId)}
          ${field('Standard Address', decoded.baseAddress)}
        </div>`;

      setToolStatus('intStatus', 'Created.', 'ok');
    } catch (err) {
      setToolStatus('intStatus', err.message, 'err');
    }
  });
}

// ─── ADDRESS DECODER ──────────────────────────────────────────────────────────

function showDecodeAddress(prefill) {
  setPage('decode');
  if (!prefill) return;

  /* A hand-edited hash can contain a stray "%", which would make
     decodeURIComponent throw and take the router down with it. */
  let address;
  try {
    address = decodeURIComponent(prefill);
  } catch (err) {
    address = prefill;
  }

  $('decAddr').value = address;
  runDecodeAddress();
}

function runDecodeAddress() {
  const address = $('decAddr').value.trim();
  $('decResult').innerHTML = '';

  if (!address) {
    setToolStatus('decStatus', 'Enter an address to decode.', 'err');
    return;
  }

  try {
    const decoded = decodeAddressForDisplay(address);
    $('decResult').innerHTML = decoded;
    setToolStatus('decStatus', 'Checksum verified.', 'ok');
  } catch (err) {
    setToolStatus('decStatus', err.message, 'err');
  }
}

function decodeAddressForDisplay(address) {
  const W = wrkzCrypto();
  const d = W.decodeAddress(address);

  const kind = !d.isIntegrated ? 'Standard address'
             : d.paymentIdType === 'short' ? 'Integrated address (short payment ID)'
             : 'Integrated address (long payment ID)';

  const integratedRows = d.isIntegrated ? `
      ${field('Payment ID', d.paymentId)}
      ${field('Standard Address', d.baseAddress)}` : '';

  return /* html */ `
    <div class="detail-card full">
      <h3>Decoded</h3>
      <div class="tool-badges">
        <span class="badge badge-accent">${escHtml(kind)}</span>
        <span class="badge">${address.trim().length} characters</span>
        <span class="badge badge-green">Checksum OK</span>
      </div>
      ${field('Address Prefix', String(d.prefix), { copy: false })}
      ${integratedRows}
      ${field('Public Spend Key', d.publicSpendKey)}
      ${field('Public View Key',  d.publicViewKey)}
    </div>`;
}

function initDecodeAddress() {
  const btn = $('decBtn');
  if (!btn) return;

  btn.addEventListener('click', runDecodeAddress);

  $('decClearBtn').addEventListener('click', () => {
    $('decAddr').value = '';
    $('decResult').innerHTML = '';
    setToolStatus('decStatus', '');
  });
}

// ─── INIT ─────────────────────────────────────────────────────────────────────

function init() {
  // Apply saved theme
  applyTheme(theme);

  // Theme toggle
  $('themeToggle').addEventListener('click', toggleTheme);

  // Search
  $('searchForm').addEventListener('submit', e => {
    e.preventDefault();
    doSearch($('searchInput').value);
  });

  // Wallet / address tools — handlers are bound once, pages are shown by route()
  initToolsMenu();
  initCopyButtons();
  initPaperWallet();
  initImportSeed();
  initIntegratedAddress();
  initDecodeAddress();

  // Hash routing
  window.addEventListener('hashchange', route);

  // Initial route
  route();
}

document.addEventListener('DOMContentLoaded', init);
