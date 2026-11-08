/**
 * wallet_worker.js
 *
 * Dedicated Web Worker that hosts the WASM wallet module.
 * All blocking WASM calls (create, open, sync XHR, etc.) run here
 * instead of the main browser thread, keeping the UI responsive.
 *
 * Communication protocol (postMessage):
 *   Main → Worker:  { id: number, type: 'call'|'async', method: string, params: object }
 *   Worker → Main:  { id: number, ok: boolean, result: any, error: string? }
 *   Worker → Main:  { type: 'event', eventType: number, eventData: object }
 */

import { WalletBridge } from './wallet_bridge.js';

let bridge = null;
let bridgeReady = false;   // true only after bridge.init() fully completes
let syncTimer = null;      // drives sync in single-threaded WASM mode
let syncRunning = false;   // guards against two chains of syncStep at once
let pthreadsEnabled = false; // true when WASM was built with -pthread

// Serialises the lifecycle operations (create / open / restore / close /
// save / delete). onmessage is async and the runtime does not wait for one
// invocation to settle before starting the next, so two lifecycle calls could
// otherwise interleave around their IndexedDB awaits: a close still writing
// its file would meet the restore that follows it, and either the restore was
// refused with "wallet already open" or close's tail ran against the wallet
// that had just replaced it - leaving the new wallet with no _currentFilename
// and therefore no autosave. Read-only 'call' messages are untouched; they run
// synchronously inside the WASM module and never await.
let lifecycleQueue = Promise.resolve();

function runExclusive(fn) {
  const result = lifecycleQueue.then(fn, fn);
  // Keep the chain alive regardless of how this operation ends.
  lifecycleQueue = result.then(() => {}, () => {});
  return result;
}

// How long to wait before looking for more blocks once the daemon has none
// left to give. While it does have blocks, the next step is queued immediately.
const SYNC_IDLE_MS = 2000;

// Each syncStep downloads one batch and processes up to a chunk of it, so a
// fixed interval puts a hard ceiling on sync speed that no amount of bandwidth
// or daemon capacity can lift — a couple of hundred blocks a second, whatever
// the machine could actually manage. Chaining the next step as soon as the last
// one reported progress lets it run as fast as the daemon will answer, and
// falls back to polling only once there is genuinely nothing left to fetch.
//
// This runs on a dedicated worker, so a busy sync loop costs the UI thread
// nothing; yielding through setTimeout still lets postMessage in between steps.
function scheduleSyncStep(delayMs) {
  if (pthreadsEnabled || !syncRunning) return;

  syncTimer = setTimeout(() => {
    syncTimer = null;

    if (!syncRunning) return;

    if (!bridgeReady || !bridge) {
      scheduleSyncStep(SYNC_IDLE_MS);
      return;
    }

    let progressed = false;

    try {
      const result = bridge.call('syncStep', {});
      progressed = result === true || (result && result.progressed === true);
    } catch (_) {
      // A failed step is usually the daemon being unreachable. Back off rather
      // than spinning on it.
    }

    scheduleSyncStep(progressed ? 0 : syncIdleMs());
  }, delayMs);
}

// ── Daemon event stream ─────────────────────────────────────────────
//
// A daemon run with --enable-websocket announces every new block on GET /ws.
// Following it lets a synced wallet step the moment a block lands instead of
// on its next idle poll, and poll far less often in between. The stream only
// ever wakes the sync loop: balances and history still come from the ordinary
// sync, so a lost message costs at most one idle interval, and a daemon that
// does not serve /ws leaves everything exactly as it was.
//
// Only the single-threaded build follows it here. In a pthreads build the
// wallet's own threads poll the daemon and nothing in this worker drives them.

// While the stream is live, the idle poll only has to catch what it missed.
const LIVE_SYNCED_POLL_MS = 30000;
// No frame for this long (the daemon sends a heartbeat every 30 s) and the
// stream is taken to be gone, even if the socket has not noticed.
const LIVE_WINDOW_MS = 75000;
// A daemon that never delivered a frame most likely does not serve /ws at all.
const UNSUPPORTED_RETRY_MS = 600000;
const RECONNECT_MIN_MS = 1000;
const RECONNECT_MAX_MS = 60000;

// "hello" wakes too: whatever happened while the stream was down is only
// caught up by syncing.
const WAKE_TOPICS = new Set([
  'hello', 'hashblock', 'chain_main', 'chainswitch', 'txpool_add', 'txpool_del',
]);

