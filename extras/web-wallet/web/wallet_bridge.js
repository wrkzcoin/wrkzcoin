/**
 * wallet_bridge.js
 *
 * Async JavaScript wrapper around the WRKZ WASM wallet module.
 * Provides a clean async API for Flutter web (via dart:js_interop) or
 * any JavaScript consumer.
 *
 * Usage:
 *   import { WalletBridge } from './wallet_bridge.js';
 *
 *   const wallet = new WalletBridge();
 *   await wallet.init('/wasm/wallet_wasm.js');
 *   const result = await wallet.create({
 *     filename: 'my_wallet', password: 'secret',
 *     daemonHost: 'node.example.com', daemonPort: 17855
 *   });
 *
 * All methods return Promises that resolve to the parsed JSON result
 * from the WASM dispatcher, or throw on error.
 */

import { WalletStorage, base64ToBytes } from './wallet_storage.js';

/**
 * Methods whose response contains key material. Their results are NEVER
 * logged, not even truncated — a seed phrase in the browser console outlives
 * the session, is readable by any extension with host access, and shows up in
 * screen shares and bug-report screenshots.
 */
const SENSITIVE_METHODS = new Set([
  'getMnemonicSeed',
  'getMnemonicSeedForAddress',
  'getSpendKeysJson',
  'getPrivateViewKey',
  'getTxPrivateKey',
  'exportJson',
  'exportFileData',
  'importFileData',
  'changePassword',
  'create',
  'open',
  'restoreFromSeed',
  'restoreFromKeys',
  'restoreViewWallet',
  'importSubwalletFromKey',
]);

/**
 * Verbose request/response logging. Off by default — turn it on from the
 * console with `window.walletBridgeDebug = true` when diagnosing an issue.
 * Sensitive methods stay redacted even when this is on.
 */
function debugEnabled() {
  return typeof globalThis !== 'undefined' && globalThis.walletBridgeDebug === true;
}

/** Default timeout for a single worker round-trip. */
const DEFAULT_CALL_TIMEOUT_MS = 120_000;

/**
 * Long-running operations that legitimately block the worker for minutes
 * (initial sync scan, transaction PoW, full rescan) get no timeout.
 */
const UNBOUNDED_METHODS = new Set([
  'create', 'open', 'restoreFromSeed', 'restoreFromKeys', 'restoreViewWallet',
  'sendBasic', 'sendPrepared', 'sendAdvancedJson', 'sweepToAddress',
  'estimateSweep', 'reset', 'save', 'close', 'syncStep',
]);

export class WalletBridge {
  constructor() {
    this._module = null;
    this._request = null; // cwrap'd _wallet_wasm_request
    this._storage = new WalletStorage();
    this._eventTimer = null;
    this._eventCallback = null;
    this._currentFilename = null;
    this._autosaveEnabled = true;
  }

  /**
   * Initialize the WASM module and IndexedDB storage.
   * @param {string} wasmModulePath - Path to wallet_wasm.js (ES6 module)
   */
  async init(wasmModulePath = './wallet_wasm.js') {
    // Dynamically import the Emscripten ES6 module
    const factory = (await import(wasmModulePath)).default;
    this._module = await factory();

    if (typeof this._module._wallet_wasm_request !== 'function') {
      throw new Error(
        'wallet_wasm.js loaded but _wallet_wasm_request is not exported — ' +
        'the WASM build is missing EXPORTED_FUNCTIONS=_wallet_wasm_request');
    }

    // Create the cwrap'd request function
    this._request = this._module.cwrap(
      'wallet_wasm_request',
      'number', // returns char* (pointer)
      ['string'] // takes const char* (JSON string)
    );

    // Initialize IndexedDB
    await this._storage.init();
  }

