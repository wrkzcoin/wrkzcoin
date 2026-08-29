/**
 * wallet_storage.js
 *
 * IndexedDB persistence layer for WRKZ web wallet files.
 * Stores encrypted wallet binary data in the browser's IndexedDB.
 *
 * Usage:
 *   import { WalletStorage } from './wallet_storage.js';
 *
 *   const storage = new WalletStorage();
 *   await storage.init();
 *   await storage.saveFile('my_wallet.wallet', base64Data);
 *   const data = await storage.loadFile('my_wallet.wallet');
 *   const list = await storage.listFiles();
 *   await storage.deleteFile('my_wallet.wallet');
 *
 * Durability note: every write resolves on the *transaction* completing, not
 * on the individual request succeeding. IndexedDB only guarantees the data is
 * committed once `tx.oncomplete` fires — resolving earlier means a tab closed
 * in that window silently loses the write. For a wallet that is unacceptable.
 */

const DB_NAME = 'wrkz_web_wallet';
const DB_VERSION = 1;
const STORE_FILES = 'wallet_files';
const STORE_META = 'wallet_meta';

/** Chunk size for byte<->base64 conversion (avoids blowing the call stack). */
const B64_CHUNK = 0x8000;

export class WalletStorage {
  constructor() {
    this._db = null;
    this._initPromise = null;
    this._persisted = null; // tri-state: null = unknown, true/false = queried
  }

  /**
   * Open (or create) the IndexedDB database.
   * Must be called before any other method. Safe to call concurrently — the
   * in-flight promise is shared so we never race two `indexedDB.open` calls.
   */
  async init() {
    if (this._db) return;
    if (this._initPromise) return this._initPromise;

    this._initPromise = new Promise((resolve, reject) => {
      let request;
      try {
        request = indexedDB.open(DB_NAME, DB_VERSION);
      } catch (err) {
        // Private-browsing modes can throw synchronously here.
        reject(new Error(`IndexedDB unavailable: ${err && err.message ? err.message : err}`));
        return;
      }

      request.onupgradeneeded = (event) => {
        const db = event.target.result;

        // Store for wallet file binary data (key = filename)
        if (!db.objectStoreNames.contains(STORE_FILES)) {
          db.createObjectStore(STORE_FILES, { keyPath: 'filename' });
        }

        // Store for metadata (preferences, registry, etc.)
        if (!db.objectStoreNames.contains(STORE_META)) {
          db.createObjectStore(STORE_META, { keyPath: 'key' });
        }
      };

      request.onblocked = () => {
        reject(new Error(
          'IndexedDB upgrade blocked — this wallet is open in another tab. ' +
          'Close the other tab and reload.'));
      };

      request.onsuccess = (event) => {
        this._db = event.target.result;
        // If another tab requests a version upgrade, drop our handle so the
        // upgrade is not blocked forever.
        this._db.onversionchange = () => {
          try { this._db.close(); } catch (_) { /* already closing */ }
          this._db = null;
          this._initPromise = null;
        };
        resolve();
      };

      request.onerror = (event) => {
        reject(new Error(`IndexedDB open failed: ${event.target.error}`));
      };
    });

    try {
      await this._initPromise;
    } catch (err) {
      this._initPromise = null; // allow a retry
      throw err;
    }

    // Ask the browser not to evict us under storage pressure. Best-effort:
    // a wallet living in "best effort" storage can be cleared without warning.
    await this._requestPersistence();
  }

  /**
   * Request persistent storage so the browser will not evict the wallet when
   * disk runs low. Returns true if storage is persisted.
   */
  async _requestPersistence() {
    if (this._persisted !== null) return this._persisted;
    try {
      if (!navigator.storage || !navigator.storage.persist) {
        this._persisted = false;
        return false;
      }
      this._persisted = await navigator.storage.persisted()
        ? true
        : await navigator.storage.persist();
      if (!this._persisted) {
        console.warn('[WalletStorage] persistent storage denied — the browser ' +
          'may evict wallet data under disk pressure. Keep a seed backup.');
      }
      return this._persisted;
    } catch (_) {
      this._persisted = false;
      return false;
    }
  }

  /** True when the browser granted persistent (non-evictable) storage. */
  isPersisted() {
    return this._persisted === true;
  }

  /**
   * Report storage usage/quota, or null when the API is unavailable.
   * @returns {{usage: number, quota: number}|null}
   */
  async estimateUsage() {
    try {
      if (!navigator.storage || !navigator.storage.estimate) return null;
      const { usage, quota } = await navigator.storage.estimate();
      return { usage: usage || 0, quota: quota || 0 };
    } catch (_) {
      return null;
    }
  }

  /**
   * Save a wallet file's binary data (as base64 string) to IndexedDB.
   *
   * The on-disk representation stays base64 rather than a raw Uint8Array:
   * existing wallets are already stored that way and a format migration is not
   * worth the risk of corrupting someone's only copy of a key file.
   *
   * @param {string} filename - The wallet filename (e.g., "my_wallet.wallet")
   * @param {string} dataBase64 - Base64-encoded binary wallet data
   */
  async saveFile(filename, dataBase64) {
    this._assertKey(filename, 'saveFile');
    if (typeof dataBase64 !== 'string') {
      throw new TypeError('saveFile: dataBase64 must be a string');
    }
    return this._put(STORE_FILES, {
      filename,
      dataBase64,
      updatedAt: new Date().toISOString(),
    });
  }

  /**
   * Load a wallet file's binary data from IndexedDB.
   * @param {string} filename
   * @returns {string|null} Base64-encoded data, or null if not found
   */
  async loadFile(filename) {
    this._assertKey(filename, 'loadFile');
    const record = await this._get(STORE_FILES, filename);
    return record ? record.dataBase64 : null;
  }

