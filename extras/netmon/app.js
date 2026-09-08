/* ═══════════════════════════════════════════════════════════════════════════
   WrkzCoin Network Monitor — front end

   Talks to wrkz-netmon's own JSON API. When the binary serves this folder
   itself (--web-root) the API is same-origin at /api; behind nginx it is
   wherever the proxy puts it, which is also /api by convention. Nothing here
   needs configuring for either case.
   ═══════════════════════════════════════════════════════════════════════════ */

'use strict';

const API = '/api';

/* Refresh cadence. The crawler sweeps every few minutes, so anything faster
   than this just re-renders identical numbers. */
const POLL_MS = 20000;

let pollTimer = null;
let peersState = { filter: 'all', offset: 0, limit: 100, q: '' };
let lastSummary = null;

/* ─── HELPERS ─────────────────────────────────────────────────────────────── */

const el = (id) => document.getElementById(id);

function esc(text) {
  return String(text == null ? '' : text)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function num(value) {
  return Number(value || 0).toLocaleString();
}

function pct(part, whole) {
  if (!whole) return '0.0%';
  return `${((part / whole) * 100).toFixed(1)}%`;
}

function ago(unix) {
  if (!unix) return 'never';
  const seconds = Math.max(0, Math.floor(Date.now() / 1000) - Number(unix));
  if (seconds < 90) return `${seconds} s`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 90) return `${minutes} min`;
  const hours = Math.floor(minutes / 60);
  if (hours < 48) return `${hours} h`;
  return `${Math.floor(hours / 24)} d`;
}

function stamp(unix) {
  if (!unix) return '—';
  const d = new Date(Number(unix) * 1000);
  return `${String(d.getUTCDate()).padStart(2, '0')} ` +
    ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'][d.getUTCMonth()] +
    ` ${d.getUTCFullYear()} · ${String(d.getUTCHours()).padStart(2, '0')}:` +
    `${String(d.getUTCMinutes()).padStart(2, '0')} UTC`;
}

function shortHash(hash) {
  if (!hash) return '—';
  return `${hash.slice(0, 8)}…${hash.slice(-6)}`;
}

async function get(path) {
  const res = await fetch(API + path, { headers: { Accept: 'application/json' } });
  if (!res.ok) throw new Error(`HTTP ${res.status} on ${path}`);
  return res.json();
}

function showError(message) {
  const box = el('globalError');
  box.textContent = message;
  box.classList.remove('hidden');
}

function clearError() {
  el('globalError').classList.add('hidden');
}

/* ─── THEME ───────────────────────────────────────────────────────────────── */

function applyTheme(theme) {
  document.documentElement.setAttribute('data-theme', theme);
  el('iconMoon').classList.toggle('hidden', theme === 'light');
  el('iconSun').classList.toggle('hidden', theme !== 'light');
  try { localStorage.setItem('netmon-theme', theme); } catch (e) { /* private window */ }
}

function initTheme() {
  let theme = 'dark';
  try { theme = localStorage.getItem('netmon-theme') || 'dark'; } catch (e) { /* ignore */ }
  applyTheme(theme);
  el('themeToggle').addEventListener('click', () => {
    applyTheme(document.documentElement.getAttribute('data-theme') === 'light' ? 'dark' : 'light');
  });
}

/* ─── SHARED RENDERERS ────────────────────────────────────────────────────── */

function statCard(label, value, sub, tone) {
  return `<div class="stat-card${tone ? ' ' + tone : ''}">
    <div class="stat-label">${esc(label)}</div>
    <div class="stat-value">${value}</div>
    <div class="stat-sub${tone ? ' ' + tone : ''}">${esc(sub || '')}</div>
  </div>`;
}

function barRows(rows, max) {
  const peak = max || rows.reduce((m, r) => Math.max(m, r.count), 0) || 1;
  return rows.map((row) => `<div class="bar-row">
      <span class="label">${esc(row.label)}</span>
      <span class="track"><span class="fill${row.tone ? ' ' + row.tone : ''}"
        style="width: ${((row.count / peak) * 100).toFixed(1)}%"></span></span>
      <span class="count"><b>${num(row.count)}</b> <span>${row.share || ''}</span></span>
    </div>`).join('');
}

function stack(parts, thin) {
  const total = parts.reduce((sum, p) => sum + p.count, 0) || 1;
  const segments = parts.map((p) => `<span style="width: ${((p.count / total) * 100).toFixed(2)}%;
    background: ${p.color}">${thin ? '' : esc(p.text || '')}</span>`).join('');
  return `<div class="stack${thin ? ' thin' : ''}">${segments}</div>`;
}

function legend(parts, total) {
  return `<div class="legend">${parts.map((p) => `<div class="legend-row">
      <span class="legend-swatch" style="background: ${p.color}"></span>
      <span class="name">${esc(p.name)}</span>
      ${p.note ? `<span class="note">${esc(p.note)}</span>` : ''}
      <span class="num"><b>${num(p.count)}</b> <span>${pct(p.count, total)}</span></span>
    </div>`).join('')}</div>`;
}