  /**
   * Send a raw JSON-RPC style request to the WASM module.
   * @param {string} method
   * @param {object} params
   * @returns {*} The result field from the response
   * @throws {Error} If the response has ok=false
   */
  call(method, params = {}) {
    if (!this._request) {
      throw new Error('WalletBridge not initialized — call init() first');
    }

    const requestJson = JSON.stringify({ method, params });
    const resultPtr = this._request(requestJson);

    if (!resultPtr) {
      throw new Error(`[WalletBridge] WASM returned null for method: ${method} — malloc failure or WASM crash`);
    }

    let resultStr;
    try {
      resultStr = this._module.UTF8ToString(resultPtr);
    } finally {
      // Free the WASM-side allocation even if decoding threw, or we leak it.
      this._module._free(resultPtr);
    }

    if (debugEnabled()) {
      if (SENSITIVE_METHODS.has(method)) {
        console.log(`[WalletBridge] ${method} → <redacted>`);
      } else {
        console.log(`[WalletBridge] ${method} →`,
          resultStr.length > 300 ? resultStr.substring(0, 300) + '…' : resultStr);
      }
    }

    const response = JSON.parse(resultStr);
    if (!response.ok) {
      const err = new Error(response.errorMessage || `Error ${response.error}`);
      err.code = response.error;
      throw err;
    }
    return response.result;
  }

  // ====== Lifecycle ======

  /**
   * Create a new wallet.
   * Also persists the (initially empty) wallet to IndexedDB after creation.
   */
  async create(opts) {
    const result = this.call('create', {
      filename: opts.filename || 'wallet',
      password: opts.password || '',
      daemonHost: opts.daemonHost || '',
      daemonPort: opts.daemonPort || 0,
      daemonSsl: opts.daemonSsl || false,
      syncThreads: opts.syncThreads || 0,
    });
    this._currentFilename = opts.filename || 'wallet';
    // Save initial wallet state to WASM store, then persist to IndexedDB
    await this._persistToIndexedDB();
    return result;
  }

  /**
   * Open an existing wallet.
   * First loads the wallet file from IndexedDB into the WASM in-memory store,
   * then calls the WASM open method.
   */
  async open(opts) {
    const filename = opts.filename || 'wallet';

    // Load from IndexedDB into WASM's in-memory store
    const b64 = await this._storage.loadFile(filename);
    if (b64) {
      this.call('importFileData', { filename, dataBase64: b64 });
    }

    const result = this.call('open', {
      filename,
      password: opts.password || '',
      daemonHost: opts.daemonHost || '',
      daemonPort: opts.daemonPort || 0,
      daemonSsl: opts.daemonSsl || false,
      syncThreads: opts.syncThreads || 0,
    });
    this._currentFilename = filename;
    return result;
  }

  /**
   * Restore wallet from mnemonic seed.
   */
  async restoreFromSeed(opts) {
    const result = this.call('restoreFromSeed', {
      mnemonicSeed: opts.mnemonicSeed,
      filename: opts.filename || 'wallet',
      password: opts.password || '',
      scanHeight: opts.scanHeight || 0,
      daemonHost: opts.daemonHost || '',
      daemonPort: opts.daemonPort || 0,
      daemonSsl: opts.daemonSsl || false,
      syncThreads: opts.syncThreads || 0,
    });
    this._currentFilename = opts.filename || 'wallet';
    await this._persistToIndexedDB();
    return result;
  }

  /**
   * Restore wallet from private keys.
   */
  async restoreFromKeys(opts) {
    const result = this.call('restoreFromKeys', {
      privateSpendKey: opts.privateSpendKey,
      privateViewKey: opts.privateViewKey,
      filename: opts.filename || 'wallet',
      password: opts.password || '',
      scanHeight: opts.scanHeight || 0,
      daemonHost: opts.daemonHost || '',
      daemonPort: opts.daemonPort || 0,
      daemonSsl: opts.daemonSsl || false,
      syncThreads: opts.syncThreads || 0,
    });
    this._currentFilename = opts.filename || 'wallet';
    await this._persistToIndexedDB();
    return result;
  }

  /**
   * Restore view-only wallet.
   */
  async restoreViewWallet(opts) {
    const result = this.call('restoreViewWallet', {
      privateViewKey: opts.privateViewKey,
      address: opts.address,
      filename: opts.filename || 'wallet',
      password: opts.password || '',
      scanHeight: opts.scanHeight || 0,
      daemonHost: opts.daemonHost || '',
      daemonPort: opts.daemonPort || 0,
      daemonSsl: opts.daemonSsl || false,
      syncThreads: opts.syncThreads || 0,
    });
    this._currentFilename = opts.filename || 'wallet';
    await this._persistToIndexedDB();
    return result;
  }

