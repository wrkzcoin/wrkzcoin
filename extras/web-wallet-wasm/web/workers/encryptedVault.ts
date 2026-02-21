const DB_NAME = "wrkz_web_wallet";
const DB_VERSION = 1;
const STORE_NAME = "vault";
const META_KEY = "__meta__";
const CHECK_KEY = "__check__";
const CHECK_VALUE = "wrkz-vault-check-v1";
const PBKDF2_ITERATIONS = 310000;

interface VaultMeta {
  key: typeof META_KEY;
  saltB64: string;
  iterations: number;
  version: number;
}

interface VaultRecord {
  key: string;
  ivB64: string;
  cipherB64: string;
  updatedAt: string;
}

interface VaultSession {
  key: CryptoKey;
}

let session: VaultSession | null = null;

function toB64(bytes: Uint8Array): string {
  let out = "";
  for (const b of bytes) {
    out += String.fromCharCode(b);
  }
  return btoa(out);
}

function fromB64(b64: string): Uint8Array {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

function utf8Encode(value: string): Uint8Array {
  return new TextEncoder().encode(value);
}

function utf8Decode(value: ArrayBuffer): string {
  return new TextDecoder().decode(value);
}

function asArrayBuffer(bytes: Uint8Array): ArrayBuffer {
  const copy = new Uint8Array(bytes.byteLength);
  copy.set(bytes);
  return copy.buffer;
}

function openDb(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, DB_VERSION);
    request.onupgradeneeded = () => {
      const db = request.result;
      if (!db.objectStoreNames.contains(STORE_NAME)) {
        db.createObjectStore(STORE_NAME, { keyPath: "key" });
      }
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error ?? new Error("indexeddb_open_failed"));
  });
}

function dbGet<T>(db: IDBDatabase, key: string): Promise<T | undefined> {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, "readonly");
    const store = tx.objectStore(STORE_NAME);
    const request = store.get(key);
    request.onsuccess = () => resolve(request.result as T | undefined);
    request.onerror = () => reject(request.error ?? new Error("indexeddb_get_failed"));
  });
}

function dbPut<T>(db: IDBDatabase, value: T): Promise<void> {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, "readwrite");
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error ?? new Error("indexeddb_put_failed"));
    tx.objectStore(STORE_NAME).put(value);
  });
}

function dbDelete(db: IDBDatabase, key: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, "readwrite");
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error ?? new Error("indexeddb_delete_failed"));
    tx.objectStore(STORE_NAME).delete(key);
  });
}

async function deriveKey(password: string, salt: Uint8Array, iterations: number): Promise<CryptoKey> {
  const base = await crypto.subtle.importKey("raw", asArrayBuffer(utf8Encode(password)), "PBKDF2", false, ["deriveKey"]);
  return crypto.subtle.deriveKey(
    { name: "PBKDF2", salt: asArrayBuffer(salt), iterations, hash: "SHA-256" },
    base,
    { name: "AES-GCM", length: 256 },
    false,
    ["encrypt", "decrypt"]
  );
}

async function encryptString(key: CryptoKey, value: string): Promise<{ ivB64: string; cipherB64: string }> {
  const iv = crypto.getRandomValues(new Uint8Array(12));
  const cipher = await crypto.subtle.encrypt({ name: "AES-GCM", iv: asArrayBuffer(iv) }, key, asArrayBuffer(utf8Encode(value)));
  return { ivB64: toB64(iv), cipherB64: toB64(new Uint8Array(cipher)) };
}

async function decryptString(key: CryptoKey, ivB64: string, cipherB64: string): Promise<string> {
  const iv = fromB64(ivB64);
  const cipher = fromB64(cipherB64);
  const plain = await crypto.subtle.decrypt({ name: "AES-GCM", iv: asArrayBuffer(iv) }, key, asArrayBuffer(cipher));
  return utf8Decode(plain);
}

async function getOrCreateMeta(db: IDBDatabase): Promise<VaultMeta> {
  const existing = await dbGet<VaultMeta>(db, META_KEY);
  if (existing) {
    return existing;
  }

  const salt = crypto.getRandomValues(new Uint8Array(16));
  const meta: VaultMeta = {
    key: META_KEY,
    saltB64: toB64(salt),
    iterations: PBKDF2_ITERATIONS,
    version: 1
  };
  await dbPut(db, meta);
  return meta;
}

export async function initializeVault(password: string): Promise<{ initialized: boolean }> {
  if (!password) {
    throw new Error("empty_password");
  }

  const db = await openDb();
  const meta = await getOrCreateMeta(db);
  const key = await deriveKey(password, fromB64(meta.saltB64), meta.iterations);
  const check = await dbGet<VaultRecord>(db, CHECK_KEY);

  if (!check) {
    const encrypted = await encryptString(key, CHECK_VALUE);
    await dbPut<VaultRecord>(db, {
      key: CHECK_KEY,
      ivB64: encrypted.ivB64,
      cipherB64: encrypted.cipherB64,
      updatedAt: new Date().toISOString()
    });
    session = { key };
    db.close();
    return { initialized: true };
  }

  const value = await decryptString(key, check.ivB64, check.cipherB64).catch(() => "");
  if (value !== CHECK_VALUE) {
    db.close();
    throw new Error("invalid_password");
  }

  session = { key };
  db.close();
  return { initialized: true };
}

export function isVaultUnlocked(): boolean {
  return session !== null;
}

export function lockVault(): void {
  session = null;
}

export async function putSecret(secretKey: string, secretValue: string): Promise<void> {
  if (!session) {
    throw new Error("vault_locked");
  }
  if (!secretKey || !secretValue) {
    throw new Error("invalid_secret_input");
  }

  const db = await openDb();
  const encrypted = await encryptString(session.key, secretValue);
  await dbPut<VaultRecord>(db, {
    key: secretKey,
    ivB64: encrypted.ivB64,
    cipherB64: encrypted.cipherB64,
    updatedAt: new Date().toISOString()
  });
  db.close();
}

export async function getSecret(secretKey: string): Promise<string | null> {
  if (!session) {
    throw new Error("vault_locked");
  }
  if (!secretKey) {
    throw new Error("invalid_secret_key");
  }

  const db = await openDb();
  const record = await dbGet<VaultRecord>(db, secretKey);
  db.close();
  if (!record) {
    return null;
  }
  return decryptString(session.key, record.ivB64, record.cipherB64);
}

export async function deleteSecret(secretKey: string): Promise<void> {
  if (!session) {
    throw new Error("vault_locked");
  }
  if (!secretKey || secretKey === CHECK_KEY || secretKey === META_KEY) {
    throw new Error("invalid_secret_key");
  }

  const db = await openDb();
  await dbDelete(db, secretKey);
  db.close();
}