function sparkline(points, accessor) {
  if (points.length < 2) {
    return '<div class="loading">Not enough sweeps yet to draw a line.</div>';
  }
  const values = points.map(accessor);
  const min = Math.min.apply(null, values);
  const max = Math.max.apply(null, values);
  const span = max - min || 1;
  const W = 1000;
  const H = 90;
  const step = W / (points.length - 1);
  const coords = values.map((v, i) => {
    const x = (i * step).toFixed(1);
    const y = (H - 10 - ((v - min) / span) * (H - 24)).toFixed(1);
    return `${x},${y}`;
  });
  return `<svg class="spark" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none" aria-hidden="true">
      <path class="spark-area" d="M${coords.join(' ')} L${W},${H} L0,${H} Z"></path>
      <polyline class="spark-line" points="${coords.join(' ')}"
        vector-effect="non-scaling-stroke"></polyline>
    </svg>`;
}

/* ─── OVERVIEW ────────────────────────────────────────────────────────────── */

function renderStatus(s) {
  const interval = s.sweep.intervalSeconds;
  const since = Math.max(0, Math.floor(Date.now() / 1000) - Number(s.sweep.finishedAt || 0));
  const nextIn = Math.max(0, interval - since);

  el('statusStrip').innerHTML = `
    <span class="badge badge-green">
      <svg width="8" height="8" viewBox="0 0 8 8"><circle cx="4" cy="4" r="4" fill="currentColor"/></svg>
      CRAWLING
    </span>
    <span class="status-text">Sweep <strong>#${num(s.sweep.number)}</strong> finished
      ${esc(ago(s.sweep.finishedAt))} ago · probed <strong>${num(s.sweep.probed)}</strong> addresses
      in ${Math.round((s.sweep.durationMs || 0) / 1000)} s · next sweep in
      ${Math.floor(nextIn / 60)} min</span>
    <span class="status-right">bootstrap: ${num((s.seeds || []).length)} seed addresses</span>`;

  el('footerVersion').textContent = `WrkzCoin Network Monitor ${s.version || ''}`;
  el('footerSweep').textContent = `sweep #${num(s.sweep.number)}, ${num(s.reachable)} reachable of ${num(s.known)} known`;
}

function renderHero(s) {
  const tips = s.topHashes || [];
  const majority = tips.length ? tips[0] : null;
  const agreed = tips.length <= 1;

  el('heroStats').innerHTML = [
    statCard('Reachable (white)', num(s.reachable),
      `${s.churn24h.joined ? '+' : ''}${num(s.churn24h.joined - s.churn24h.left)} in 24 h`,
      s.churn24h.joined >= s.churn24h.left ? 'good' : 'warn'),
    statCard('Known only (grey)', num(s.grey), 'port closed or offline'),
    statCard('Network height', num(s.networkHeight), 'tallest chain claimed'),
    statCard('Chain agreement',
      `<span class="badge ${agreed ? 'badge-green' : 'badge-yellow'}"
        style="font-size: 0.95rem; padding: 0.45rem 0.85rem">${agreed ? 'ONE TIP' : `${tips.length} TIPS`}</span>`,
      majority ? `${num(majority.count)} of ${num(s.reachable)} on the commonest hash` : 'no handshakes yet',
      agreed ? 'good' : 'warn'),
    statCard('IPv6 reachable', num(s.transport.ipv6), `${pct(s.transport.ipv6, s.reachable)} of reachable`),
    statCard('Median handshake', `${num(s.rtt.medianMs)} ms`, `p95 ${num(s.rtt.p95Ms)} ms`)
  ].join('');
}

function renderHeightBars(s) {
  const tones = ['good', 'good', 'warn', 'warn', 'bad', 'mute'];
  const rows = (s.heightBuckets || []).map((b, i) => ({
    label: b.label, count: b.count, tone: tones[i] || '', share: pct(b.count, s.reachable)
  }));
  el('heightBars').innerHTML = barRows(rows);

  const tips = s.topHashes || [];
  if (!tips.length) {
    el('topHashes').innerHTML = '<div class="loading">No node has completed a handshake yet.</div>';
    return;
  }
  el('topHashes').innerHTML = tips.slice(0, 6).map((tip, i) => `
    <div style="display: flex; align-items: center; gap: 0.85rem; flex-wrap: wrap">
      <span class="mono" style="min-width: 158px">${esc(shortHash(tip.label))}</span>
      <span class="badge ${i === 0 ? 'badge-green' : 'badge-yellow'}">${i === 0 ? 'MAJORITY' : 'BEHIND'}</span>
      <span class="section-note">${num(tip.count)} node${tip.count === 1 ? '' : 's'}</span>
    </div>`).join('');
}

function renderCapability(s) {
  const parts = [
    { name: 'Full', count: s.capability.full, color: 'var(--accent)' },
    { name: 'Pruned', count: s.capability.pruned, color: 'var(--yellow)' },
    { name: 'Lite', count: s.capability.lite, color: 'var(--green)' }
  ];
  el('capabilityPanel').innerHTML =
    stack(parts.map((p) => ({ count: p.count, color: p.color })), true) + legend(parts, s.reachable);
}