  /**
   * Close the current wallet. Persists to IndexedDB before closing.
   *
   * A failure to save must not strand the wallet open — we report the save
   * error but still close, otherwise a full disk leaves the user unable to
   * log out.
   */
  async close() {
    let saveError = null;
    try {
      await this.save();
    } catch (err) {
      saveError = err;
    }
    this.stopEventPolling();
    const result = this.call('close');
    this._currentFilename = null;
    if (saveError) throw saveError;
    return result;
  }

  /**
   * Save wallet and persist to IndexedDB (browser storage).
   */
  async save() {
    this.call('save');
    await this._persistToIndexedDB();
  }

  /**
   * Delete a wallet file from both WASM store and IndexedDB.
   * @param {string} filename
   */
  async deleteFile(filename) {
    const name = normaliseFilename(filename);
    try { this.call('deleteFile', { filename: name }); } catch (_) { /* may not exist in WASM store */ }
    await this._storage.deleteFile(name);
    if (this._currentFilename === name) this._currentFilename = null;
  }

  // ====== Sync & Node ======

  getSyncStatus() {
    return this.call('getSyncStatus');
  }

  getStatusJson() {
    return this.call('getStatusJson');
  }

  isDaemonOnline() {
    return this.call('isDaemonOnline');
  }

  getNodeInfoJson() {
    return this.call('getNodeInfoJson');
  }

  swapNode(daemonHost, daemonPort, daemonSsl = false) {
    return this.call('swapNode', { daemonHost, daemonPort, daemonSsl });
  }

  reset(scanHeight = 0, timestamp = 0) {
    return this.call('reset', { scanHeight, timestamp });
  }

  // ====== Balance ======

  getTotalBalance() {
    return this.call('getTotalBalance');
  }

  getBalanceForAddress(address) {
    return this.call('getBalanceForAddress', { address });
  }

  getBalancesJson() {
    return this.call('getBalancesJson');
  }

  // ====== Addresses ======

  getPrimaryAddress() {
    return this.call('getPrimaryAddress');
  }

  getAddressesJson() {
    return this.call('getAddressesJson');
  }

  // ====== Transactions ======

  getTransactionsJson(startHeight = 0, endHeight = 0, includeUnconfirmed = true) {
    return this.call('getTransactionsJson', { startHeight, endHeight, includeUnconfirmed });
  }

  getTransactionsStatusJson(requestJson) {
    return this.call('getTransactionsStatusJson', { requestJson });
  }

  getTxPrivateKey(txHash) {
    return this.call('getTxPrivateKey', { txHash });
  }

  // ====== Send / Sweep ======

  /**
   * Spending changes wallet state (inputs consumed, outputs added). Persist
   * immediately rather than waiting for the next autosave tick — a tab closed
   * right after a send would otherwise resync into a stale UTXO set.
   */
  async sendBasic(destination, amount, opts = {}) {
    const result = this.call('sendBasic', normaliseSendBasic(destination, amount, opts));
    await this._persistToIndexedDB();
    return result;
  }

  async sendPrepared(preparedTxHash) {
    const result = this.call('sendPrepared', { preparedTxHash });
    await this._persistToIndexedDB();
    return result;
  }

  async sendAdvancedJson(requestJson, broadcast = true) {
    const result = this.call('sendAdvancedJson', { requestJson, broadcast });
    if (broadcast) await this._persistToIndexedDB();
    return result;
  }

  async sweepToAddress(destination, opts = {}) {
    const result = this.call('sweepToAddress', normaliseSweep(destination, opts));
    await this._persistToIndexedDB();
    return result;
  }

  estimateSweep(amountToSweep = 0) {
    return this.call('estimateSweep', { amountToSweep });
  }

  // ====== Keys / Seeds ======

  getPrivateViewKey() {
    return this.call('getPrivateViewKey');
  }

  getSpendKeysJson(address) {
    return this.call('getSpendKeysJson', { address });
  }

  getMnemonicSeed() {
    return this.call('getMnemonicSeed');
  }

  getMnemonicSeedForAddress(address) {
    return this.call('getMnemonicSeedForAddress', { address });
  }

  isViewWallet() {
    return this.call('isViewWallet');
  }

  // ====== Password ======

  /**
   * Re-encrypt the wallet under a new password.
   *
   * This MUST persist: the WASM store now holds a file encrypted with the new
   * password, and leaving IndexedDB on the old ciphertext means the new
   * password silently fails to open the wallet on the next page load.
   */
  async changePassword(newPassword) {
    const result = this.call('changePassword', { newPassword });
    this.call('save');
    await this._persistToIndexedDB();
    return result;
  }

