import { probeNode } from "./nodeProbe";
import { HttpRpcClient } from "./rpcClient";
import { RpcEngineStorage } from "./storage";
import { WalletWorkerClient } from "../wallet/walletWorkerClient";
import { COIN_ADDRESS_PREFIX } from "../config/coin";
import {
  SYNC_NEAR_TIP_BLOCKS,
  SYNC_NEAR_TIP_INTERVAL_MS,
  SYNC_POLL_INTERVAL_MS
} from "../config/sync";
import type {
  DirectRpcEngine,
  DirectRpcSessionProfile,
  BackendRuntimeLogEntry,
  NodeCapabilities,
  NodeScore,
  RpcEngineStatus,
  RpcNode,
  ScannerSnapshotState,
  ScannedHeaderEntry,
  SyncCursor,
  SyncRuntimeStats,
  WalletTxHistoryEntry,
  WalletSummary
} from "./types";

const SYNC_BATCH_SIZE = 250;
const MIN_SYNC_BATCH_SIZE = 1;
const CAPABILITY_TTL_MS = 10 * 60 * 1000;
const NODE_SCORE_DECAY_INTERVAL_MS = 5 * 60 * 1000;
const NODE_SCORE_DECAY_STEP = 3;
const NODE_FAILURE_COOLDOWN_MS = 60 * 1000;
const MAX_AUTO_SYNC_THREADS = 8;

type WalletSyncDataResponse = {
  status?: string;
  items?: Array<Record<string, unknown>>;
  topBlock?: {
    hash?: string;
    height?: number;
  };
  synced?: boolean;
};

type PreparedTransferContext = {
  preparedTxHash: string;
  destination: string;
  amountAtomic: string;
  paymentId: string;
  feeAtomic: string;
  createdAt: number;
};

function nodeKey(node: RpcNode): string {
  return `${node.ssl ? "https" : "http"}://${node.host}:${node.port}`;
}

function isMethodMissing(message: string): boolean {
  return /method not found|unknown method|invalid method|no method/i.test(message);
}

function detectSyncThreads(): number {
  if (typeof navigator !== "undefined" && Number.isFinite(navigator.hardwareConcurrency)) {
    const threads = Math.floor(Number(navigator.hardwareConcurrency));
    if (threads > 0) {
      return Math.max(1, Math.min(MAX_AUTO_SYNC_THREADS, threads));
    }
  }
  return 2;
}

function looksLikeAddressFallback(address: string, allowIntegrated: boolean): boolean {
  const value = address.trim();
  if (!value.startsWith(COIN_ADDRESS_PREFIX)) {
    return false;
  }
  if (!/^[1-9A-HJ-NP-Za-km-z]+$/.test(value)) {
    return false;
  }
  const length = value.length;
  if (allowIntegrated) {
    return length >= 95 && length <= 220;
  }
  return length >= 95 && length <= 120;
}

export class BrowserDirectRpcEngine implements DirectRpcEngine {
  private readonly storage = new RpcEngineStorage();
  private readonly rpc = new HttpRpcClient({ timeoutMs: 15000, retries: 1 });
  private readonly worker = new WalletWorkerClient();
  private running = false;
  private currentWalletId?: string;
  private currentNodeId?: string;
  private currentNode?: RpcNode;
  private nodePool: RpcNode[] = [];
  private pollTimer: ReturnType<typeof setTimeout> | null = null;
  private fetchMode: "unknown" | "range" | "by_height" | "none" = "unknown";
  private failoverCount = 0;
  private scanKeys: { privateSpendKey: string; privateViewKey: string } | null = null;
  private scannerResetPending = true;
  private scanTimestampPrimed = false;
  private vaultUnlocked = false;
  private walletSyncBatchSize = SYNC_BATCH_SIZE;
  private preparedTransfer: PreparedTransferContext | null = null;
  private transferWalletId: number | null = null;
  private static readonly TRANSFER_SYNC_WAIT_TIMEOUT_MS = 180000;
  private static readonly TRANSFER_SYNC_POLL_MS = 400;
  private static readonly PREPARED_TRANSFER_TTL_MS = 2 * 60 * 1000;

  public async configureNodePool(nodes: RpcNode[], preferredNodeId?: string): Promise<void> {
    const uniqueByKey = new Map<string, RpcNode>();
    for (const node of nodes) {
      uniqueByKey.set(nodeKey(node), node);
    }
    let normalized = Array.from(uniqueByKey.values()).sort((a, b) => a.priority - b.priority);
    if (preferredNodeId) {
      const preferredIdx = normalized.findIndex((n) => n.id === preferredNodeId);
      if (preferredIdx > 0) {
        const [preferred] = normalized.splice(preferredIdx, 1);
        normalized = [preferred, ...normalized];
      }
    }
    this.nodePool = normalized;
  }

  public async probeNode(node: RpcNode) {
    return probeNode(node);
  }

  public async start(walletId: string, node: RpcNode): Promise<RpcEngineStatus> {
    await this.stop();
    this.running = true;
    this.currentWalletId = walletId;
    this.currentNodeId = node.id;
    this.currentNode = node;
    this.fetchMode = "unknown";
    this.failoverCount = 0;
    this.scannerResetPending = true;
    this.scanTimestampPrimed = false;
    this.walletSyncBatchSize = SYNC_BATCH_SIZE;
    if (this.nodePool.length === 0) {
      await this.configureNodePool([node], node.id);
    }
    const cachedCapabilities = await this.getNodeCapabilities(node);
    if (!cachedCapabilities) {
      await this.refreshNodeCapabilities(node).catch(() => undefined);
    }

    const status: RpcEngineStatus = {
      running: true,
      walletId,
      nodeId: node.id,
      activeNodeEndpoint: nodeKey(node),
      failoverCount: this.failoverCount,
      message: "direct_rpc_engine_started"
    };
    await this.ensureCursorInitialized(walletId);
    await this.reinitializeScannerStateIfNeeded(walletId);
    await this.storage.saveStatus(status);
    await this.pollOnce();
    const stats = await this.storage.loadSyncStats();
    this.scheduleNextPoll(this.selectPollIntervalMs(stats?.remainingBlocks));
    return status;
  }