function renderTransport(s) {
  const transport = [
    { name: 'IPv4', count: s.transport.ipv4, color: 'var(--accent)' },
    { name: 'IPv6', count: s.transport.ipv6, color: 'var(--accent-hover)' }
  ];
  const clocks = [
    { label: 'Within ±5 s', count: s.clockSkew.within5s, tone: 'good' },
    { label: '5–60 s off', count: s.clockSkew.under60s, tone: 'warn' },
    { label: 'Over 60 s off', count: s.clockSkew.over60s, tone: 'bad' }
  ];
  el('transportPanel').innerHTML =
    stack(transport.map((p) => ({ count: p.count, color: p.color })), true) +
    legend(transport, s.reachable) +
    `<div class="stat-label" style="margin: 1.1rem 0 0.7rem">Clock skew vs. this host</div>` +
    barRows(clocks.map((c) => ({ ...c, share: pct(c.count, s.reachable) })));
}

const VERSION_COLORS = ['var(--accent)', 'var(--green)', 'var(--yellow)', 'var(--red)', 'var(--text-faint)'];

function renderVersionOverview(s) {
  const versions = (s.versions || []).slice().sort((a, b) =>
    Number(b.label.slice(1)) - Number(a.label.slice(1)));

  if (!versions.length) {
    el('versionOverview').innerHTML = '<div class="loading">No handshakes recorded yet.</div>';
    return;
  }

  const parts = versions.map((v, i) => ({
    count: v.count, color: VERSION_COLORS[i] || 'var(--text-faint)', text: v.label
  }));

  el('versionNote').textContent =
    `${num(s.softwareKnown)} of ${num(s.reachable)} reachable nodes also publish a software version`;

  el('versionOverview').innerHTML = stack(parts) +
    legend(versions.map((v, i) => ({
      name: v.label, count: v.count, color: VERSION_COLORS[i] || 'var(--text-faint)'
    })), s.reachable);
}

function renderChurn(s) {
  const points = s.sweepHistory || [];
  el('churnPanel').innerHTML = `
    ${sparkline(points, (p) => p.reachable)}
    <div class="legend" style="margin-top: 1rem">
      <div class="legend-row"><span class="name">Joined in 24 h</span>
        <span class="num" style="color: var(--green)"><b>${num(s.churn24h.joined)}</b></span></div>
      <div class="legend-row"><span class="name">Left in 24 h</span>
        <span class="num" style="color: var(--red)"><b>${num(s.churn24h.left)}</b></span></div>
      <div class="legend-row"><span class="name">Sweeps kept</span>
        <span class="num"><b>${num(points.length)}</b></span></div>
    </div>`;
}

async function loadOverview() {
  const s = await get('/summary');
  lastSummary = s;
  renderStatus(s);
  renderHero(s);
  renderHeightBars(s);
  renderCapability(s);
  renderTransport(s);
  renderVersionOverview(s);
  renderChurn(s);
}

/* ─── PEERS ───────────────────────────────────────────────────────────────── */

const PEER_FILTERS = [
  { key: 'all', label: 'All' },
  { key: 'reachable', label: 'Reachable' },
  { key: 'grey', label: 'Grey' },
  { key: 'full', label: 'Full' },
  { key: 'pruned', label: 'Pruned' },
  { key: 'lite', label: 'Lite' },
  { key: 'ipv6', label: 'IPv6' },
  { key: 'behind', label: 'Off majority tip' }
];

function reachBadge(reach) {
  const map = {
    open: 'badge-green', timeout: 'badge-yellow',
    refused: 'badge-red', failed: 'badge-red', unknown: 'badge-mute'
  };
  return `<span class="badge ${map[reach] || 'badge-mute'}">${esc(reach.toUpperCase())}</span>`;
}

function capabilityBadge(peer) {
  const r = peer.reported;
  if (!r.haveHandshake) return '<span style="color: var(--text-faint)">—</span>';
  if (r.lite) return `<span class="badge badge-green">LITE ${num(r.liteStartHeight)}</span>`;
  if (r.pruned) return `<span class="badge badge-yellow">PRUNED ${num(r.prunedHeight)}</span>`;
  return '<span class="badge badge-accent">FULL</span>';
}

function uptimeBar(peer) {
  if (!peer.sweepsOffered) return '<span style="color: var(--text-faint)">—</span>';
  const share = peer.sweepsAnswered / peer.sweepsOffered;
  const tone = share > 0.95 ? 'good' : share > 0.5 ? 'warn' : 'bad';
  return `<span class="track" style="display: block; width: 72px; height: 8px;
      background: var(--bg-code); border-radius: 99px; overflow: hidden"
      title="${num(peer.sweepsAnswered)} of ${num(peer.sweepsOffered)} sweeps">
      <span class="fill ${tone}" style="display: block; height: 100%;
        width: ${(share * 100).toFixed(0)}%; border-radius: 99px"></span></span>`;
}