  // ====== Export ======

  exportJson() {
    return this.call('exportJson');
  }

  // ====== Subwallets ======

  async addSubwallet() {
    const result = this.call('addSubwallet');
    await this._persistToIndexedDB();
    return result;
  }

  async importSubwalletFromKey(privateSpendKey, scanHeight = 0) {
    const result = this.call('importSubwalletFromKey', { privateSpendKey, scanHeight });
    await this._persistToIndexedDB();
    return result;
  }

  async importSubwalletFromIndex(walletIndex, scanHeight = 0) {
    const result = this.call('importSubwalletFromIndex', { walletIndex, scanHeight });
    await this._persistToIndexedDB();
    return result;
  }

  async deleteSubwallet(address) {
    const result = this.call('deleteSubwallet', { address });
    await this._persistToIndexedDB();
    return result;
  }

  // ====== Integrated Address ======

  createIntegratedAddress(address, paymentId) {
    return this.call('createIntegratedAddress', { address, paymentId });
  }

  // ====== Events ======

  /**
   * Poll for a single event from the wallet.
   * @param {number} timeoutMs - Max wait time in milliseconds
   * @returns {{ eventType: number, eventData: object|null }}
   */
  pollEvent(timeoutMs = 100) {
    return this.call('pollEvent', { timeoutMs });
  }

  /**
   * Start automatic event polling. Calls the callback whenever
   * a SYNCED or TRANSACTION event arrives.
   * @param {function} callback - (eventType, eventData) => void
   * @param {number} intervalMs - Polling interval (default 1000ms)
   */
  startEventPolling(callback, intervalMs = 1000) {
    this.stopEventPolling();
    this._eventCallback = callback;
    this._eventTimer = setInterval(() => {
      try {
        // Drain the queue — several events can accumulate between ticks and
        // dropping all but one loses transaction notifications.
        for (let i = 0; i < 16; i++) {
          const ev = this.pollEvent(0);
          if (!ev || ev.eventType === 0) break;
          if (this._eventCallback) this._eventCallback(ev.eventType, ev.eventData);
        }
      } catch (e) {
        // Wallet might be closed — stop polling
        this.stopEventPolling();
      }
    }, intervalMs);
  }

  /**
   * Stop automatic event polling.
   */
  stopEventPolling() {
    if (this._eventTimer) {
      clearInterval(this._eventTimer);
      this._eventTimer = null;
    }
    this._eventCallback = null;
  }

  // ====== PoW Status ======

  getPowStatus() {
    return this.call('getPowStatus');
  }

  // ====== Logging ======

  setLogLevel(level) {
    return this.call('setLogLevel', { level });
  }

  takeLogsJson() {
    return this.call('takeLogsJson');
  }

  clearLogs() {
    return this.call('clearLogs');
  }

  // ====== Coinbase ======

  setScanCoinbase(scan) {
    return this.call('setScanCoinbase', { scan });
  }

  // ====== File Management (Browser Storage) ======

  /**
   * List all wallet files stored in IndexedDB.
   * @returns {string[]}
   */
  async listWallets() {
    return this._storage.listFiles();
  }

  /**
   * Download the current wallet file to the user's computer.
   */
  async downloadWallet(filename) {
    const fn = normaliseFilename(filename || this._currentFilename);
    if (!fn) throw new Error('No wallet filename specified');
    // Ensure latest data is in IndexedDB
    await this.save();
    await this._storage.downloadFile(fn);
  }

  /**
   * Import a wallet file from the user's computer into IndexedDB.
   * @param {string} filename - Name to store as
   * @param {File} file - Browser File object from <input type="file">
   * @returns {string} Base64 data (for reference)
   */
  async importWalletFile(filename, file) {
    return this._storage.importFromFile(normaliseFilename(filename), file);
  }

  /** Storage usage/quota, or null when unavailable. */
  async storageEstimate() {
    return this._storage.estimateUsage();
  }

  /** True when the browser granted persistent (non-evictable) storage. */
  isStoragePersisted() {
    return this._storage.isPersisted();
  }

  /**
   * Get the underlying WalletStorage instance for direct access.
   */
  get storage() {
    return this._storage;
  }

  // ====== Version ======