  /**
   * Delete a wallet file from IndexedDB.
   * @param {string} filename
   */
  async deleteFile(filename) {
    this._assertKey(filename, 'deleteFile');
    return this._delete(STORE_FILES, filename);
  }

  /**
   * List all wallet filenames stored in IndexedDB.
   * @returns {string[]} Array of filenames
   */
  async listFiles() {
    return new Promise((resolve, reject) => {
      const tx = this._db.transaction(STORE_FILES, 'readonly');
      const store = tx.objectStore(STORE_FILES);
      const request = store.getAllKeys();

      request.onsuccess = () => resolve(request.result);
      tx.onerror = () => reject(new Error(`listFiles failed: ${tx.error}`));
      tx.onabort = () => reject(new Error(`listFiles aborted: ${tx.error}`));
    });
  }

  /**
   * Save a metadata key-value pair.
   * @param {string} key
   * @param {*} value - Any JSON-serializable value
   */
  async setMeta(key, value) {
    this._assertKey(key, 'setMeta');
    return this._put(STORE_META, { key, value });
  }

  /**
   * Get a metadata value by key.
   * @param {string} key
   * @returns {*} The stored value, or undefined if not found
   */
  async getMeta(key) {
    this._assertKey(key, 'getMeta');
    const record = await this._get(STORE_META, key);
    return record ? record.value : undefined;
  }

  /**
   * Delete a metadata entry.
   * @param {string} key
   */
  async deleteMeta(key) {
    this._assertKey(key, 'deleteMeta');
    return this._delete(STORE_META, key);
  }

  /**
   * Export a wallet file as a downloadable Blob (for "Save As" to disk).
   * @param {string} filename
   * @returns {Blob|null}
   */
  async exportAsBlob(filename) {
    const b64 = await this.loadFile(filename);
    if (!b64) return null;
    return new Blob([base64ToBytes(b64)], { type: 'application/octet-stream' });
  }

  /**
   * Import a wallet file from a browser File object (from <input type="file">).
   * @param {string} filename - Name to store as
   * @param {File} file - Browser File object
   */
  async importFromFile(filename, file) {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const b64 = bytesToBase64(bytes);
    await this.saveFile(filename, b64);
    return b64;
  }

  /**
   * Trigger a browser download of a wallet file.
   *
   * Main thread only — a Web Worker has no DOM. From the worker, read the data
   * with loadFile() and do the anchor click on the main thread instead.
   * @param {string} filename
   */
  async downloadFile(filename) {
    if (typeof document === 'undefined') {
      throw new Error(
        'downloadFile requires a DOM — call it from the main thread, not the worker');
    }
    const blob = await this.exportAsBlob(filename);
    if (!blob) throw new Error(`File not found: ${filename}`);

    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.style.display = 'none';
    document.body.appendChild(a);
    a.click();
    a.remove();
    // Revoking synchronously can cancel the download in some browsers —
    // give the navigation a turn of the event loop first.
    setTimeout(() => URL.revokeObjectURL(url), 30_000);
  }

  // --- Internal helpers ---

  /** IndexedDB keys must be strings here; an object silently throws DataError. */
  _assertKey(key, who) {
    if (typeof key !== 'string' || key.length === 0) {
      throw new TypeError(`${who}: expected a non-empty string key, got ${typeof key}`);
    }
  }

  _requireDb() {
    if (!this._db) {
      throw new Error('WalletStorage not initialised — call init() first');
    }
    return this._db;
  }

  _put(storeName, record) {
    return new Promise((resolve, reject) => {
      let tx;
      try {
        tx = this._requireDb().transaction(storeName, 'readwrite');
      } catch (err) {
        reject(err);
        return;
      }
      tx.objectStore(storeName).put(record);
      // Resolve only once the transaction has actually committed.
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(new Error(`put failed: ${tx.error}`));
      tx.onabort = () => reject(new Error(
        tx.error && tx.error.name === 'QuotaExceededError'
          ? 'Browser storage quota exceeded — free up space or the wallet cannot be saved.'
          : `put aborted: ${tx.error}`));
    });
  }

  _get(storeName, key) {
    return new Promise((resolve, reject) => {
      let tx;
      try {
        tx = this._requireDb().transaction(storeName, 'readonly');
      } catch (err) {
        reject(err);
        return;
      }
      const request = tx.objectStore(storeName).get(key);
      request.onsuccess = () => resolve(request.result || null);
      tx.onerror = () => reject(new Error(`get failed: ${tx.error}`));
      tx.onabort = () => reject(new Error(`get aborted: ${tx.error}`));
    });
  }

  _delete(storeName, key) {
    return new Promise((resolve, reject) => {
      let tx;
      try {
        tx = this._requireDb().transaction(storeName, 'readwrite');
      } catch (err) {
        reject(err);
        return;
      }
      tx.objectStore(storeName).delete(key);
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(new Error(`delete failed: ${tx.error}`));
      tx.onabort = () => reject(new Error(`delete aborted: ${tx.error}`));
    });
  }
}

// --- base64 helpers ---------------------------------------------------------
// String.fromCharCode(...bytes) blows the call stack past ~100k arguments and
// building the string one byte at a time is quadratic. Chunk it.

/** @param {Uint8Array} bytes @returns {string} */
export function bytesToBase64(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i += B64_CHUNK) {
    binary += String.fromCharCode.apply(null, bytes.subarray(i, i + B64_CHUNK));
  }
  return btoa(binary);
}

/** @param {string} b64 @returns {Uint8Array} */
export function base64ToBytes(b64) {
  const binary = atob(b64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}