function peerRow(peer, majorityHash) {
  const r = peer.reported;
  const dot = { open: 'var(--green)', timeout: 'var(--yellow)' }[peer.reach] || 'var(--text-faint)';
  const offTip = r.haveHandshake && majorityHash && r.topId !== majorityHash;
  const dash = '<span style="color: var(--text-faint)">—</span>';

  return `<tr class="${offTip ? 'off-tip' : ''}" data-key="${esc(peer.key)}">
    <td><svg width="8" height="8" viewBox="0 0 8 8"><circle cx="4" cy="4" r="4" fill="${dot}"/></svg></td>
    <td><span class="mono" style="color: var(--accent)">${esc(peer.key)}</span>
      ${peer.isSeed ? ' <span class="badge badge-accent">SEED</span>' : ''}</td>
    <td>${reachBadge(peer.reach)}</td>
    <td>${r.haveHandshake ? `<b>v${r.p2pVersion}</b>` : dash}</td>
    <td>${r.softwareVersion
      ? `<span class="mono">${esc(r.softwareVersion)}</span>`
      : `<span style="color: var(--text-faint)">— <span style="font-size: 0.78rem">RPC closed</span></span>`}</td>
    <td>${r.haveHandshake ? `<span class="mono">${num(r.height)}</span>` : dash}</td>
    <td>${r.haveHandshake
      ? `<span class="mono" style="color: ${offTip ? 'var(--yellow)' : 'var(--green)'}">${esc(shortHash(r.topId))}</span>`
      : dash}</td>
    <td>${capabilityBadge(peer)}</td>
    <td style="color: var(--text-muted)">${peer.reach === 'open' ? `${num(peer.rttMs)} ms` : '—'}</td>
    <td>${uptimeBar(peer)}</td>
    <td style="color: var(--text-muted)">${esc(ago(peer.lastSeen))}</td>
    <td style="color: var(--text-muted)">${esc(peer.location.country || '—')}</td>
  </tr>`;
}

function renderPeerFilters(total) {
  el('peerFilters').innerHTML = PEER_FILTERS.map((f) => `
    <button type="button" class="chip${peersState.filter === f.key ? ' active' : ''}"
      data-filter="${f.key}">${esc(f.label)}</button>`).join('') +
    (peersState.q ? `<button type="button" class="chip active" id="clearSearch">
      search: ${esc(peersState.q)} ✕</button>` : '') +
    `<span class="status-right">${num(total)} matching</span>`;

  el('peerFilters').querySelectorAll('[data-filter]').forEach((button) => {
    button.addEventListener('click', () => {
      peersState.filter = button.getAttribute('data-filter');
      peersState.offset = 0;
      loadPeers();
    });
  });

  const clear = el('clearSearch');
  if (clear) {
    clear.addEventListener('click', () => {
      peersState.q = '';
      el('searchInput').value = '';
      peersState.offset = 0;
      loadPeers();
    });
  }
}

async function loadPeers() {
  const query = `/peers?filter=${encodeURIComponent(peersState.filter)}` +
    `&offset=${peersState.offset}&limit=${peersState.limit}` +
    (peersState.q ? `&q=${encodeURIComponent(peersState.q)}` : '');

  const data = await get(query);

  renderPeerFilters(data.total);

  el('peersNote').textContent =
    'every address ever seen in a peer list or a seed bootstrap · state from the last sweep';

  if (!data.peers.length) {
    el('peersBody').innerHTML = '<tr><td colspan="12" class="empty-cell">Nothing matches.</td></tr>';
    el('peersPager').innerHTML = '';
    return;
  }

  el('peersBody').innerHTML = data.peers.map((p) => peerRow(p, data.majorityHash)).join('');

  el('peersBody').querySelectorAll('tr[data-key]').forEach((row) => {
    row.addEventListener('click', () => {
      location.hash = `#/node/${row.getAttribute('data-key')}`;
    });
  });

  const from = data.offset + 1;
  const to = Math.min(data.total, data.offset + data.limit);

  el('peersPager').innerHTML = `
    <span class="count">Showing ${num(from)}–${num(to)} of ${num(data.total)}</span>
    <span class="controls">
      <button type="button" class="btn btn-outline btn-sm" id="pagePrev"
        ${data.offset === 0 ? 'disabled' : ''}>Previous</button>
      <button type="button" class="btn btn-outline btn-sm" id="pageNext"
        ${to >= data.total ? 'disabled' : ''}>Next</button>
    </span>`;

  el('pagePrev').addEventListener('click', () => {
    peersState.offset = Math.max(0, peersState.offset - peersState.limit);
    loadPeers();
  });
  el('pageNext').addEventListener('click', () => {
    peersState.offset += peersState.limit;
    loadPeers();
  });
}

/* ─── GEOGRAPHY ───────────────────────────────────────────────────────────── */

/* Equirectangular: x = (lon + 180) * 2.7778, y = (90 - lat) * 2.7778.
   Continent outlines are deliberately coarse - they are a backdrop for the
   bubbles, which sit at real country centroids. */