  apiVersion() {
    return this.call('apiVersion');
  }

  versionString() {
    return this.call('versionString');
  }

  // ====== Internal ======

  /**
   * Export wallet file data from WASM in-memory store and persist to IndexedDB.
   *
   * Failures propagate. Swallowing them here is how a full quota or a denied
   * write turns into silent data loss: the UI reports "saved" while nothing
   * reached disk.
   */
  async _persistToIndexedDB() {
    if (!this._currentFilename) return;

    let exported;
    try {
      exported = this.call('exportFileData', { filename: this._currentFilename });
    } catch (err) {
      // The file genuinely may not exist yet (wallet created but never saved).
      // That is not an error worth failing the caller over.
      return;
    }

    if (!exported || !exported.dataBase64) return;
    await this._storage.saveFile(this._currentFilename, exported.dataBase64);
  }
}

// Event type constants (matching wallet_capi.h)
export const WALLET_EVENT_NONE = 0;
export const WALLET_EVENT_SYNCED = 1;
export const WALLET_EVENT_TRANSACTION = 2;

// ====== Shared parameter normalisation ======================================
// WalletBridge and WalletBridgeWorker must apply identical defaults, or the
// same call behaves differently depending on which side of the worker you are
// on. `broadcast` is the dangerous one: undefined must mean "yes, broadcast".

function normaliseSendBasic(destination, amount, opts = {}) {
  return {
    destination,
    amount,
    paymentId: opts.paymentId || '',
    sendAll: opts.sendAll || false,
    broadcast: opts.broadcast !== false,
  };
}

function normaliseSweep(destination, opts = {}) {
  return {
    destination,
    paymentId: opts.paymentId || '',
    amountToSweep: opts.amountToSweep || 0,
  };
}

/**
 * Accept either a plain filename or a `{ filename }` wrapper.
 *
 * The Dart interop layer marshals every lifecycle call as a single options
 * object, so `deleteFile` arrives here wrapped. Passing that object straight
 * to IndexedDB throws DataError ("not a valid key") and the delete silently
 * does nothing.
 */
function normaliseFilename(value) {
  if (typeof value === 'string') return value;
  if (value && typeof value === 'object' && typeof value.filename === 'string') {
    return value.filename;
  }
  if (value === null || value === undefined) return '';
  throw new TypeError(`Expected a wallet filename string, got ${typeof value}`);
}

// ====== Worker-based proxy (runs on main thread, delegates to Web Worker) ======

/**
 * WalletBridgeWorker — main-thread proxy that delegates all WASM calls to a
 * dedicated Web Worker, keeping the browser UI thread responsive.
 *
 * Provides the same public API as WalletBridge, but every method is async.
 * The Dart js_interop layer calls this instead of WalletBridge directly.
 *
 * Usage:
 *   const wallet = new WalletBridgeWorker();
 *   await wallet.init('./wallet_worker.js', './wallet_wasm.js');
 *   await wallet.create({ filename: 'my_wallet', ... });
 */
export class WalletBridgeWorker {
  constructor() {
    this._worker = null;
    this._nextId = 1;
    this._pending = new Map(); // id → { resolve, reject, timer }
    this._eventCallback = null;
    this._dead = null; // Error once the worker has died
  }

  /**
   * Spawn the worker and initialize the WASM module inside it.
   */
  async init(workerPath = './wallet_worker.js', wasmPath = './wallet_wasm.js') {
    this._worker = new Worker(workerPath, { type: 'module' });

    this._worker.onmessage = (e) => {
      const msg = e.data;

      // Event broadcast from polling
      if (msg.type === 'event') {
        if (this._eventCallback) {
          this._eventCallback(msg.eventType, msg.eventData);
        }
        return;
      }

      // RPC response
      const p = this._pending.get(msg.id);
      if (!p) return;
      this._pending.delete(msg.id);
      if (p.timer) clearTimeout(p.timer);
      if (msg.ok) {
        p.resolve(msg.result);
      } else {
        p.reject(new Error(msg.error || 'Unknown worker error'));
      }
    };

    // A worker that fails to load (404, syntax error, WASM OOM) previously
    // left every pending call hanging forever, which the UI showed as an
    // eternal spinner. Fail them all loudly instead.
    this._worker.onerror = (e) => {
      const err = new Error(
        `Wallet worker crashed: ${e.message || 'failed to load ' + workerPath}`);
      console.error('[WalletBridgeWorker]', err.message, e);
      this._failAll(err);
    };

    this._worker.onmessageerror = (e) => {
      console.error('[WalletBridgeWorker] message could not be deserialised', e);
    };

    // Send init message
    return this._send('init', null, null, { wasmPath });
  }