let tipSocket = null;       // the open (or opening) WebSocket, if any
let tipUrl = null;          // the stream being followed; null when none
let tipRetryTimer = null;
let tipWatchdog = null;     // drops a socket that has gone quiet
let tipBackoffMs = RECONNECT_MIN_MS;
let tipLastFrameAt = 0;     // Date.now() of the last frame; 0 when not live
let tipDelivered = false;   // whether tipUrl has ever delivered a frame

function syncIdleMs() {
  const live = tipLastFrameAt !== 0 && Date.now() - tipLastFrameAt < LIVE_WINDOW_MS;
  return live ? LIVE_SYNCED_POLL_MS : SYNC_IDLE_MS;
}

// Mirrors the URL Nigel builds for its requests (daemonBaseUrl + the https
// upgrade in wrkzSyncXhr), so the stream comes from exactly where sync does.
function daemonEventStreamUrl(host, port, ssl) {
  if (!host || host.startsWith('/') || host.startsWith('@') || host.startsWith('ipc://')) {
    return null;
  }

  let hostname = host;
  let basePath = '';
  const slash = host.indexOf('/');
  if (slash !== -1) {
    hostname = host.substring(0, slash);
    basePath = host.substring(slash).replace(/\/+$/, '');
  }

  if (hostname.includes(':') && !hostname.startsWith('[')) {
    hostname = `[${hostname}]`;
  }

  const defaultPort = ssl ? 443 : 80;
  const portPart = !port || Number(port) === defaultPort ? '' : `:${port}`;

  // The page being https means mixed content rules, and the sync requests are
  // upgraded for the same reason.
  const secure = ssl || (typeof self !== 'undefined' && self.location && self.location.protocol === 'https:');

  return `${secure ? 'wss' : 'ws'}://${hostname}${portPart}${basePath}/ws`;
}

// Runs the next sync step now rather than when the idle timer says. Only
// replaces a pending timer, so there is never more than one chain of steps.
function wakeSync() {
  if (pthreadsEnabled || !syncRunning || !syncTimer) return;

  clearTimeout(syncTimer);
  syncTimer = null;
  scheduleSyncStep(0);
}

function followDaemon(host, port, ssl) {
  if (pthreadsEnabled) return;

  const url = daemonEventStreamUrl(host, port, ssl === true);
  if (url === tipUrl) return;

  unfollowDaemon();

  if (!url || typeof WebSocket === 'undefined') return;

  tipUrl = url;
  tipDelivered = false;
  tipBackoffMs = RECONNECT_MIN_MS;
  connectTipStream();
}

function unfollowDaemon() {
  tipUrl = null;
  tipLastFrameAt = 0;
  if (tipRetryTimer) { clearTimeout(tipRetryTimer); tipRetryTimer = null; }
  if (tipWatchdog) { clearTimeout(tipWatchdog); tipWatchdog = null; }
  if (tipSocket) {
    const socket = tipSocket;
    tipSocket = null;
    socket.onopen = socket.onmessage = socket.onerror = socket.onclose = null;
    try { socket.close(); } catch (_) {}
  }
}

function connectTipStream() {
  tipRetryTimer = null;
  if (!tipUrl) return;

  let socket;
  try {
    socket = new WebSocket(tipUrl);
  } catch (_) {
    scheduleTipReconnect();
    return;
  }
  tipSocket = socket;

  socket.onmessage = (ev) => {
    if (tipSocket !== socket) return;

    // Every frame counts towards liveness, heartbeats included.
    tipLastFrameAt = Date.now();
    tipBackoffMs = RECONNECT_MIN_MS;
    if (!tipDelivered) {
      tipDelivered = true;
      console.debug(`[wallet_worker] following the daemon's event stream at ${tipUrl}`);
    }

    if (tipWatchdog) clearTimeout(tipWatchdog);
    tipWatchdog = setTimeout(() => {
      tipWatchdog = null;
      if (tipSocket === socket) {
        try { socket.close(); } catch (_) {}
      }
    }, LIVE_WINDOW_MS);

    let topic = null;
    try {
      const message = JSON.parse(ev.data);
      topic = message && typeof message.topic === 'string' ? message.topic : null;
    } catch (_) {
      return;
    }

    if (WAKE_TOPICS.has(topic)) wakeSync();
  };

  // A failed connect reports an error and then a close; handle it once.
  socket.onerror = () => {};

  socket.onclose = () => {
    if (tipSocket !== socket) return;
    tipSocket = null;
    tipLastFrameAt = 0;
    if (tipWatchdog) { clearTimeout(tipWatchdog); tipWatchdog = null; }
    scheduleTipReconnect();
  };
}