const MAP_LAND = [
  '33.3,66.7 61.1,52.8 138.9,55.6 236.1,50 277.8,47.2 327.8,83.3 347.2,119.4 305.6,133.3 275,180.6 230.6,177.8 208.3,194.4 194.4,186.1 180.6,161.1 152.8,138.9 152.8,113.9 125,88.9 83.3,83.3',
  '275,227.8 288.9,219.4 333.3,222.2 361.1,250 402.8,263.9 394.4,291.7 366.7,319.4 338.9,347.2 327.8,366.7 311.1,394.4 291.7,394.4 302.8,361.1 300,333.3 305.6,300 275,263.9',
  '472.2,150 475,130.6 500,113.9 522.2,100 527.8,88.9 550,97.2 558.3,83.3 583.3,66.7 569.4,52.8 611.1,61.1 666.7,52.8 722.2,44.4 791.7,36.1 888.9,47.2 952.8,55.6 994.4,66.7 994.4,83.3 944.4,86.1 897.2,105.6 875,127.8 852.8,147.2 838.9,163.9 805.6,191.7 791.7,222.2 777.8,233.3 761.1,202.8 747.2,188.9 722.2,213.9 700,191.7 683.3,180.6 658.3,177.8 638.9,166.7 625,144.4 600,150 583.3,150 563.9,144.4 550,138.9 536.1,144.4 522.2,127.8 508.3,133.3 494.4,147.2',
  '452.8,208.3 455.6,191.7 472.2,175 500,161.1 527.8,155.6 555.6,161.1 588.9,163.9 597.2,183.3 608.3,208.3 619.4,219.4 641.7,216.7 633.3,236.1 613.9,255.6 611.1,277.8 597.2,302.8 591.7,322.2 575,344.4 555.6,344.4 550,327.8 536.1,297.2 525,252.8 508.3,233.3 477.8,238.9 463.9,225',
  '813.9,311.1 816.7,322.2 822.2,341.7 838.9,344.4 858.3,338.9 875,347.2 891.7,355.6 908.3,355.6 916.7,344.4 925,327.8 905.6,302.8 894.4,280.6 877.8,283.3 861.1,280.6 847.2,288.9 838.9,300',
  '375,83.3 355.6,66.7 347.2,55.6 361.1,38.9 388.9,27.8 430.6,22.2 450,36.1 438.9,55.6 411.1,72.2',
  '483.3,111.1 494.4,102.8 497.2,88.9 486.1,88.9 477.8,97.2 472.2,105.6',
  '861.1,161.1 877.8,152.8 891.7,138.9 902.8,127.8 894.4,125 883.3,147.2 869.4,155.6',
  '619.4,283.3 638.9,294.4 630.6,319.4 622.2,311.1',
  '977.8,344.4 994.4,355.6 983.3,366.7 972.2,377.8 961.1,377.8 972.2,363.9'
];

/* Country centroids, lat/lon. Only the codes seen on this network need to be
   here; anything else still counts in the side panel, it just has no bubble. */
const CENTROIDS = {
  US: [39.5, -98.5], CA: [56.0, -106.0], MX: [23.6, -102.5], BR: [-10.0, -53.0],
  AR: [-34.0, -64.0], CL: [-33.5, -70.7], GB: [54.0, -2.5], IE: [53.4, -8.2],
  FR: [46.6, 2.3], DE: [51.2, 10.4], NL: [52.2, 5.3], BE: [50.6, 4.6],
  LU: [49.8, 6.1], CH: [46.8, 8.2], AT: [47.5, 14.6], IT: [42.8, 12.6],
  ES: [40.0, -3.7], PT: [39.5, -8.0], DK: [56.0, 10.0], NO: [61.0, 8.5],
  SE: [62.0, 15.0], FI: [64.0, 26.0], EE: [58.6, 25.0], LV: [56.9, 24.6],
  LT: [55.2, 23.9], PL: [52.0, 19.1], CZ: [49.8, 15.5], SK: [48.7, 19.7],
  HU: [47.2, 19.5], RO: [45.9, 25.0], BG: [42.7, 25.5], GR: [39.1, 21.8],
  TR: [39.0, 35.2], UA: [48.4, 31.2], RU: [61.0, 90.0], BY: [53.7, 27.9],
  ZA: [-29.0, 24.7], NG: [9.1, 8.7], KE: [-0.02, 37.9], EG: [26.8, 30.8],
  IN: [21.0, 78.0], CN: [35.9, 104.2], JP: [36.2, 138.3], KR: [36.5, 127.8],
  TW: [23.7, 121.0], HK: [22.3, 114.2], SG: [1.35, 103.8], MY: [4.2, 101.9],
  TH: [15.9, 101.0], VN: [14.1, 108.3], ID: [-2.5, 118.0], PH: [12.9, 121.8],
  AU: [-25.0, 134.0], NZ: [-41.0, 174.0], IL: [31.0, 34.9], AE: [23.4, 53.8],
  SA: [23.9, 45.1], IR: [32.4, 53.7], KZ: [48.0, 66.9], PK: [30.4, 69.3],
  BD: [23.7, 90.4], MD: [47.4, 28.4], RS: [44.0, 21.0], HR: [45.1, 15.2],
  SI: [46.2, 15.0], IS: [65.0, -18.6], MT: [35.9, 14.4], CY: [35.1, 33.4]
};

function project(lat, lon) {
  return [(lon + 180) * 2.7778, (90 - lat) * 2.7778];
}

function bubbleRadius(count) {
  return 4.026 + 1.5935 * Math.sqrt(count);
}

