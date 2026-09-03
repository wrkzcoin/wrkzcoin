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

import { WalletStorage } from './wallet_storage.js';

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

    // Verify the exported function exists
    console.log('[WalletBridge] module ready. _wallet_wasm_request exported:',
      typeof this._module._wallet_wasm_request);

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

    const resultStr = this._module.UTF8ToString(resultPtr);

    // syncStep is called back to back for as long as the daemon has blocks to
    // give, so logging it would flood the console and cost more than the call.
    if (method !== 'syncStep') {
      console.log(`[WalletBridge] ${method} →`, resultStr.length > 300 ? resultStr.substring(0, 300) + '…' : resultStr);
    }

    this._module._free(resultPtr);

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
   */
  async close() {
    await this.save();
    const result = this.call('close');
    this.stopEventPolling();
    this._currentFilename = null;
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
   */
  async deleteFile(filename) {
    try { this.call('deleteFile', { filename }); } catch (_) { /* may not exist in WASM store */ }
    await this._storage.deleteFile(filename);
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

  sendBasic(destination, amount, opts = {}) {
    return this.call('sendBasic', {
      destination,
      amount,
      paymentId: opts.paymentId || '',
      sendAll: opts.sendAll || false,
      broadcast: opts.broadcast !== false,
    });
  }

  sendPrepared(preparedTxHash) {
    return this.call('sendPrepared', { preparedTxHash });
  }

  sendAdvancedJson(requestJson, broadcast = true) {
    return this.call('sendAdvancedJson', { requestJson, broadcast });
  }

  sweepToAddress(destination, opts = {}) {
    return this.call('sweepToAddress', {
      destination,
      paymentId: opts.paymentId || '',
      amountToSweep: opts.amountToSweep || 0,
    });
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

  changePassword(newPassword) {
    return this.call('changePassword', { newPassword });
  }

  // ====== Export ======

  exportJson() {
    return this.call('exportJson');
  }

  // ====== Subwallets ======

  addSubwallet() {
    return this.call('addSubwallet');
  }

  importSubwalletFromKey(privateSpendKey, scanHeight = 0) {
    return this.call('importSubwalletFromKey', { privateSpendKey, scanHeight });
  }

  importSubwalletFromIndex(walletIndex, scanHeight = 0) {
    return this.call('importSubwalletFromIndex', { walletIndex, scanHeight });
  }

  deleteSubwallet(address) {
    return this.call('deleteSubwallet', { address });
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
        const ev = this.pollEvent(50);
        if (ev.eventType !== 0 && this._eventCallback) {
          this._eventCallback(ev.eventType, ev.eventData);
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

  // ====== External Tx PoW server ======

  setTxPowServer(host, port, ssl = false) {
    return this.call('setTxPowServer', { host, port, ssl });
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
    const fn = filename || this._currentFilename;
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
    return this._storage.importFromFile(filename, file);
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
   */
  async _persistToIndexedDB() {
    if (!this._currentFilename) return;
    try {
      const exported = this.call('exportFileData', {
        filename: this._currentFilename,
      });
      if (exported && exported.dataBase64) {
        await this._storage.saveFile(this._currentFilename, exported.dataBase64);
      }
    } catch (_) {
      // File might not exist yet (e.g., wallet just created but not saved)
    }
  }
}

// Event type constants (matching wallet_capi.h)
export const WALLET_EVENT_NONE = 0;
export const WALLET_EVENT_SYNCED = 1;
export const WALLET_EVENT_TRANSACTION = 2;

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
    this._pending = new Map(); // id → { resolve, reject }
    this._eventCallback = null;
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
      if (msg.ok) {
        p.resolve(msg.result);
      } else {
        p.reject(new Error(msg.error || 'Unknown worker error'));
      }
    };

    this._worker.onerror = (e) => {
      console.error('[WalletBridgeWorker] worker error:', e);
    };

    // Send init message
    return this._send('init', null, null, { wasmPath });
  }

  /** Low-level: send a message to the worker and return a Promise. */
  _send(type, method, params, extra = {}) {
    return new Promise((resolve, reject) => {
      const id = this._nextId++;
      this._pending.set(id, { resolve, reject });
      this._worker.postMessage({ id, type, method, params, ...extra });
    });
  }

  /** Call a synchronous WASM method via the worker (non-blocking from main thread). */
  call(method, params = {}) {
    return this._send('call', method, params);
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
  deleteFile(filename) { return this._async('deleteFile', { filename }); }
  listWallets() { return this._async('listWallets'); }

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
  sendBasic(destination, amount, opts = {}) {
    return this.call('sendBasic', { destination, amount, ...opts });
  }
  sendPrepared(preparedTxHash) { return this.call('sendPrepared', { preparedTxHash }); }
  sendAdvancedJson(requestJson, broadcast = true) {
    return this.call('sendAdvancedJson', { requestJson, broadcast });
  }
  sweepToAddress(destination, opts = {}) {
    return this.call('sweepToAddress', { destination, ...opts });
  }
  estimateSweep(amountToSweep = 0) { return this.call('estimateSweep', { amountToSweep }); }

  // ====== Keys / Seeds ======
  getPrivateViewKey() { return this.call('getPrivateViewKey'); }
  getSpendKeysJson(address) { return this.call('getSpendKeysJson', { address }); }
  getMnemonicSeed() { return this.call('getMnemonicSeed'); }
  getMnemonicSeedForAddress(address) { return this.call('getMnemonicSeedForAddress', { address }); }
  isViewWallet() { return this.call('isViewWallet'); }

  // ====== Password ======
  changePassword(newPassword) { return this.call('changePassword', { newPassword }); }

  // ====== Export ======
  exportJson() { return this.call('exportJson'); }

  // ====== Subwallets ======
  addSubwallet() { return this.call('addSubwallet'); }
  importSubwalletFromKey(privateSpendKey, scanHeight = 0) {
    return this.call('importSubwalletFromKey', { privateSpendKey, scanHeight });
  }
  importSubwalletFromIndex(walletIndex, scanHeight = 0) {
    return this.call('importSubwalletFromIndex', { walletIndex, scanHeight });
  }
  deleteSubwallet(address) { return this.call('deleteSubwallet', { address }); }

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

  // ====== External Tx PoW server ======
  setTxPowServer(host, port, ssl = false) { return this.call('setTxPowServer', { host, port, ssl }); }

  // ====== Version ======
  apiVersion() { return this.call('apiVersion'); }
  versionString() { return this.call('versionString'); }

  // ====== Cleanup ======
  terminate() {
    if (this._worker) {
      this._worker.terminate();
      this._worker = null;
    }
    this._pending.clear();
  }
}