  public async stop(): Promise<void> {
    await this.discardPreparedTransfer();
    await this.closeTransferWallet();
    if (this.pollTimer) {
      clearTimeout(this.pollTimer);
      this.pollTimer = null;
    }
    this.running = false;
    this.currentWalletId = undefined;
    this.currentNodeId = undefined;
    this.currentNode = undefined;
    this.fetchMode = "unknown";
    this.failoverCount = 0;
    this.scannerResetPending = true;
    this.scanTimestampPrimed = false;
    this.walletSyncBatchSize = SYNC_BATCH_SIZE;
    await this.storage.saveStatus({ running: false, message: "direct_rpc_engine_stopped", failoverCount: 0 });
  }

  public async getStatus(): Promise<RpcEngineStatus> {
    if (this.running) {
      return {
        running: true,
        walletId: this.currentWalletId,
        nodeId: this.currentNodeId,
        activeNodeEndpoint: this.currentNode ? nodeKey(this.currentNode) : undefined,
        failoverCount: this.failoverCount
      };
    }
    return (await this.storage.loadStatus()) ?? { running: false };
  }

  public async getSummary(): Promise<WalletSummary | null> {
    return this.storage.loadSummary();
  }

  public async getHistory(limit = 50): Promise<ScannedHeaderEntry[]> {
    const history = await this.storage.loadHistory();
    return history.slice(0, Math.max(1, limit));
  }

  public async getTransactionHistory(limit = 100): Promise<WalletTxHistoryEntry[]> {
    const history = await this.storage.loadTransactionHistory();
    const walletId = this.currentWalletId ?? (await this.storage.loadProfile())?.walletId;
    const filtered = walletId ? history.filter((entry) => entry.walletId === walletId) : history;
    return filtered.slice(0, Math.max(1, limit));
  }

  public async getCursor(): Promise<SyncCursor | null> {
    return this.storage.loadCursor();
  }

  public async getScannerSnapshotState(): Promise<ScannerSnapshotState | null> {
    const snapshot = await this.storage.loadScannerSnapshot();
    if (!snapshot) {
      return null;
    }
    const cursor = await this.storage.loadCursor();
    return {
      walletId: snapshot.walletId,
      snapshotHeight: snapshot.cursorHeight,
      cursorHeight: cursor?.walletId === snapshot.walletId ? cursor.height : 0,
      updatedAt: snapshot.updatedAt
    };
  }

  public async getSyncStats(): Promise<SyncRuntimeStats | null> {
    return this.storage.loadSyncStats();
  }

  public async setBackendLogLevel(level: "trace" | "debug" | "info" | "warning" | "fatal"): Promise<void> {
    await this.worker.setBackendLogLevel(level);
  }

  public async takeBackendLogs(): Promise<BackendRuntimeLogEntry[]> {
    const raw = await this.worker.takeBackendLogs();
    const entries = Array.isArray(raw.entries) ? raw.entries : [];
    return entries
      .map((entry) => {
        const levelRaw = String(entry.level ?? "info").toLowerCase();
        const level: "trace" | "debug" | "info" | "warning" | "fatal" =
          levelRaw === "trace" || levelRaw === "debug" || levelRaw === "warning" || levelRaw === "fatal"
            ? levelRaw
            : "info";
        return {
          pretty: String(entry.pretty ?? ""),
          message: String(entry.message ?? ""),
          level,
          categories: Array.isArray(entry.categories) ? entry.categories.map((x) => String(x)) : [],
          ts: Number(entry.ts ?? Date.now())
        } as BackendRuntimeLogEntry;
      })
      .filter((entry) => entry.pretty.length > 0 || entry.message.length > 0);
  }

  public async clearBackendLogs(): Promise<void> {
    await this.worker.clearBackendLogs();
  }

  public async getNodeCapabilities(node: RpcNode): Promise<NodeCapabilities | null> {
    const cached = await this.storage.loadNodeCapabilities(nodeKey(node));
    if (!cached) {
      return null;
    }
    if (Date.now() - cached.checkedAt > CAPABILITY_TTL_MS) {
      return null;
    }
    return cached;
  }

  public async refreshNodeCapabilities(node: RpcNode): Promise<NodeCapabilities> {
    const key = nodeKey(node);
    let supportsGetBlockCount = false;
    let supportsWalletSyncData = false;
    let supportsGetBlockHeadersRange = false;
    let supportsGetBlockHeaderByHeight = false;
    let corsLikely = true;
    let lastError: string | undefined;

    try {
      await this.rpc.call(node, "getblockcount");
      supportsGetBlockCount = true;
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      lastError = reason;
      corsLikely = !reason.includes("Failed to fetch");
    }

    try {
      const walletSync = await this.rpc.postPath<WalletSyncDataResponse>(node, "/getwalletsyncdata", {
        startHeight: 0,
        startTimestamp: 0,
        blockCount: 1,
        skipCoinbaseTransactions: false,
        blockHashCheckpoints: []
      });
      supportsWalletSyncData = walletSync.status === "OK";
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      lastError = lastError ?? reason;
    }

    try {
      await this.tryFetchBlockHeaderRange(node, 1, 1);
      supportsGetBlockHeadersRange = true;
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      supportsGetBlockHeadersRange = false;
      lastError = lastError ?? reason;
    }

    try {
      await this.tryFetchBlockHeaderByHeight(node, 1, 1);
      supportsGetBlockHeaderByHeight = true;
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      supportsGetBlockHeaderByHeight = false;
      lastError = lastError ?? reason;
    }

    const capabilities: NodeCapabilities = {
      nodeKey: key,
      checkedAt: Date.now(),
      supportsGetBlockCount,
      supportsWalletSyncData,
      supportsGetBlockHeadersRange,
      supportsGetBlockHeaderByHeight,
      corsLikely,
      lastError
    };
    await this.storage.saveNodeCapabilities(capabilities);
    return capabilities;
  }

  public async getNodeScores(): Promise<NodeScore[]> {
    const scores = await this.storage.loadNodeScores();
    return scores.sort((a, b) => b.score - a.score);
  }

  public async setProfile(profile: DirectRpcSessionProfile): Promise<void> {
    await this.storage.saveProfile(profile);
  }

  public async clearProfile(): Promise<void> {
    await this.discardPreparedTransfer();
    await this.closeTransferWallet();
    await this.storage.clearProfile();
  }