function renderMap(geo) {
  const countries = geo.countries || [];
  const placed = countries.filter((c) => CENTROIDS[c.label]);

  const bubbles = placed
    .slice()
    .sort((a, b) => b.count - a.count)
    .map((c) => {
      const [x, y] = project(CENTROIDS[c.label][0], CENTROIDS[c.label][1]);
      const r = bubbleRadius(c.count);
      return { x, y, r, label: c.label, count: c.count };
    });

  const labels = bubbles.slice(0, 4).map((b) =>
    `<text class="map-lab" x="${(b.x + b.r + 4).toFixed(1)}" y="${(b.y + 3).toFixed(1)}">${esc(b.label)} ${b.count}</text>`);

  const grat = [250, 184.9, 315.1].map((y) =>
    `<line class="map-grat" x1="0" y1="${y}" x2="1000" y2="${y}"/>`).concat(
    [166.7, 333.3, 500, 666.7, 833.3].map((x) =>
      `<line class="map-grat" x1="${x}" y1="15" x2="${x}" y2="400"/>`));

  const located = countries.reduce((sum, c) => sum + c.count, 0);

  el('mapPanel').innerHTML = `
    <svg class="map-svg" viewBox="0 15 1000 385" role="img"
      aria-label="World map with reachable nodes per country">
      ${grat.join('')}
      ${MAP_LAND.map((points) => `<polygon class="map-land" points="${points}"/>`).join('')}
      ${bubbles.map((b) => `<circle class="map-bub" cx="${b.x.toFixed(1)}" cy="${b.y.toFixed(1)}"
        r="${b.r.toFixed(2)}"><title>${esc(b.label)}: ${b.count}</title></circle>`).join('')}
      ${labels.join('')}
    </svg>
    <div class="map-legend">
      <span style="display: inline-flex; align-items: center; gap: 0.5rem">
        <svg width="78" height="38" viewBox="0 0 78 38" aria-hidden="true">
          <circle class="map-bub" cx="7" cy="19" r="${bubbleRadius(3).toFixed(2)}"/>
          <circle class="map-bub" cx="28" cy="19" r="${bubbleRadius(20).toFixed(2)}"/>
          <circle class="map-bub" cx="59" cy="19" r="${bubbleRadius(80).toFixed(2)}"/>
        </svg>
        3 · 20 · 80 nodes
      </span>
      <span>${num(countries.length)} countries located · <strong>${num(located)}</strong>
        of ${num(geo.reachable)} nodes</span>
      ${geo.unlocated ? `<span style="color: var(--text-faint)">${num(geo.unlocated)}
        nodes have no location in the loaded database</span>` : ''}
    </div>`;
}

function renderAsn(geo) {
  const networks = (geo.networks || []).slice(0, 8);

  if (!networks.length) {
    el('asnPanel').innerHTML = geo.haveAsnDb
      ? '<div class="loading">No node matched a range in the ASN database.</div>'
      : '<div class="loading">No ASN database loaded — start with <code>--asn-db</code>.</div>';
    el('asnNote').textContent = '';
    return;
  }

  const total = geo.reachable || 1;
  const ramp = ['#a5b4fc', '#818cf8', '#6b76d8', '#565fb4', '#434b91', '#39407c', '#2f3567', '#262b55'];
  const named = networks.reduce((sum, n) => sum + n.count, 0);
  const top3 = networks.slice(0, 3).reduce((sum, n) => sum + n.count, 0);

  const parts = networks.map((n, i) => ({ count: n.count, color: ramp[i] }))
    .concat([{ count: Math.max(0, total - named), color: 'var(--bg-code)' }]);

  el('asnNote').innerHTML = `<span class="badge badge-yellow">TOP 3 ASNS = ${pct(top3, total)}</span>`;

  el('asnPanel').innerHTML = stack(parts, false).replace(/class="stack"/, 'class="stack" style="height: 22px"') +
    legend(networks.map((n, i) => ({ name: n.label, count: n.count, color: ramp[i] })), total);
}

async function loadGeo() {
  const geo = await get('/geo');

  el('geoNoDb').classList.toggle('hidden', geo.haveCountryDb || geo.haveAsnDb);

  renderMap(geo);
  renderAsn(geo);

  const countries = geo.countries || [];
  const peak = countries.length ? countries[0].count : 1;

  el('countryBars').innerHTML = countries.length
    ? barRows(countries.slice(0, 12).map((c) => ({
        label: c.label, count: c.count, share: pct(c.count, geo.reachable)
      })), peak)
    : '<div class="loading">Nothing located yet.</div>';
}

/* ─── VERSIONS ────────────────────────────────────────────────────────────── */

async function loadVersions() {
  const v = await get('/versions');

  const versions = (v.versions || []).slice().sort((a, b) =>
    Number(b.label.slice(1)) - Number(a.label.slice(1)));

  el('versionDetail').innerHTML = versions.length
    ? stack(versions.map((x, i) => ({
        count: x.count, color: VERSION_COLORS[i] || 'var(--text-faint)', text: x.label
      }))) +
      legend(versions.map((x, i) => ({
        name: x.label,
        count: x.count,
        color: VERSION_COLORS[i] || 'var(--text-faint)',
        note: Number(x.label.slice(1)) === v.p2pMinimumVersion ? 'at the handshake floor' : ''
      })), v.reachable) +
      `<div class="section-note" style="margin-top: 1rem">Network is at
        <b>v${v.p2pCurrentVersion}</b>; the handshake floor is
        <b>v${v.p2pMinimumVersion}</b>. This is a protocol byte, not a software
        release — see the builds panel below.</div>`
    : '<div class="loading">No handshakes recorded yet.</div>';

  const floors = v.raiseFloor || [];

  el('floorPanel').innerHTML = floors.length
    ? `<div class="stats-grid" style="margin: 0">${floors.map((f) => {
        const tone = f.share < 0.05 ? 'good' : f.share < 0.2 ? 'warn' : 'bad';
        const verdict = f.share < 0.05 ? 'SAFE NOW'
          : f.share < 0.2 ? 'WAIT' : 'WOULD SPLIT THE NETWORK';
        return statCard(`Raise floor to v${f.version}`, num(f.cutOff),
          `nodes cut off · ${(f.share * 100).toFixed(1)}% — ${verdict}`, tone);
      }).join('')}</div>`
    : '<div class="loading">Nothing to compare yet.</div>';

  const builds = v.builds || [];

  el('buildsNote').textContent = v.softwareProbeEnabled
    ? `known for ${num(v.softwareKnown)} of ${num(v.reachable)} reachable nodes`
    : 'RPC probing is off — start with --probe-rpc';

  el('buildsPanel').innerHTML = builds.length
    ? barRows(builds.slice().sort((a, b) => b.count - a.count).map((b) => ({
        label: b.label, count: b.count, share: pct(b.count, v.softwareKnown)
      })))
    : `<div class="note" style="border: none; background: transparent; padding: 0">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
          <circle cx="12" cy="12" r="9"/><path d="M12 16v-5"/><path d="M12 8h.01"/>
        </svg>
        <div class="body"><strong>No software versions known.</strong>
        A software version only exists on the daemon's RPC port, and RPC binds to loopback by default, so
        most nodes will never answer. Run with <code>--probe-rpc</code> to ask the ones that do. Network-wide
        coverage would need a user-agent field on the handshake itself.</div></div>`;
}

