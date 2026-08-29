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
 *
 * Note: there is no DOM in here. Anything that needs `document` (triggering a
 * file download) returns raw data and is finished on the main thread.
 */

import { WalletBridge } from './wallet_bridge.js';

let bridge = null;
let bridgeReady = false;   // true only after bridge.init() fully completes
let syncTimer = null;      // drives sync in single-threaded WASM mode
let syncRunning = false;   // guards against two chains of syncStep at once
let pthreadsEnabled = false; // true when WASM was built with -pthread

// How long to wait before looking for more blocks once the daemon has none left
// to give. While it does have blocks, the next step is queued immediately.
const SYNC_STEP_INTERVAL_MS = 2000;

// Each syncStep downloads one batch and processes up to a chunk of it, so a
// fixed interval puts a hard ceiling on sync speed that no amount of bandwidth
// or daemon capacity can lift — a couple of hundred blocks a second, whatever
// the machine could actually manage. Chaining the next step as soon as the last
// one reported progress lets it run as fast as the daemon will answer, and falls
// back to polling only once there is genuinely nothing left to fetch.
//
// This runs on a dedicated worker, so a busy sync loop costs the UI thread
// nothing; yielding through setTimeout still lets postMessage in between steps.
function scheduleSyncStep(delayMs) {
  if (pthreadsEnabled || !syncRunning) return;

  syncTimer = setTimeout(() => {
    syncTimer = null;

    if (!syncRunning) return;

    if (!bridgeReady || !bridge) {
      scheduleSyncStep(SYNC_STEP_INTERVAL_MS);
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

    scheduleSyncStep(progressed ? 0 : SYNC_STEP_INTERVAL_MS);
  }, delayMs);
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
 * Async methods handled here. Each entry receives the request params and
 * returns the value posted back to the main thread.
 */
const ASYNC_METHODS = {
  // --- lifecycle ---
  create: async (p) => { const r = await bridge.create(p); startSyncTimer(); return r; },
  open: async (p) => { const r = await bridge.open(p); startSyncTimer(); return r; },
  restoreFromSeed: async (p) => { const r = await bridge.restoreFromSeed(p); startSyncTimer(); return r; },
  restoreFromKeys: async (p) => { const r = await bridge.restoreFromKeys(p); startSyncTimer(); return r; },
  restoreViewWallet: async (p) => { const r = await bridge.restoreViewWallet(p); startSyncTimer(); return r; },
  close: async () => { stopSyncTimer(); return bridge.close(); },
  save: async () => bridge.save(),

  // --- files ---
  deleteFile: async (p) => bridge.deleteFile(p.filename),
  listWallets: async () => bridge.listWallets(),
  // No DOM here — hand the encoded bytes back and let the main thread save it.
  exportWalletData: async (p) => {
    await bridge.save();
    const name = p.filename || bridge._currentFilename;
    if (!name) throw new Error('No wallet filename specified');
    return { filename: name, dataBase64: await bridge.storage.loadFile(name) };
  },
  importWalletFile: async (p) => bridge.importWalletFile(p.filename, p.file),
  storageEstimate: async () => bridge.storageEstimate(),

  // --- spending (must persist after mutating wallet state) ---
  sendBasic: async (p) => bridge.sendBasic(p.destination, p.amount, p),
  sendPrepared: async (p) => bridge.sendPrepared(p.preparedTxHash),
  sendAdvancedJson: async (p) => bridge.sendAdvancedJson(p.requestJson, p.broadcast),
  sweepToAddress: async (p) => bridge.sweepToAddress(p.destination, p),

  // --- password / subwallets (re-encrypt or mutate the key set) ---
  changePassword: async (p) => bridge.changePassword(p.newPassword),
  addSubwallet: async () => bridge.addSubwallet(),
  importSubwalletFromKey: async (p) => bridge.importSubwalletFromKey(p.privateSpendKey, p.scanHeight),
  importSubwalletFromIndex: async (p) => bridge.importSubwalletFromIndex(p.walletIndex, p.scanHeight),
  deleteSubwallet: async (p) => bridge.deleteSubwallet(p.address),
};

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
      bridgeReady = false;
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
    const handler = ASYNC_METHODS[msg.method];
    if (!handler) {
      self.postMessage({ id: msg.id, ok: false, error: `Unknown async method: ${msg.method}` });
      return;
    }
    try {
      const result = await handler(msg.params || {});
      self.postMessage({ id: msg.id, ok: true, result });
    } catch (err) {
      self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
    }
    return;
  }

  // ── Sync methods (pure WASM, no IndexedDB) ──────────────────────────
  if (msg.type === 'call') {
    try {
      const result = bridge.call(msg.method, msg.params || {});
      self.postMessage({ id: msg.id, ok: true, result });
    } catch (err) {
      self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
    }
    return;
  }

  // ── Start event polling ─────────────────────────────────────────────
  if (msg.type === 'startEvents') {
    try {
      bridge.startEventPolling((eventType, eventData) => {
        self.postMessage({ type: 'event', eventType, eventData });
      }, msg.intervalMs || 1000);
      self.postMessage({ id: msg.id, ok: true, result: 'polling' });
    } catch (err) {
      self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
    }
    return;
  }

  // ── Stop event polling ──────────────────────────────────────────────
  if (msg.type === 'stopEvents') {
    try {
      bridge.stopEventPolling();
      self.postMessage({ id: msg.id, ok: true, result: 'stopped' });
    } catch (err) {
      self.postMessage({ id: msg.id, ok: false, error: err.message || String(err) });
    }
    return;
  }

  self.postMessage({ id: msg.id, ok: false, error: `Unknown message type: ${msg.type}` });
};
