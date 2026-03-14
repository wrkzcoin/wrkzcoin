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
 */

const DB_NAME = 'wrkz_web_wallet';
const DB_VERSION = 1;
const STORE_FILES = 'wallet_files';
const STORE_META = 'wallet_meta';

export class WalletStorage {
  constructor() {
    this._db = null;
  }

  /**
   * Open (or create) the IndexedDB database.
   * Must be called before any other method.
   */
  async init() {
    if (this._db) return;

    return new Promise((resolve, reject) => {
      const request = indexedDB.open(DB_NAME, DB_VERSION);

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

      request.onsuccess = (event) => {
        this._db = event.target.result;
        resolve();
      };

      request.onerror = (event) => {
        reject(new Error(`IndexedDB open failed: ${event.target.error}`));
      };
    });
  }

  /**
   * Save a wallet file's binary data (as base64 string) to IndexedDB.
   * @param {string} filename - The wallet filename (e.g., "my_wallet.wallet")
   * @param {string} dataBase64 - Base64-encoded binary wallet data
   */
  async saveFile(filename, dataBase64) {
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
    const record = await this._get(STORE_FILES, filename);
    return record ? record.dataBase64 : null;
  }

  /**
   * Delete a wallet file from IndexedDB.
   * @param {string} filename
   */
  async deleteFile(filename) {
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
      request.onerror = () => reject(new Error(`listFiles failed: ${request.error}`));
    });
  }

  /**
   * Save a metadata key-value pair.
   * @param {string} key
   * @param {*} value - Any JSON-serializable value
   */
  async setMeta(key, value) {
    return this._put(STORE_META, { key, value });
  }

  /**
   * Get a metadata value by key.
   * @param {string} key
   * @returns {*} The stored value, or undefined if not found
   */
  async getMeta(key) {
    const record = await this._get(STORE_META, key);
    return record ? record.value : undefined;
  }

  /**
   * Delete a metadata entry.
   * @param {string} key
   */
  async deleteMeta(key) {
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

    const binary = atob(b64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) {
      bytes[i] = binary.charCodeAt(i);
    }
    return new Blob([bytes], { type: 'application/octet-stream' });
  }

  /**
   * Import a wallet file from a browser File object (from <input type="file">).
   * @param {string} filename - Name to store as
   * @param {File} file - Browser File object
   */
  async importFromFile(filename, file) {
    const buffer = await file.arrayBuffer();
    const bytes = new Uint8Array(buffer);
    let binary = '';
    for (let i = 0; i < bytes.length; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    const b64 = btoa(binary);
    await this.saveFile(filename, b64);
    return b64;
  }

  /**
   * Trigger a browser download of a wallet file.
   * @param {string} filename
   */
  async downloadFile(filename) {
    const blob = await this.exportAsBlob(filename);
    if (!blob) throw new Error(`File not found: ${filename}`);

    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  // --- Internal helpers ---

  _put(storeName, record) {
    return new Promise((resolve, reject) => {
      const tx = this._db.transaction(storeName, 'readwrite');
      const store = tx.objectStore(storeName);
      const request = store.put(record);

      request.onsuccess = () => resolve();
      request.onerror = () => reject(new Error(`put failed: ${request.error}`));
    });
  }

  _get(storeName, key) {
    return new Promise((resolve, reject) => {
      const tx = this._db.transaction(storeName, 'readonly');
      const store = tx.objectStore(storeName);
      const request = store.get(key);

      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(new Error(`get failed: ${request.error}`));
    });
  }

  _delete(storeName, key) {
    return new Promise((resolve, reject) => {
      const tx = this._db.transaction(storeName, 'readwrite');
      const store = tx.objectStore(storeName);
      const request = store.delete(key);

      request.onsuccess = () => resolve();
      request.onerror = () => reject(new Error(`delete failed: ${request.error}`));
    });
  }
}