  public async setScanKeys(privateSpendKey: string, privateViewKey: string): Promise<void> {
    await this.closeTransferWallet();
    this.scanKeys = {
      privateSpendKey: privateSpendKey.trim().toLowerCase(),
      privateViewKey: privateViewKey.trim().toLowerCase()
    };
    this.scannerResetPending = true;
    if (this.vaultUnlocked) {
      await this.worker.vaultPut(
        "direct_rpc_scan_keys",
        JSON.stringify({
          privateSpendKey: this.scanKeys.privateSpendKey,
          privateViewKey: this.scanKeys.privateViewKey
        })
      );
    }
  }

  public async deriveScanKeysFromSeed(
    mnemonicSeed: string
  ): Promise<{ privateSpendKey: string; privateViewKey: string; mnemonicSeed: string; address: string }> {
    const derived = await this.worker.deriveKeysFromSeed(mnemonicSeed);
    await this.setScanKeys(derived.privateSpendKey, derived.privateViewKey);
    return derived;
  }

  public async generateScanKeys(): Promise<{ privateSpendKey: string; privateViewKey: string; mnemonicSeed: string; address: string }> {
    const generated = await this.worker.generateSeedKeys();
    await this.setScanKeys(generated.privateSpendKey, generated.privateViewKey);
    return generated;
  }

  public async deriveAddressFromKeys(privateSpendKey: string, privateViewKey: string): Promise<string> {
    const result = await this.worker.deriveAddressFromKeys(privateSpendKey, privateViewKey);
    return result.address;
  }

  public async validateAddress(address: string, allowIntegrated = true): Promise<{ valid: boolean; reason?: string }> {
    try {
      return await this.worker.validateAddress(address, allowIntegrated);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      if (/memory access out of bounds|unaligned accesses|unwind|RuntimeError/i.test(message)) {
        const fallbackValid = looksLikeAddressFallback(address, allowIntegrated);
        return fallbackValid
          ? { valid: true, reason: "wasm_validate_trap_fallback" }
          : { valid: false, reason: "Address format invalid." };
      }
      throw error;
    }
  }

  public async createIntegratedAddress(
    address: string,
    paymentId: string
  ): Promise<{ integratedAddress: string; error?: string }> {
    return this.worker.createIntegratedAddress(address, paymentId);
  }

  public async prepareBasicTransfer(request: {
    destination: string;
    amountAtomic: string;
    paymentId?: string;
    password: string;
    filenameHint?: string;
  }): Promise<{ preparedTxHash: string; feeAtomic: string }> {
    if (!this.currentNode) {
      throw new Error("no_active_node");
    }
    if (!this.scanKeys) {
      throw new Error("scan_keys_not_loaded");
    }
    const profile = await this.storage.loadProfile();
    if (!profile) {
      throw new Error("no_wallet_profile");
    }
    await this.discardPreparedTransfer();
    const walletId = await this.ensureTransferWallet(profile);
    try {
      await this.waitForTransferWalletSync(walletId);
      const prepared = await this.prepareBasicWithRetry(
        walletId,
        request.destination,
        request.amountAtomic,
        request.paymentId?.trim() ?? ""
      );
      this.preparedTransfer = {
        preparedTxHash: prepared.preparedTxHash,
        destination: request.destination,
        amountAtomic: request.amountAtomic,
        paymentId: request.paymentId?.trim() ?? "",
        feeAtomic: prepared.feeAtomic,
        createdAt: Date.now()
      };
      return prepared;
    } catch (error) {
      throw error;
    }
  }

  public async submitPreparedTransfer(preparedTxHash: string): Promise<{ txHash: string }> {
    const active = this.preparedTransfer;
    if (!active) {
      throw new Error("no_prepared_transfer");
    }
    const now = Date.now();
    if (now - active.createdAt > BrowserDirectRpcEngine.PREPARED_TRANSFER_TTL_MS) {
      await this.discardPreparedTransfer();
      throw new Error("prepared_transfer_expired");
    }
    if (preparedTxHash !== active.preparedTxHash) {
      throw new Error("prepared_transfer_mismatch");
    }
    if (this.transferWalletId === null) {
      throw new Error("transfer_wallet_not_ready");
    }
    const sent = await this.worker.sendPreparedTransfer(this.transferWalletId, preparedTxHash);
    await this.discardPreparedTransfer();
    return sent;
  }

  public async discardPreparedTransfer(): Promise<void> {
    this.preparedTransfer = null;
  }

  public async sendBasicTransfer(request: {
    destination: string;
    amountAtomic: string;
    paymentId?: string;
    password: string;
    filenameHint?: string;
  }): Promise<{ txHash: string }> {
    const prepared = await this.prepareBasicTransfer(request);
    return this.submitPreparedTransfer(prepared.preparedTxHash);
  }

  public async estimateBasicTransferFee(request: {
    destination: string;
    amountAtomic: string;
    paymentId?: string;
    password?: string;
    filenameHint?: string;
  }): Promise<{ feeAtomic: string }> {
    const prepared = await this.prepareBasicTransfer({
      ...request,
      password: request.password?.trim() || "__preflight__"
    });
    try {
      return { feeAtomic: prepared.feeAtomic };
    } finally {
      await this.discardPreparedTransfer();
    }
  }

  private async waitForTransferWalletSync(walletId: number): Promise<void> {
    const startedAt = Date.now();
    while (Date.now() - startedAt < BrowserDirectRpcEngine.TRANSFER_SYNC_WAIT_TIMEOUT_MS) {
      const status = await this.worker.status(walletId).catch(() => null);
      if (status) {
        const walletHeight = Number(status.walletBlockCount ?? 0);
        const localDaemonHeight = Number(status.localDaemonBlockCount ?? 0);
        const networkHeight = Number(status.networkBlockCount ?? 0);
        const targetHeight = localDaemonHeight > 0 ? localDaemonHeight : networkHeight;
        if (!Number.isFinite(targetHeight) || targetHeight <= 0 || walletHeight >= Math.max(0, targetHeight - 1)) {
          return;
        }
      }
      await new Promise<void>((resolve) => setTimeout(resolve, BrowserDirectRpcEngine.TRANSFER_SYNC_POLL_MS));
    }
  }

  private isTransferUnlockedFundsError(error: unknown): boolean {
    const message = error instanceof Error ? error.message : String(error);
    return /not enough unlocked funds/i.test(message);
  }