function scheduleTipReconnect() {
  if (!tipUrl) return;

  let delay;
  if (!tipDelivered) {
    // A browser reports a refused connection and a 404 to the upgrade the
    // same way, and a daemon that has never answered with a frame is most
    // likely one without the stream. Leave it alone for a while.
    delay = UNSUPPORTED_RETRY_MS;
    console.debug(`[wallet_worker] no event stream at ${tipUrl}; polling instead`);
  } else {
    delay = tipBackoffMs;
    tipBackoffMs = Math.min(tipBackoffMs * 2, RECONNECT_MAX_MS);
  }

  tipRetryTimer = setTimeout(connectTipStream, delay);
}

// The daemon a lifecycle call or swapNode just pointed the wallet at.
function followDaemonFrom(params) {
  const p = params || {};
  followDaemon(p.daemonHost, p.daemonPort, p.daemonSsl);
}

function startSyncTimer() {
  // In pthread builds the WASM background threads drive sync — no JS timer needed.
  if (pthreadsEnabled) return;
  if (syncRunning) return;

  syncRunning = true;
  scheduleSyncStep(0);
}

function stopSyncTimer() {
  syncRunning = false;
  if (syncTimer) { clearTimeout(syncTimer); syncTimer = null; }
}

/**
 * Handle messages from the main thread.
 */
self.onmessage = async (e) => {
  const msg = e.data;

  // ── Init ────────────────────────────────────────────────────────────
  if (msg.type === 'init') {
    try {
      bridge = new WalletBridge();
      await bridge.init(msg.wasmPath || './wallet_wasm.js');
      bridgeReady = true;
      // Detect build mode: pthreads builds drive sync internally via WASM threads.
      try { pthreadsEnabled = bridge.call('isPthreadsEnabled', {}) === true; } catch (_) {}
      self.postMessage({ id: msg.id, ok: true, result: 'initialized', pthreadsEnabled });
    } catch (err) {
      bridge = null;
      self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
    }
    return;
  }

  if (!bridgeReady) {
    self.postMessage({ id: msg.id, ok: false, error: 'WASM wallet not ready — wait for walletBridgeReady event' });
    return;
  }

  // ── Async methods (IndexedDB involved) ──────────────────────────────
  if (msg.type === 'async') {
    await runExclusive(async () => {
      try {
        let result;
        switch (msg.method) {
          case 'create':
            result = await bridge.create(msg.params);
            startSyncTimer();
            followDaemonFrom(msg.params);
            break;
          case 'open':
            result = await bridge.open(msg.params);
            startSyncTimer();
            followDaemonFrom(msg.params);
            break;
          case 'restoreFromSeed':
            result = await bridge.restoreFromSeed(msg.params);
            startSyncTimer();
            followDaemonFrom(msg.params);
            break;
          case 'restoreFromKeys':
            result = await bridge.restoreFromKeys(msg.params);
            startSyncTimer();
            followDaemonFrom(msg.params);
            break;
          case 'restoreViewWallet':
            result = await bridge.restoreViewWallet(msg.params);
            startSyncTimer();
            followDaemonFrom(msg.params);
            break;
          case 'close':
            stopSyncTimer();
            unfollowDaemon();
            result = await bridge.close();
            break;
          case 'save':
            result = await bridge.save();
            break;
          case 'deleteFile':
            result = await bridge.deleteFile(msg.params.filename);
            break;
          case 'listWallets':
            result = await bridge.listWallets();
            break;
          default:
            throw new Error(`Unknown async method: ${msg.method}`);
        }
        self.postMessage({ id: msg.id, ok: true, result });
      } catch (err) {
        self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
      }
    });
    return;
  }

  // ── Sync methods (pure WASM, no IndexedDB) ──────────────────────────
  if (msg.type === 'call') {
    try {
      const result = bridge.call(msg.method, msg.params || {});
      if (msg.method === 'swapNode') followDaemonFrom(msg.params);
      self.postMessage({ id: msg.id, ok: true, result });
    } catch (err) {
      self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
    }
    return;
  }

  // ── Start event polling ─────────────────────────────────────────────
  if (msg.type === 'startEvents') {
    bridge.startEventPolling((eventType, eventData) => {
      self.postMessage({ type: 'event', eventType, eventData });
    }, msg.intervalMs || 1000);
    self.postMessage({ id: msg.id, ok: true, result: 'polling' });
    return;
  }

  // ── Stop event polling ──────────────────────────────────────────────
  if (msg.type === 'stopEvents') {
    bridge.stopEventPolling();
    self.postMessage({ id: msg.id, ok: true, result: 'stopped' });
    return;
  }

  self.postMessage({ id: msg.id, ok: false, error: `Unknown message type: ${msg.type}` });
};