  /** Reject every in-flight call and mark the bridge dead. */
  _failAll(err) {
    this._dead = err;
    for (const [, p] of this._pending) {
      if (p.timer) clearTimeout(p.timer);
      p.reject(err);
    }
    this._pending.clear();
  }

  /** Low-level: send a message to the worker and return a Promise. */
  _send(type, method, params, extra = {}) {
    if (this._dead) return Promise.reject(this._dead);
    if (!this._worker) {
      return Promise.reject(new Error('WalletBridgeWorker not initialised — call init() first'));
    }

    return new Promise((resolve, reject) => {
      const id = this._nextId++;
      const entry = { resolve, reject, timer: null };

      // Bound anything that is not expected to block for minutes, so a wedged
      // worker surfaces as an error instead of an indefinite spinner.
      const unbounded = type === 'init' || (method && UNBOUNDED_METHODS.has(method));
      if (!unbounded) {
        entry.timer = setTimeout(() => {
          if (this._pending.delete(id)) {
            reject(new Error(
              `Wallet call '${method || type}' timed out after ${DEFAULT_CALL_TIMEOUT_MS / 1000}s`));
          }
        }, DEFAULT_CALL_TIMEOUT_MS);
      }

      this._pending.set(id, entry);
      try {
        this._worker.postMessage({ id, type, method, params, ...extra });
      } catch (err) {
        this._pending.delete(id);
        if (entry.timer) clearTimeout(entry.timer);
        reject(err);
      }
    });
  }

  /** Call a synchronous WASM method via the worker (non-blocking from main thread). */
  call(method, params = {}) {
    return this._send('call', method, params);
  }

  /**
   * Invoke an async (IndexedDB-touching) worker method by name, passing a
   * single params object.
   *
   * This is the entry point the Dart interop layer uses for every stateful
   * operation. Going through named methods instead meant each one had its own
   * arity and shape — `deleteFile(filename)` took a bare string while Dart
   * always marshals an options object, so the object got wrapped twice and
   * IndexedDB was handed `{filename: {filename: "..."}}` as a key. One uniform
   * (method, params) contract makes that class of mismatch impossible.
   */
  request(method, params = {}) {
    return this._send('async', method, params);
  }

  /** Call an async method (involves IndexedDB). */
  _async(method, params = {}) {
    return this._send('async', method, params);
  }

  // ====== Lifecycle (async — IndexedDB) ======
  create(opts) { return this._async('create', opts); }
  open(opts) { return this._async('open', opts); }
  restoreFromSeed(opts) { return this._async('restoreFromSeed', opts); }
  restoreFromKeys(opts) { return this._async('restoreFromKeys', opts); }
  restoreViewWallet(opts) { return this._async('restoreViewWallet', opts); }
  close() { return this._async('close'); }
  save() { return this._async('save'); }
  deleteFile(filename) { return this._async('deleteFile', { filename: normaliseFilename(filename) }); }
  listWallets() { return this._async('listWallets'); }
  storageEstimate() { return this._async('storageEstimate'); }
  importWalletFile(filename, file) {
    return this._async('importWalletFile', { filename: normaliseFilename(filename), file });
  }

  /**
   * Save the encrypted wallet file to the user's computer.
   *
   * The worker has no DOM, so it hands back the encoded bytes and the anchor
   * click happens here on the main thread.
   */
  async downloadWallet(filename) {
    const { filename: name, dataBase64 } =
      await this._async('exportWalletData', { filename: normaliseFilename(filename) });
    if (!dataBase64) throw new Error(`Wallet file not found: ${name}`);

    const blob = new Blob([base64ToBytes(dataBase64)], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = name.endsWith('.wallet') ? name : `${name}.wallet`;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 30_000);
    return name;
  }

