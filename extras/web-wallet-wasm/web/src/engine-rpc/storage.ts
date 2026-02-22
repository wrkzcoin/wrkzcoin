import type {
  DirectRpcSessionProfile,
  NodeCapabilities,
  NodeScore,
  RpcEngineStatus,
  ScannedHeaderEntry,
  WalletTxHistoryEntry,
  SyncCursor,
  SyncRuntimeStats,
  WalletSummary
} from "./types";

const STORAGE_PREFIX = "wrkz_direct_rpc_v1";
const CURSOR_KEY = `${STORAGE_PREFIX}:cursor`;
const STATUS_KEY = `${STORAGE_PREFIX}:status`;
const SUMMARY_KEY = `${STORAGE_PREFIX}:summary`;
const PROFILE_KEY = `${STORAGE_PREFIX}:profile`;
const STATS_KEY = `${STORAGE_PREFIX}:stats`;
const CAPABILITIES_KEY = `${STORAGE_PREFIX}:capabilities`;
const NODE_SCORES_KEY = `${STORAGE_PREFIX}:node_scores`;
const HISTORY_KEY = `${STORAGE_PREFIX}:history`;
const TX_HISTORY_KEY = `${STORAGE_PREFIX}:tx_history`;
const SCANNER_SNAPSHOT_KEY = `${STORAGE_PREFIX}:scanner_snapshot`;
const SNAPSHOT_DB_NAME = "wrkz_direct_rpc_state";
const SNAPSHOT_DB_VERSION = 1;
const SNAPSHOT_STORE = "scanner_snapshots";
const SNAPSHOT_ID = "active";

type ScannerSnapshotRecord = { walletId: string; snapshot: string; cursorHeight: number; updatedAt: number };

let snapshotDbPromise: Promise<IDBDatabase> | null = null;

function openSnapshotDb(): Promise<IDBDatabase> {
  if (snapshotDbPromise) {
    return snapshotDbPromise;
  }
  snapshotDbPromise = new Promise((resolve, reject) => {
    const request = indexedDB.open(SNAPSHOT_DB_NAME, SNAPSHOT_DB_VERSION);
    request.onupgradeneeded = () => {
      const db = request.result;
      if (!db.objectStoreNames.contains(SNAPSHOT_STORE)) {
        db.createObjectStore(SNAPSHOT_STORE);
      }
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error ?? new Error("snapshot_db_open_failed"));
  });
  return snapshotDbPromise;
}

async function idbPutSnapshot(record: ScannerSnapshotRecord): Promise<void> {
  const db = await openSnapshotDb();
  await new Promise<void>((resolve, reject) => {
    const tx = db.transaction(SNAPSHOT_STORE, "readwrite");
    tx.objectStore(SNAPSHOT_STORE).put(record, SNAPSHOT_ID);
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error ?? new Error("snapshot_db_put_failed"));
  });
}

async function idbGetSnapshot(): Promise<ScannerSnapshotRecord | null> {
  const db = await openSnapshotDb();
  return await new Promise<ScannerSnapshotRecord | null>((resolve, reject) => {
    const tx = db.transaction(SNAPSHOT_STORE, "readonly");
    const request = tx.objectStore(SNAPSHOT_STORE).get(SNAPSHOT_ID);
    request.onsuccess = () => {
      resolve((request.result as ScannerSnapshotRecord | undefined) ?? null);
    };
    request.onerror = () => reject(request.error ?? new Error("snapshot_db_get_failed"));
  });
}

async function idbDeleteSnapshot(): Promise<void> {
  const db = await openSnapshotDb();
  await new Promise<void>((resolve, reject) => {
    const tx = db.transaction(SNAPSHOT_STORE, "readwrite");
    tx.objectStore(SNAPSHOT_STORE).delete(SNAPSHOT_ID);
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error ?? new Error("snapshot_db_delete_failed"));
  });
}

export class RpcEngineStorage {
  public async saveScannerSnapshot(walletId: string, snapshot: string, cursorHeight: number): Promise<void> {
    const record = {
      walletId,
      snapshot,
      cursorHeight,
      updatedAt: Date.now()
    };
    try {
      await idbPutSnapshot(record);
    } catch {
      localStorage.setItem(SCANNER_SNAPSHOT_KEY, JSON.stringify(record));
    }
  }

  public async loadScannerSnapshot(): Promise<{ walletId: string; snapshot: string; cursorHeight: number; updatedAt: number } | null> {
    try {
      const idbRecord = await idbGetSnapshot();
      if (idbRecord) {
        return idbRecord;
      }
    } catch {
      // Fallback to legacy localStorage snapshot.
    }
    const raw = localStorage.getItem(SCANNER_SNAPSHOT_KEY);
    if (!raw) {
      return null;
    }
    return JSON.parse(raw) as { walletId: string; snapshot: string; cursorHeight: number; updatedAt: number };
  }

  public async clearScannerSnapshot(): Promise<void> {
    try {
      await idbDeleteSnapshot();
    } catch {
      // Keep localStorage clear path regardless of indexedDB state.
    }
    localStorage.removeItem(SCANNER_SNAPSHOT_KEY);
  }

  public async saveCursor(cursor: SyncCursor): Promise<void> {
    localStorage.setItem(CURSOR_KEY, JSON.stringify(cursor));
  }

  public async loadCursor(): Promise<SyncCursor | null> {
    const raw = localStorage.getItem(CURSOR_KEY);
    if (!raw) {
      return null;
    }
    return JSON.parse(raw) as SyncCursor;
  }

  public async saveStatus(status: RpcEngineStatus): Promise<void> {
    localStorage.setItem(STATUS_KEY, JSON.stringify(status));
  }