/* ─── NODE DETAIL ─────────────────────────────────────────────────────────── */

function dayStrip(history, days) {
  if (!history || !history.length) {
    return '<div class="loading">No history yet.</div>';
  }

  const byDay = {};
  history.forEach((h) => { byDay[h.day] = h; });

  const today = Math.floor(Date.now() / 86400000);
  const cells = [];

  for (let i = days - 1; i >= 0; i--) {
    const day = byDay[today - i];
    let cls = 'none';
    let title = 'no data';

    if (day && day.offered) {
      const share = day.answered / day.offered;
      cls = share >= 0.98 ? 'full' : share > 0 ? 'part' : 'down';
      title = `${day.answered} of ${day.offered} sweeps`;
    }

    cells.push(`<span class="day-cell ${cls}" title="${esc(title)}"></span>`);
  }

  return `<div class="day-strip">${cells.join('')}</div>
    <div class="map-legend" style="border: none; padding-top: 0.75rem">
      <span>${days} days ago</span>
      <span style="display: inline-flex; gap: 0.85rem; margin-left: auto">
        <span><span class="legend-swatch" style="background: var(--green); display: inline-block"></span> every sweep</span>
        <span><span class="legend-swatch" style="background: var(--yellow); display: inline-block"></span> partial</span>
        <span><span class="legend-swatch" style="background: var(--red); display: inline-block"></span> down</span>
      </span>
      <span>today</span>
    </div>`;
}