  private async prepareBasicWithRetry(
    walletId: number,
    destination: string,
    amountAtomic: string,
    paymentId: string
  ): Promise<{ preparedTxHash: string; feeAtomic: string }> {
    const startedAt = Date.now();
    let lastError: unknown;
    while (Date.now() - startedAt < BrowserDirectRpcEngine.TRANSFER_SYNC_WAIT_TIMEOUT_MS) {
      try {
        return await this.worker.prepareBasicTransfer(walletId, destination, amountAtomic, paymentId);
      } catch (error) {
        lastError = error;
        if (!this.isTransferUnlockedFundsError(error)) {
          throw error;
        }
      }
      await new Promise<void>((resolve) => setTimeout(resolve, BrowserDirectRpcEngine.TRANSFER_SYNC_POLL_MS));
    }
    throw lastError instanceof Error ? lastError : new Error(String(lastError ?? "transfer_prepare_timeout"));
  }

  private async verifyDaemonConnectivity(node: RpcNode): Promise<boolean> {
    try {
      await this.rpc.call<{ count?: number }>(node, "getblockcount");
      return true;
    } catch {
      return false;
    }
  }

  private async ensureTransferWallet(profile: DirectRpcSessionProfile): Promise<number> {
    if (this.transferWalletId !== null) {
      return this.transferWalletId;
    }
    if (!this.currentNode || !this.scanKeys) {
      throw new Error("transfer_wallet_prerequisites_missing");
    }

    const restoreScanHeight = Math.max(0, profile.scanHeight || 0);
    const maxRetries = 3;
    const initialDelayMs = 500;
    let lastError: Error | null = null;

    for (let attempt = 0; attempt < maxRetries; attempt += 1) {
      try {
        if (attempt > 0) {
          const delayMs = initialDelayMs * Math.pow(2, attempt - 1);
          await new Promise<void>((resolve) => setTimeout(resolve, delayMs));
        }

        const restored = await this.worker.restoreFromKeys({
          filename: "transfer-runtime.wallet",
          password: "__transfer_runtime__",
          privateSpendKey: this.scanKeys.privateSpendKey,
          privateViewKey: this.scanKeys.privateViewKey,
          scanHeight: restoreScanHeight,
          daemonHost: this.currentNode.host,
          daemonPort: this.currentNode.port,
          daemonSsl: this.currentNode.ssl,
          syncThreads: 1
        });

        this.transferWalletId = restored.walletId;
        return restored.walletId;
      } catch (error) {
        lastError = error instanceof Error ? error : new Error(String(error));
        const reason = lastError.message;

        if (attempt < maxRetries - 1 && /status code 0|connection|network|timeout|unreachable/i.test(reason)) {
          continue;
        }

        throw new Error(
          `restoreFromKeys_failed attempt=${attempt + 1}/${maxRetries} node=${this.currentNode.host}:${this.currentNode.port} ssl=${this.currentNode.ssl} scanHeight=${restoreScanHeight} reason=${reason}`
        );
      }
    }

    throw lastError || new Error("restoreFromKeys_failed_max_retries");
  }

  private async closeTransferWallet(): Promise<void> {
    const walletId = this.transferWalletId;
    this.transferWalletId = null;
    if (walletId !== null) {
      await this.worker.close(walletId).catch(() => undefined);
    }
  }

  public async clearScanKeys(): Promise<void> {
    await this.discardPreparedTransfer();
    await this.closeTransferWallet();
    this.scanKeys = null;
    this.scannerResetPending = true;
    await this.storage.clearScannerSnapshot().catch(() => undefined);
    if (this.vaultUnlocked) {
      await this.worker.vaultDelete("direct_rpc_scan_keys");
    }
  }

  public async initVault(password: string): Promise<void> {
    await this.worker.vaultInit(password);
    this.vaultUnlocked = true;
    let loadedScanKeys = false;
    const existing = await this.worker.vaultGet("direct_rpc_scan_keys");
    const value = (existing as { value?: string }).value;
    if (typeof value === "string" && value.length > 0) {
      try {
        const parsed = JSON.parse(value) as { privateSpendKey?: string; privateViewKey?: string };
        if (typeof parsed.privateSpendKey === "string" && typeof parsed.privateViewKey === "string") {
          this.scanKeys = {
            privateSpendKey: parsed.privateSpendKey,
            privateViewKey: parsed.privateViewKey
          };
          loadedScanKeys = true;
          this.scannerResetPending = true;
        }
      } catch {
        // Ignore corrupt vault entry.
      }
    }

    // If keys are restored while a session is already running, rewind persisted sync state
    // and immediately poll so scanner state is rebuilt from the configured scan start.
    if (loadedScanKeys) {
      const walletId = this.currentWalletId ?? (await this.storage.loadProfile())?.walletId;
      if (walletId) {
        await this.reinitializeScannerStateIfNeeded(walletId);
      }
      if (this.running) {
        await this.pollOnce();
      }
    }
  }

  public async lockVault(): Promise<void> {
    await this.discardPreparedTransfer();
    await this.closeTransferWallet();
    if (this.vaultUnlocked) {
      await this.worker.vaultLock();
      this.vaultUnlocked = false;
    }
  }

  public async getProfile(): Promise<DirectRpcSessionProfile | null> {
    return this.storage.loadProfile();
  }

  public async resetScanFromHeight(height: number, timestamp = 0): Promise<void> {
    const target = Math.max(0, Math.floor(height));
    const ts = 0;
    const profile = await this.storage.loadProfile();
    const walletId = this.currentWalletId ?? profile?.walletId;
    if (!walletId) {
      throw new Error("no_active_wallet_for_rescan");
    }
    await this.storage.clearSyncArtifacts();
    await this.storage.clearScannerSnapshot().catch(() => undefined);
    if (profile && profile.walletId === walletId) {
      await this.storage.saveProfile({
        ...profile,
        scanHeight: target,
        scanTimestamp: ts
      });
    }
    this.scannerResetPending = true;
    this.scanTimestampPrimed = false;
    await this.storage.saveCursor({
      walletId,
      height: target,
      updatedAt: Date.now()
    });
    await this.storage.saveStatus({
      running: this.running,
      walletId,
      nodeId: this.currentNodeId,
      activeNodeEndpoint: this.currentNode ? nodeKey(this.currentNode) : undefined,
      failoverCount: this.failoverCount,
      message: `direct_rpc_rescan_from_${target}`
    });
    if (this.running) {
      await this.pollOnce();
    }
  }