  // ====== Sync & Node (sync WASM via worker) ======
  getSyncStatus() { return this.call('getSyncStatus'); }
  getStatusJson() { return this.call('getStatusJson'); }
  isDaemonOnline() { return this.call('isDaemonOnline'); }
  getNodeInfoJson() { return this.call('getNodeInfoJson'); }
  swapNode(daemonHost, daemonPort, daemonSsl = false) {
    return this.call('swapNode', { daemonHost, daemonPort, daemonSsl });
  }
  reset(scanHeight = 0, timestamp = 0) {
    return this.call('reset', { scanHeight, timestamp });
  }

  // ====== Balance ======
  getTotalBalance() { return this.call('getTotalBalance'); }
  getBalanceForAddress(address) { return this.call('getBalanceForAddress', { address }); }
  getBalancesJson() { return this.call('getBalancesJson'); }

  // ====== Addresses ======
  getPrimaryAddress() { return this.call('getPrimaryAddress'); }
  getAddressesJson() { return this.call('getAddressesJson'); }

  // ====== Transactions ======
  getTransactionsJson(startHeight = 0, endHeight = 0, includeUnconfirmed = true) {
    return this.call('getTransactionsJson', { startHeight, endHeight, includeUnconfirmed });
  }
  getTransactionsStatusJson(requestJson) {
    return this.call('getTransactionsStatusJson', { requestJson });
  }
  getTxPrivateKey(txHash) { return this.call('getTxPrivateKey', { txHash }); }

  // ====== Send / Sweep ======
  // Routed through the 'async' channel so the worker persists to IndexedDB
  // immediately after the spend, matching WalletBridge's behaviour.
  sendBasic(destination, amount, opts = {}) {
    return this._async('sendBasic', normaliseSendBasic(destination, amount, opts));
  }
  sendPrepared(preparedTxHash) { return this._async('sendPrepared', { preparedTxHash }); }
  sendAdvancedJson(requestJson, broadcast = true) {
    return this._async('sendAdvancedJson', { requestJson, broadcast });
  }
  sweepToAddress(destination, opts = {}) {
    return this._async('sweepToAddress', normaliseSweep(destination, opts));
  }
  estimateSweep(amountToSweep = 0) { return this.call('estimateSweep', { amountToSweep }); }

  // ====== Keys / Seeds ======
  getPrivateViewKey() { return this.call('getPrivateViewKey'); }
  getSpendKeysJson(address) { return this.call('getSpendKeysJson', { address }); }
  getMnemonicSeed() { return this.call('getMnemonicSeed'); }
  getMnemonicSeedForAddress(address) { return this.call('getMnemonicSeedForAddress', { address }); }
  isViewWallet() { return this.call('isViewWallet'); }

  // ====== Password ======
  changePassword(newPassword) { return this._async('changePassword', { newPassword }); }

  // ====== Export ======
  exportJson() { return this.call('exportJson'); }

  // ====== Subwallets ======
  addSubwallet() { return this._async('addSubwallet'); }
  importSubwalletFromKey(privateSpendKey, scanHeight = 0) {
    return this._async('importSubwalletFromKey', { privateSpendKey, scanHeight });
  }
  importSubwalletFromIndex(walletIndex, scanHeight = 0) {
    return this._async('importSubwalletFromIndex', { walletIndex, scanHeight });
  }
  deleteSubwallet(address) { return this._async('deleteSubwallet', { address }); }

  // ====== Integrated Address ======
  createIntegratedAddress(address, paymentId) {
    return this.call('createIntegratedAddress', { address, paymentId });
  }

  // ====== Events ======
  startEventPolling(callback, intervalMs = 1000) {
    this._eventCallback = callback;
    return this._send('startEvents', null, null, { intervalMs });
  }
  stopEventPolling() {
    this._eventCallback = null;
    return this._send('stopEvents');
  }

  // ====== PoW Status ======
  getPowStatus() { return this.call('getPowStatus'); }

  // ====== Logging ======
  setLogLevel(level) { return this.call('setLogLevel', { level }); }
  takeLogsJson() { return this.call('takeLogsJson'); }
  clearLogs() { return this.call('clearLogs'); }

  // ====== Coinbase ======
  setScanCoinbase(scan) { return this.call('setScanCoinbase', { scan }); }

  // ====== Version ======
  apiVersion() { return this.call('apiVersion'); }
  versionString() { return this.call('versionString'); }

  // ====== Cleanup ======
  terminate() {
    if (this._worker) {
      this._worker.terminate();
      this._worker = null;
    }
    this._failAll(new Error('Wallet worker terminated'));
  }
}