  public async loadStatus(): Promise<RpcEngineStatus | null> {
    const raw = localStorage.getItem(STATUS_KEY);
    if (!raw) {
      return null;
    }
    return JSON.parse(raw) as RpcEngineStatus;
  }

  public async saveSummary(summary: WalletSummary): Promise<void> {
    localStorage.setItem(SUMMARY_KEY, JSON.stringify(summary));
  }

  public async loadSummary(): Promise<WalletSummary | null> {
    const raw = localStorage.getItem(SUMMARY_KEY);
    if (!raw) {
      return null;
    }
    return JSON.parse(raw) as WalletSummary;
  }

  public async saveProfile(profile: DirectRpcSessionProfile): Promise<void> {
    localStorage.setItem(PROFILE_KEY, JSON.stringify(profile));
  }

  public async clearProfile(): Promise<void> {
    localStorage.removeItem(PROFILE_KEY);
  }

  public async loadProfile(): Promise<DirectRpcSessionProfile | null> {
    const raw = localStorage.getItem(PROFILE_KEY);
    if (!raw) {
      return null;
    }
    return JSON.parse(raw) as DirectRpcSessionProfile;
  }

  public async saveSyncStats(stats: SyncRuntimeStats): Promise<void> {
    localStorage.setItem(STATS_KEY, JSON.stringify(stats));
  }

  public async loadSyncStats(): Promise<SyncRuntimeStats | null> {
    const raw = localStorage.getItem(STATS_KEY);
    if (!raw) {
      return null;
    }
    return JSON.parse(raw) as SyncRuntimeStats;
  }

  public async saveNodeCapabilities(capabilities: NodeCapabilities): Promise<void> {
    const all = await this.loadNodeCapabilitiesMap();
    all[capabilities.nodeKey] = capabilities;
    localStorage.setItem(CAPABILITIES_KEY, JSON.stringify(all));
  }

  public async loadNodeCapabilities(nodeKey: string): Promise<NodeCapabilities | null> {
    const all = await this.loadNodeCapabilitiesMap();
    return all[nodeKey] ?? null;
  }

  private async loadNodeCapabilitiesMap(): Promise<Record<string, NodeCapabilities>> {
    const raw = localStorage.getItem(CAPABILITIES_KEY);
    if (!raw) {
      return {};
    }
    return JSON.parse(raw) as Record<string, NodeCapabilities>;
  }

  public async saveNodeScore(score: NodeScore): Promise<void> {
    const map = await this.loadNodeScoresMap();
    map[score.nodeKey] = score;
    localStorage.setItem(NODE_SCORES_KEY, JSON.stringify(map));
  }

  public async loadNodeScores(): Promise<NodeScore[]> {
    const map = await this.loadNodeScoresMap();
    return Object.values(map);
  }

  private async loadNodeScoresMap(): Promise<Record<string, NodeScore>> {
    const raw = localStorage.getItem(NODE_SCORES_KEY);
    if (!raw) {
      return {};
    }
    return JSON.parse(raw) as Record<string, NodeScore>;
  }

  public async appendHistory(entries: ScannedHeaderEntry[], maxEntries = 500): Promise<void> {
    if (entries.length === 0) {
      return;
    }
    const existing = await this.loadHistory();
    const merged = [...entries, ...existing]
      .sort((a, b) => b.height - a.height)
      .slice(0, maxEntries);
    localStorage.setItem(HISTORY_KEY, JSON.stringify(merged));
  }

  public async loadHistory(): Promise<ScannedHeaderEntry[]> {
    const raw = localStorage.getItem(HISTORY_KEY);
    if (!raw) {
      return [];
    }
    return JSON.parse(raw) as ScannedHeaderEntry[];
  }

  public async appendTransactionHistory(entries: WalletTxHistoryEntry[], maxEntries = 1000): Promise<void> {
    if (entries.length === 0) {
      return;
    }
    const existing = await this.loadTransactionHistory();
    const key = (entry: WalletTxHistoryEntry): string => `${entry.txHash}:${entry.blockHeight}`;
    const mergedMap = new Map<string, WalletTxHistoryEntry>();
    for (const entry of [...entries, ...existing]) {
      mergedMap.set(key(entry), entry);
    }
    const merged = Array.from(mergedMap.values())
      .sort((a, b) => (b.blockHeight - a.blockHeight) || ((b.blockTimestamp ?? 0) - (a.blockTimestamp ?? 0)))
      .slice(0, maxEntries);
    localStorage.setItem(TX_HISTORY_KEY, JSON.stringify(merged));
  }

  public async loadTransactionHistory(): Promise<WalletTxHistoryEntry[]> {
    const raw = localStorage.getItem(TX_HISTORY_KEY);
    if (!raw) {
      return [];
    }
    return JSON.parse(raw) as WalletTxHistoryEntry[];
  }

  public async clear(): Promise<void> {
    localStorage.removeItem(CURSOR_KEY);
    localStorage.removeItem(STATUS_KEY);
    localStorage.removeItem(SUMMARY_KEY);
    localStorage.removeItem(PROFILE_KEY);
    localStorage.removeItem(STATS_KEY);
    localStorage.removeItem(CAPABILITIES_KEY);
    localStorage.removeItem(NODE_SCORES_KEY);
    localStorage.removeItem(HISTORY_KEY);
    localStorage.removeItem(TX_HISTORY_KEY);
    await this.clearScannerSnapshot();
  }

  public async clearSyncArtifacts(): Promise<void> {
    localStorage.removeItem(CURSOR_KEY);
    localStorage.removeItem(SUMMARY_KEY);
    localStorage.removeItem(STATS_KEY);
    localStorage.removeItem(HISTORY_KEY);
    localStorage.removeItem(TX_HISTORY_KEY);
    await this.clearScannerSnapshot();
  }
}