  public async fetchHeight(node: RpcNode): Promise<number> {
    const started = Date.now();
    try {
      const result = await this.rpc.call<{ count?: number }>(node, "getblockcount");
      const latency = Date.now() - started;
      await this.recordNodeScore(node, true, latency);
      return Number(result.count ?? 0);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      await this.recordNodeScore(node, false, undefined, message);
      throw error;
    }
  }

  private async pollOnce(): Promise<void> {
    if (!this.running || !this.currentWalletId || !this.currentNode) {
      return;
    }
    try {
      await this.syncWithNode(this.currentNode);
      return;
    } catch (error) {
      const baseError = error instanceof Error ? error.message : String(error);
      const failoverNode = await this.tryFailoverNode();
      if (!failoverNode) {
        await this.storage.saveStatus({
          running: true,
          walletId: this.currentWalletId,
          nodeId: this.currentNode.id,
          activeNodeEndpoint: nodeKey(this.currentNode),
          failoverCount: this.failoverCount,
          message: `direct_rpc_sync_error_${baseError}`
        });
        return;
      }
      this.currentNode = failoverNode;
      this.currentNodeId = failoverNode.id;
      this.failoverCount += 1;
      this.fetchMode = "unknown";
      await this.storage.saveStatus({
        running: true,
        walletId: this.currentWalletId,
        nodeId: failoverNode.id,
        activeNodeEndpoint: nodeKey(failoverNode),
        failoverCount: this.failoverCount,
        message: `direct_rpc_failover_to_${failoverNode.id}`
      });
      await this.syncWithNode(failoverNode);
    }
  }

  private selectPollIntervalMs(remainingBlocks?: number): number {
    if (typeof remainingBlocks === "number" && Number.isFinite(remainingBlocks) && remainingBlocks > SYNC_NEAR_TIP_BLOCKS) {
      return SYNC_POLL_INTERVAL_MS;
    }
    return SYNC_NEAR_TIP_INTERVAL_MS;
  }

  private scheduleNextPoll(intervalMs: number): void {
    if (!this.running) {
      return;
    }
    if (this.pollTimer) {
      clearTimeout(this.pollTimer);
      this.pollTimer = null;
    }
    this.pollTimer = setTimeout(() => {
      this.pollTick().catch(() => undefined);
    }, intervalMs);
  }

  private async pollTick(): Promise<void> {
    if (!this.running) {
      return;
    }
    await this.pollOnce().catch(() => undefined);
    const stats = await this.storage.loadSyncStats().catch(() => null);
    this.scheduleNextPoll(this.selectPollIntervalMs(stats?.remainingBlocks));
  }