async function loadNode(key) {
  el('nodeCrumb').textContent = key;

  let node;

  try {
    node = await get(`/peers/${encodeURIComponent(key)}`);
  } catch (e) {
    el('nodeDetail').innerHTML =
      `<div class="error-box">No node called <span class="mono">${esc(key)}</span> is known.</div>`;
    return;
  }

  const r = node.reported;
  const offTip = r.haveHandshake && node.majorityHash && r.topId !== node.majorityHash;
  const dash = '<span style="color: var(--text-faint)">—</span>';

  el('nodeDetail').innerHTML = `
    <div class="section-header">
      <span class="section-title mono">${esc(node.key)}</span>
      <span style="display: flex; gap: 0.5rem; flex-wrap: wrap">
        ${reachBadge(node.reach)}
        ${capabilityBadge(node)}
        ${r.haveHandshake ? `<span class="badge badge-accent">P2P v${r.p2pVersion}</span>` : ''}
        ${node.isSeed ? '<span class="badge badge-accent">SEED</span>' : ''}
      </span>
    </div>

    <div class="section two-col">
      <div class="panel">
        <div class="panel-head">
          <span class="panel-title" style="color: var(--green)">Measured here</span>
        </div>
        <div class="panel-body">
          <div class="kv"><span class="k">Reachability</span><span class="v">${reachBadge(node.reach)}</span></div>
          <div class="kv"><span class="k">Handshake RTT</span><span class="v">${node.reach === 'open' ? `${num(node.rttMs)} ms` : '—'}</span></div>
          <div class="kv"><span class="k">Sweeps answered</span><span class="v">${num(node.sweepsAnswered)} of ${num(node.sweepsOffered)}
            <span class="aside">${pct(node.sweepsAnswered, node.sweepsOffered)}</span></span></div>
          <div class="kv"><span class="k">First seen</span><span class="v">${esc(stamp(node.firstSeen))}</span></div>
          <div class="kv"><span class="k">Last seen</span><span class="v">${esc(stamp(node.lastSeen))}</span></div>
          <div class="kv"><span class="k">Last probed</span><span class="v">${esc(ago(node.lastTried))} ago</span></div>
          <div class="kv"><span class="k">Peers advertised</span><span class="v">${num(node.peersAdvertised)}</span></div>
          <div class="kv"><span class="k">Location (GeoIP)</span><span class="v">${esc(node.location.country || '—')}
            ${node.location.asn ? ` · <span class="mono">${esc(node.location.asn)}</span> ${esc(node.location.asName)}` : ''}</span></div>
        </div>
      </div>

      <div class="panel">
        <div class="panel-head">
          <span class="panel-title" style="color: var(--yellow)">Reported by the node</span>
          <span class="section-note" style="margin-left: auto">self-declared, spoofable</span>
        </div>
        <div class="panel-body">
          <div class="kv"><span class="k">Peer ID</span><span class="v mono">${r.haveHandshake ? esc(String(r.peerId)) : '—'}</span></div>
          <div class="kv"><span class="k">P2P version</span><span class="v">${r.haveHandshake ? r.p2pVersion : '—'}</span></div>
          <div class="kv"><span class="k">Software</span><span class="v">${r.softwareVersion
            ? `<span class="mono">${esc(r.softwareVersion)}</span> <span class="aside">via RPC</span>`
            : `${dash} <span class="aside">RPC closed or not probed</span>`}</span></div>
          <div class="kv"><span class="k">Height</span><span class="v mono">${r.haveHandshake ? num(r.height) : '—'}</span></div>
          <div class="kv"><span class="k">Top block</span><span class="v mono"
            style="color: ${offTip ? 'var(--yellow)' : 'var(--green)'}">${r.haveHandshake ? esc(shortHash(r.topId)) : '—'}</span></div>
          <div class="kv"><span class="k">Capability flags</span><span class="v mono">0x${(r.capabilityFlags || 0).toString(16)}
            <span class="aside">${r.pruned ? 'pruned' : ''}${r.pruned && r.lite ? ', ' : ''}${r.lite ? 'lite' : ''}${!r.pruned && !r.lite ? 'not pruned, not lite' : ''}</span></span></div>
          <div class="kv"><span class="k">Advertised port</span><span class="v">${r.advertisedPort || '—'}
            <span class="aside">${r.advertisedPort ? 'accepts inbound' : 'hides its port'}</span></span></div>
          <div class="kv"><span class="k">Clock skew</span><span class="v">${r.haveHandshake ? `${r.clockSkewSeconds > 0 ? '+' : ''}${r.clockSkewSeconds} s` : '—'}</span></div>
        </div>
      </div>
    </div>

    <div class="section">
      <div class="panel">
        <div class="panel-head"><span class="panel-title">Reachability by day</span>
          <span class="section-note" style="margin-left: auto">one cell per day</span></div>
        <div class="panel-body">${dayStrip(node.history, 30)}</div>
      </div>
    </div>

    <div class="note">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
        <circle cx="12" cy="12" r="9"/><path d="M12 16v-5"/><path d="M12 8h.01"/>
      </svg>
      <div class="body"><strong>The split above is deliberate.</strong>
        Reachability, latency and uptime are things this crawler observed. Peer ID, version, height, top
        hash and capability flags are claims the node made in its handshake — a node can say anything. The
        tip hash is the one claim the network cross-checks for itself:
        ${num(node.majorityHashNodes)} node${node.majorityHashNodes === 1 ? '' : 's'} independently
        reported the same one.</div>
    </div>`;
}

/* ─── ROUTER ──────────────────────────────────────────────────────────────── */

const PAGES = ['pageOverview', 'pagePeers', 'pageGeo', 'pageVersions', 'pageNode', 'pageLoading'];

function showPage(id) {
  PAGES.forEach((page) => el(page).classList.toggle('active', page === id));
}

function setNav(name) {
  document.querySelectorAll('[data-nav]').forEach((link) => {
    link.classList.toggle('active', link.getAttribute('data-nav') === name);
  });
}

async function route() {
  const hash = location.hash || '#/';
  clearError();

  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }

  try {
    if (hash.startsWith('#/node/')) {
      setNav('peers');
      showPage('pageNode');
      await loadNode(decodeURIComponent(hash.slice('#/node/'.length)));
      return;
    }

    if (hash.startsWith('#/peers')) {
      setNav('peers');
      showPage('pagePeers');
      await loadPeers();
      pollTimer = setInterval(() => loadPeers().catch(() => {}), POLL_MS);
      return;
    }

    if (hash.startsWith('#/geo')) {
      setNav('geo');
      showPage('pageGeo');
      await loadGeo();
      pollTimer = setInterval(() => loadGeo().catch(() => {}), POLL_MS);
      return;
    }

    if (hash.startsWith('#/versions')) {
      setNav('versions');
      showPage('pageVersions');
      await loadVersions();
      pollTimer = setInterval(() => loadVersions().catch(() => {}), POLL_MS);
      return;
    }

    setNav('overview');
    showPage('pageOverview');
    await loadOverview();
    pollTimer = setInterval(() => loadOverview().catch(() => {}), POLL_MS);
  } catch (e) {
    showPage('pageLoading');
    showError(`${e.message}. Is wrkz-netmon running, and is ${API} reachable from this page?`);
  }
}

function initSearch() {
  el('searchForm').addEventListener('submit', (event) => {
    event.preventDefault();
    const value = el('searchInput').value.trim();
    if (!value) return;

    /* An exact "address:port" is a node; anything else filters the table. */
    peersState.q = /:\d+$/.test(value) ? '' : value;
    peersState.offset = 0;
    peersState.filter = 'all';

    location.hash = /:\d+$/.test(value) ? `#/node/${value}` : '#/peers';

    if (location.hash === '#/peers') route();
  });
}

window.addEventListener('hashchange', route);

initTheme();
initSearch();
route();
