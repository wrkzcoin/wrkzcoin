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

    scheduleSyncStep(progressed ? 0 : SYNC_IDLE_MS);
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
    try {
      let result;
      switch (msg.method) {
        case 'create':
          result = await bridge.create(msg.params);
          startSyncTimer();
          break;
        case 'open':
          result = await bridge.open(msg.params);
          startSyncTimer();
          break;
        case 'restoreFromSeed':
          result = await bridge.restoreFromSeed(msg.params);
          startSyncTimer();
          break;
        case 'restoreFromKeys':
          result = await bridge.restoreFromKeys(msg.params);
          startSyncTimer();
          break;
        case 'restoreViewWallet':
          result = await bridge.restoreViewWallet(msg.params);
          startSyncTimer();
          break;
        case 'close':
          stopSyncTimer();
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