  private async syncWithNode(node: RpcNode): Promise<void> {
    if (!this.currentWalletId) {
      return;
    }
    const daemonHeight = await this.fetchHeight(node);
    const profile = await this.storage.loadProfile();
    const currentCursor = await this.storage.loadCursor();
    const baseline = Math.max(0, profile?.scanHeight ?? 0);
    const cursor = currentCursor && currentCursor.walletId === this.currentWalletId ? currentCursor.height : baseline;
    const clampedCursor = Math.min(cursor, daemonHeight);

    const previousSummary = await this.storage.loadSummary();
    const previousOwnedSummary =
      previousSummary && previousSummary.walletId === this.currentWalletId && previousSummary.scanMode === "wallet_owned_outputs"
        ? previousSummary
        : null;

    let nextSyncedHeight = clampedCursor;
    let lastBatchStart: number | undefined;
    let lastBatchEnd: number | undefined;
    let lastBatchSize = 0;
    let rangeFetchOk = false;
    let methodsTried: string[] = [];
    let usedFetchMode: "walletsync" | "range" | "by_height" | "none" = "none";
    let lastError: string | undefined;
    let scannedTransactions = 0;
    let scannedOutputs = 0;
    let unspentOwnedOutputs = 0;
    let spentOwnedOutputs = 0;
    let unlockedBalanceAtomic = previousOwnedSummary?.unlockedBalanceAtomic ?? "0";
    let lockedBalanceAtomic = previousOwnedSummary?.lockedBalanceAtomic ?? "0";
    let scanMode: WalletSummary["scanMode"] = previousOwnedSummary ? "wallet_owned_outputs" : "headers_only";
    if (previousOwnedSummary) {
      scannedTransactions = previousOwnedSummary.scannedTransactions ?? scannedTransactions;
      scannedOutputs = previousOwnedSummary.scannedOutputs ?? scannedOutputs;
      unspentOwnedOutputs = previousOwnedSummary.unspentOwnedOutputs ?? unspentOwnedOutputs;
      spentOwnedOutputs = previousOwnedSummary.spentOwnedOutputs ?? spentOwnedOutputs;
    }
    const persistedSnapshot = this.scanKeys ? await this.storage.loadScannerSnapshot().catch(() => null) : null;
    const scannerSnapshot =
      persistedSnapshot && persistedSnapshot.walletId === this.currentWalletId
        ? persistedSnapshot.snapshot
        : undefined;
    let alignedCursor = clampedCursor;
    if (
      persistedSnapshot
      && persistedSnapshot.walletId === this.currentWalletId
      && previousOwnedSummary
      && persistedSnapshot.cursorHeight >= baseline
      && persistedSnapshot.cursorHeight < clampedCursor
    ) {
      alignedCursor = persistedSnapshot.cursorHeight;
    }
    const remaining = Math.max(0, daemonHeight - alignedCursor);
    const hasScannerSnapshot = typeof scannerSnapshot === "string" && scannerSnapshot.length > 0;
    const canRunResetScan = !this.scannerResetPending || hasScannerSnapshot || previousOwnedSummary === null;

    nextSyncedHeight = alignedCursor;
    if (remaining > 0) {
      lastBatchStart = alignedCursor + 1;
      lastBatchEnd = Math.min(lastBatchStart + this.walletSyncBatchSize - 1, daemonHeight);
      lastBatchSize = lastBatchEnd - lastBatchStart + 1;
      const walletSync = await this.tryFetchWalletSyncBatch(node, lastBatchStart, lastBatchSize);
      if (walletSync.ok) {
        usedFetchMode = "walletsync";
        rangeFetchOk = true;
        methodsTried = walletSync.methodsTried;
        scannedTransactions = walletSync.scannedTransactions;
        scannedOutputs = walletSync.scannedOutputs;
        lastError = walletSync.error;

        if (walletSync.items.length > 0) {
          await this.storage.appendHistory(
            walletSync.items.map((item) => ({
              walletId: this.currentWalletId as string,
              height: Number(item.blockHeight ?? 0),
              hash: typeof item.blockHash === "string" ? item.blockHash : undefined,
              timestamp: Number.isFinite(Number(item.blockTimestamp)) ? Number(item.blockTimestamp) : undefined,
              scannedAt: Date.now()
            }))
          );

          const highest = walletSync.items.reduce((max, item) => Math.max(max, Number(item.blockHeight ?? max)), alignedCursor);
          nextSyncedHeight = Math.min(highest, daemonHeight);
        } else {
          nextSyncedHeight = Math.min(lastBatchEnd, daemonHeight);
        }

        if (this.scanKeys && canRunResetScan) {
          try {
            const scanResult = await this.worker.scanSyncDataBalance({
              scannerId: this.currentWalletId,
              privateSpendKey: this.scanKeys.privateSpendKey,
              privateViewKey: this.scanKeys.privateViewKey,
              daemonHeight,
              reset: this.scannerResetPending,
              scannerSnapshot,
              items: walletSync.items
            });
            this.scannerResetPending = false;
            unlockedBalanceAtomic = scanResult.unlocked;
            lockedBalanceAtomic = scanResult.locked;
            unspentOwnedOutputs = scanResult.unspentOwnedOutputs;
            spentOwnedOutputs = scanResult.spentOwnedOutputs;
            scannedTransactions = scanResult.scannedTransactions;
            scannedOutputs = scanResult.scannedOutputs;
            scanMode = "wallet_owned_outputs";
            if (typeof scanResult.scannerSnapshot === "string" && scanResult.scannerSnapshot.length > 0) {
              await this.storage.saveScannerSnapshot(this.currentWalletId as string, scanResult.scannerSnapshot, nextSyncedHeight);
            }
            if (Array.isArray(scanResult.transactions) && scanResult.transactions.length > 0) {
              await this.storage.appendTransactionHistory(
                scanResult.transactions.map((entry) => ({
                  walletId: this.currentWalletId as string,
                  txHash: entry.txHash,
                  blockHeight: entry.blockHeight,
                  blockTimestamp: entry.blockTimestamp,
                  paymentId: entry.paymentId,
                  incomingAtomic: entry.incomingAtomic,
                  outgoingAtomic: entry.outgoingAtomic,
                  netAtomic: entry.netAtomic,
                  direction: entry.direction,
                  scannedAt: Date.now()
                }))
              );
            }
          } catch (error) {
            lastError = error instanceof Error ? error.message : String(error);
          }
        } else if (this.scanKeys && !canRunResetScan) {
          // Keep prior computed balances and avoid resetting scanner with partial history only.
          lastError = lastError ?? "scanner_snapshot_missing_preserving_balance";
        } else if (previousOwnedSummary) {
          // Preserve prior computed wallet-owned balances until scan keys are reloaded.
          lastError = lastError ?? "scan_keys_not_loaded";
        }
      } else {
        const fetchResult = await this.tryFetchHeaderBatch(node, lastBatchStart, lastBatchEnd);
        rangeFetchOk = fetchResult.ok;
        methodsTried = [...walletSync.methodsTried, ...fetchResult.methodsTried];
        usedFetchMode = fetchResult.mode;
        lastError = walletSync.error ?? fetchResult.error;
        if (fetchResult.headers.length > 0) {
          await this.storage.appendHistory(
            fetchResult.headers.map((header) => ({
              walletId: this.currentWalletId as string,
              height: Number(header.height ?? 0),
              hash: typeof header.hash === "string" ? header.hash : undefined,
              timestamp: Number.isFinite(Number(header.timestamp)) ? Number(header.timestamp) : undefined,
              reward: header.reward !== undefined ? String(header.reward) : undefined,
              scannedAt: Date.now()
            }))
          );
        }
        nextSyncedHeight = lastBatchEnd;
      }
    }
    else if (
      this.scanKeys
      && previousOwnedSummary !== null
      && canRunResetScan
    ) {
      try {
        const scanResult = await this.worker.scanSyncDataBalance({
          scannerId: this.currentWalletId,
          privateSpendKey: this.scanKeys.privateSpendKey,
          privateViewKey: this.scanKeys.privateViewKey,
          daemonHeight,
          reset: this.scannerResetPending,
          scannerSnapshot,
          items: []
        });
        this.scannerResetPending = false;
        unlockedBalanceAtomic = scanResult.unlocked;
        lockedBalanceAtomic = scanResult.locked;
        unspentOwnedOutputs = scanResult.unspentOwnedOutputs;
        spentOwnedOutputs = scanResult.spentOwnedOutputs;
        scannedTransactions = scanResult.scannedTransactions;
        scannedOutputs = scanResult.scannedOutputs;
        scanMode = "wallet_owned_outputs";
        if (typeof scanResult.scannerSnapshot === "string" && scanResult.scannerSnapshot.length > 0) {
          await this.storage.saveScannerSnapshot(this.currentWalletId as string, scanResult.scannerSnapshot, nextSyncedHeight);
        }
      } catch (error) {
        lastError = error instanceof Error ? error.message : String(error);
      }
    }

    const historyCount = (await this.storage.loadHistory()).length;
    const summary: WalletSummary = {
      walletId: this.currentWalletId,
      daemonHeight,
      syncedHeight: nextSyncedHeight,
      unlockedBalanceAtomic,
      lockedBalanceAtomic,
      scanMode,
      scannedTransactions,
      scannedOutputs,
      unspentOwnedOutputs,
      spentOwnedOutputs,
      scannedHeaderCount: historyCount
    };

    await this.storage.saveSummary(summary);
    await this.storage.saveCursor({
      walletId: this.currentWalletId,
      height: nextSyncedHeight,
      updatedAt: Date.now()
    });
    await this.storage.saveSyncStats({
      walletId: this.currentWalletId,
      targetHeight: daemonHeight,
      syncedHeight: nextSyncedHeight,
      remainingBlocks: Math.max(0, daemonHeight - nextSyncedHeight),
      lastBatchStart,
      lastBatchEnd,
      lastBatchSize,
      fetchMode: usedFetchMode,
      rangeFetchOk,
      methodsTried,
      lastError,
      updatedAt: Date.now()
    });
    await this.storage.saveStatus({
      running: true,
      walletId: this.currentWalletId,
      nodeId: node.id,
      activeNodeEndpoint: nodeKey(node),
      failoverCount: this.failoverCount,
      message: `direct_rpc_sync_${nextSyncedHeight}_of_${daemonHeight}`
    });
  }

  private async ensureCursorInitialized(walletId: string): Promise<void> {
    const profile = await this.storage.loadProfile();
    const startHeight = Math.max(0, profile?.walletId === walletId ? profile.scanHeight : 0);
    const existing = await this.storage.loadCursor();
    if (existing && existing.walletId === walletId && existing.height >= startHeight) {
      return;
    }
    if (existing && existing.walletId === walletId && existing.height < startHeight) {
      await this.storage.clearSyncArtifacts();
    }
    await this.storage.saveCursor({
      walletId,
      height: startHeight,
      updatedAt: Date.now()
    });
  }

  private async reinitializeScannerStateIfNeeded(walletId: string): Promise<void> {
    if (!this.scannerResetPending || !this.scanKeys) {
      return;
    }
    const profile = await this.storage.loadProfile();
    if (!profile || profile.walletId !== walletId) {
      return;
    }
    const summary = await this.storage.loadSummary();
    const cursor = await this.storage.loadCursor();
    const snapshot = await this.storage.loadScannerSnapshot();
    const startHeight = Math.max(0, profile.scanHeight || 0);
    const hasOwnedOutputState = summary?.walletId === walletId && summary.scanMode === "wallet_owned_outputs";
    const cursorAheadOfStart = Boolean(cursor && cursor.walletId === walletId && cursor.height > startHeight);
    const hasSnapshot = Boolean(snapshot && snapshot.walletId === walletId && snapshot.snapshot.length > 0);
    if (!hasOwnedOutputState || !cursorAheadOfStart) {
      return;
    }
    if (hasSnapshot) {
      // Scanner state can be restored from persisted snapshot; no rewind needed.
      return;
    }

    // No persisted scanner snapshot available. Keep cached summary/cursor to avoid
    // balance dropping to zero on reload/offline, and rebuild scanner state lazily.
    return;
  }

  private async tryFetchWalletSyncBatch(
    node: RpcNode,
    startHeight: number,
    blockCount: number
  ): Promise<{
    ok: boolean;
    methodsTried: string[];
    items: Array<Record<string, unknown>>;
    scannedTransactions: number;
    scannedOutputs: number;
    error?: string;
  }> {
    const methodsTried: string[] = [];
    const profile = await this.storage.loadProfile();
    const skipCoinbaseTransactions = profile?.scanFromCoinbase === true ? false : true;
    const startTimestamp = 0;
    const candidateCounts = Array.from(
      new Set([blockCount, 200, 100, 50, 25, 10, 5, 1].filter((count) => count <= blockCount && count >= MIN_SYNC_BATCH_SIZE))
    ).sort((a, b) => b - a);
    if (candidateCounts.length === 0) {
      candidateCounts.push(Math.max(MIN_SYNC_BATCH_SIZE, blockCount));
    }
    let lastError: string | undefined;
    for (const candidateCount of candidateCounts) {
      try {
        methodsTried.push(`/getwalletsyncdata:blockCount=${candidateCount}`);
        const response = await this.rpc.postPath<WalletSyncDataResponse>(node, "/getwalletsyncdata", {
          startHeight,
          startTimestamp,
          blockCount: candidateCount,
          skipCoinbaseTransactions,
          blockHashCheckpoints: []
        });

        if (response.status !== "OK") {
          lastError = `walletsync_status_${response.status ?? "unknown"}`;
          continue;
        }

        const items = Array.isArray(response.items) ? response.items : [];
        let scannedTransactions = 0;
        let scannedOutputs = 0;
        for (const block of items) {
          const coinbase = block.coinbaseTX as { outputs?: unknown[] } | undefined;
          if (coinbase) {
            scannedTransactions += 1;
            scannedOutputs += Array.isArray(coinbase.outputs) ? coinbase.outputs.length : 0;
          }
          const txs = Array.isArray(block.transactions) ? block.transactions : [];
          scannedTransactions += txs.length;
          for (const tx of txs) {
            const outputs = (tx as { outputs?: unknown[] }).outputs;
            scannedOutputs += Array.isArray(outputs) ? outputs.length : 0;
          }
        }

        this.walletSyncBatchSize = candidateCount;
        return {
          ok: true,
          methodsTried,
          items,
          scannedTransactions,
          scannedOutputs
        };
      } catch (error) {
        const reason = error instanceof Error ? error.message : String(error);
        methodsTried.push(`/getwalletsyncdata:blockCount=${candidateCount}:${reason}`);
        lastError = reason;
        if (!reason.includes("rpc_http_400")) {
          break;
        }
      }
    }
    return {
      ok: false,
      methodsTried,
      items: [],
      scannedTransactions: 0,
      scannedOutputs: 0,
      error: lastError
    };
  }

  private async tryFetchHeaderBatch(
    node: RpcNode,
    startHeight: number,
    endHeight: number
  ): Promise<{
    ok: boolean;
    mode: "range" | "by_height" | "none";
    methodsTried: string[];
    headers: Array<Record<string, unknown>>;
    error?: string;
  }> {
    const methodsTried: string[] = [];
    const capabilities = (await this.getNodeCapabilities(node)) ?? (await this.refreshNodeCapabilities(node));

    if (!capabilities.supportsGetBlockHeadersRange && !capabilities.supportsGetBlockHeaderByHeight) {
      return {
        ok: false,
        mode: "none",
        methodsTried: ["header_methods_unavailable"],
        headers: [],
        error: "header_methods_unavailable"
      };
    }

    if (!capabilities.supportsGetBlockHeadersRange && capabilities.supportsGetBlockHeaderByHeight) {
      this.fetchMode = "by_height";
    }

    if (this.fetchMode === "range") {
      try {
        const headers = await this.tryFetchBlockHeaderRange(node, startHeight, endHeight);
        return { ok: true, mode: "range", methodsTried: ["getblockheadersrange"], headers };
      } catch (error) {
        this.fetchMode = "unknown";
        const reason = error instanceof Error ? error.message : String(error);
        methodsTried.push(`getblockheadersrange:${reason}`);
      }
    }

    if (this.fetchMode === "unknown") {
      try {
        const headers = await this.tryFetchBlockHeaderRange(node, startHeight, endHeight);
        this.fetchMode = "range";
        return { ok: true, mode: "range", methodsTried: [...methodsTried, "getblockheadersrange"], headers };
      } catch (error) {
        const reason = error instanceof Error ? error.message : String(error);
        methodsTried.push(`getblockheadersrange:${reason}`);
      }
    }

    try {
      const headers = await this.tryFetchBlockHeaderByHeight(node, startHeight, endHeight);
      this.fetchMode = "by_height";
      await this.storage.saveNodeCapabilities({
        ...(capabilities ?? (await this.refreshNodeCapabilities(node))),
        supportsGetBlockHeaderByHeight: true,
        checkedAt: Date.now()
      });
      return { ok: true, mode: "by_height", methodsTried: [...methodsTried, "getblockheaderbyheight"], headers };
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      this.fetchMode = "none";
      await this.storage.saveNodeCapabilities({
        ...(capabilities ?? (await this.refreshNodeCapabilities(node))),
        supportsGetBlockHeadersRange: false,
        supportsGetBlockHeaderByHeight: false,
        checkedAt: Date.now(),
        lastError: reason
      });
      return {
        ok: false,
        mode: "none",
        methodsTried: [...methodsTried, `getblockheaderbyheight:${reason}`],
        headers: [],
        error: reason
      };
    }
  }

  private async tryFetchBlockHeaderRange(
    node: RpcNode,
    startHeight: number,
    endHeight: number
  ): Promise<Array<Record<string, unknown>>> {
    let response: unknown;
    try {
      response = await this.rpc.call(node, "getblockheadersrange", { startHeight, endHeight });
    } catch {
      response = await this.rpc.call(node, "getblockheadersrange", { start_height: startHeight, end_height: endHeight });
    }
    const parsed = response as { headers?: Array<Record<string, unknown>> };
    return Array.isArray(parsed.headers) ? parsed.headers : [];
  }

  private async tryFetchBlockHeaderByHeight(
    node: RpcNode,
    startHeight: number,
    endHeight: number
  ): Promise<Array<Record<string, unknown>>> {
    const headers: Array<Record<string, unknown>> = [];
    for (let height = startHeight; height <= endHeight; height += 1) {
      let response: unknown;
      try {
        response = await this.rpc.call(node, "getblockheaderbyheight", { height });
      } catch {
        response = await this.rpc.call(node, "getblockheaderbyheight", { height: String(height) });
      }
      const parsed = response as { block_header?: Record<string, unknown> };
      if (parsed.block_header) {
        headers.push(parsed.block_header);
      }
    }
    return headers;
  }

  private async tryFailoverNode(): Promise<RpcNode | null> {
    if (!this.currentNode) {
      return null;
    }
    const currentKey = nodeKey(this.currentNode);
    const candidates = await this.rankNodesForFailover(this.nodePool.filter((n) => nodeKey(n) !== currentKey));
    for (const candidate of candidates) {
      try {
        await this.fetchHeight(candidate);
        return candidate;
      } catch {
        continue;
      }
    }
    return null;
  }

  private async rankNodesForFailover(candidates: RpcNode[]): Promise<RpcNode[]> {
    const scores = await this.storage.loadNodeScores();
    const now = Date.now();
    const scoreMap = new Map(scores.map((s) => [s.nodeKey, s]));
    const eligible = candidates.filter((node) => {
      const score = scoreMap.get(nodeKey(node));
      if (!score?.cooldownUntil) {
        return true;
      }
      return score.cooldownUntil <= now;
    });
    return candidates
      .slice()
      .filter((node) => eligible.some((ok) => nodeKey(ok) === nodeKey(node)))
      .slice()
      .sort((a, b) => {
        const sa = scoreMap.get(nodeKey(a));
        const sb = scoreMap.get(nodeKey(b));
        const aval = this.effectiveScore(sa, now);
        const bval = this.effectiveScore(sb, now);
        if (bval !== aval) {
          return bval - aval;
        }
        return a.priority - b.priority;
      });
  }

  private effectiveScore(score: NodeScore | undefined, now: number): number {
    if (!score) {
      return -999;
    }
    const reference = score.lastSuccessAt ?? score.lastFailureAt ?? now;
    const elapsed = Math.max(0, now - reference);
    const decaySteps = Math.floor(elapsed / NODE_SCORE_DECAY_INTERVAL_MS);
    const decayed = score.score - decaySteps * NODE_SCORE_DECAY_STEP;
    return Math.max(-1000, decayed);
  }

  private async recordNodeScore(
    node: RpcNode,
    success: boolean,
    latencyMs?: number,
    lastError?: string
  ): Promise<void> {
    const key = nodeKey(node);
    const now = Date.now();
    const existing = (await this.storage.loadNodeScores()).find((item) => item.nodeKey === key);
    const successCount = (existing?.successCount ?? 0) + (success ? 1 : 0);
    const failureCount = (existing?.failureCount ?? 0) + (success ? 0 : 1);
    const penalty = failureCount * 15;
    const latencyPenalty = latencyMs ? Math.floor(latencyMs / 50) : 0;
    const reward = successCount * 2;
    const baseScore = this.effectiveScore(existing, now);
    const score = Math.max(-1000, baseScore + (success ? 6 : -20) + reward - penalty - latencyPenalty);
    const cooldownUntil = success ? undefined : now + NODE_FAILURE_COOLDOWN_MS;

    const next: NodeScore = {
      nodeKey: key,
      nodeId: node.id,
      lastLatencyMs: latencyMs ?? existing?.lastLatencyMs,
      successCount,
      failureCount,
      lastSuccessAt: success ? now : existing?.lastSuccessAt,
      lastFailureAt: success ? existing?.lastFailureAt : now,
      cooldownUntil,
      lastError: success ? undefined : lastError,
      score
    };
    await this.storage.saveNodeScore(next);
  }
}
