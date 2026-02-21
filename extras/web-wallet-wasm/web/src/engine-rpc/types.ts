export type WalletEngineMode = "wasm_socket" | "direct_rpc";

export interface RpcNode {
  id: string;
  host: string;
  port: number;
  ssl: boolean;
  priority: number;
}

export interface NodeProbeResult {
  ok: boolean;
  nodeId: string;
  endpoint: string;
  latencyMs?: number;
  supportsCors: boolean;
  supportsRequiredMethods: boolean;
  reason?: string;
  height?: number;
}

export interface NodeCapabilities {
  nodeKey: string;
  checkedAt: number;
  supportsGetBlockCount: boolean;
  supportsWalletSyncData: boolean;
  supportsGetBlockHeadersRange: boolean;
  supportsGetBlockHeaderByHeight: boolean;
  corsLikely: boolean;
  lastError?: string;
}

export interface NodeScore {
  nodeKey: string;
  nodeId: string;
  lastLatencyMs?: number;
  successCount: number;
  failureCount: number;
  lastSuccessAt?: number;
  lastFailureAt?: number;
  cooldownUntil?: number;
  lastError?: string;
  score: number;
}

export interface SyncCursor {
  walletId: string;
  height: number;
  blockHash?: string;
  updatedAt: number;
}

export interface WalletSummary {
  walletId: string;
  address?: string;
  syncedHeight: number;
  daemonHeight: number;
  unlockedBalanceAtomic: string;
  lockedBalanceAtomic: string;
  scanMode?: "wallet_owned_outputs" | "headers_only";
  scannedTransactions?: number;
  scannedOutputs?: number;
  unspentOwnedOutputs?: number;
  spentOwnedOutputs?: number;
  scannedHeaderCount?: number;
  lastScannedHeaderTimestamp?: number;
}

export interface WalletTxHistoryEntry {
  walletId: string;
  txHash: string;
  blockHeight: number;
  blockTimestamp?: number;
  paymentId?: string;
  incomingAtomic: string;
  outgoingAtomic: string;
  netAtomic: string;
  direction: "incoming" | "outgoing" | "self";
  scannedAt: number;
}

export type DirectRpcSessionKind = "create" | "import_seed" | "import_keys";

export interface DirectRpcSessionProfile {
  walletId: string;
  kind: DirectRpcSessionKind;
  filename: string;
  address?: string;
  scanHeight: number;
  scanTimestamp?: number;
  createdAt: number;
  sourceFingerprint?: string;
  hasMnemonicSeed: boolean;
  hasPrivateKeys: boolean;
  scanFromCoinbase?: boolean;
}

export interface RpcEngineStatus {
  running: boolean;
  walletId?: string;
  message?: string;
  nodeId?: string;
  activeNodeEndpoint?: string;
  failoverCount?: number;
}

export interface SyncRuntimeStats {
  walletId: string;
  targetHeight: number;
  syncedHeight: number;
  remainingBlocks: number;
  lastBatchStart?: number;
  lastBatchEnd?: number;
  lastBatchSize: number;
  fetchMode: "walletsync" | "range" | "by_height" | "none";
  rangeFetchOk: boolean;
  methodsTried: string[];
  lastError?: string;
  updatedAt: number;
}

export interface ScannedHeaderEntry {
  walletId: string;
  height: number;
  hash?: string;
  timestamp?: number;
  reward?: string;
  scannedAt: number;
}

export interface RpcClientOptions {
  timeoutMs?: number;
  retries?: number;
}

export interface RpcCallResult<T = unknown> {
  jsonrpc: string;
  id: string;
  result?: T;
  error?: {
    code: number;
    message: string;
    data?: unknown;
  };
}

export interface DirectRpcEngine {
  configureNodePool(nodes: RpcNode[], preferredNodeId?: string): Promise<void>;
  probeNode(node: RpcNode): Promise<NodeProbeResult>;
  start(walletId: string, node: RpcNode): Promise<RpcEngineStatus>;
  stop(): Promise<void>;
  getStatus(): Promise<RpcEngineStatus>;
  getSummary(): Promise<WalletSummary | null>;
  getHistory(limit?: number): Promise<ScannedHeaderEntry[]>;
  getTransactionHistory(limit?: number): Promise<WalletTxHistoryEntry[]>;
  getCursor(): Promise<SyncCursor | null>;
  getSyncStats(): Promise<SyncRuntimeStats | null>;
  getNodeCapabilities(node: RpcNode): Promise<NodeCapabilities | null>;
  refreshNodeCapabilities(node: RpcNode): Promise<NodeCapabilities>;
  getNodeScores(): Promise<NodeScore[]>;
  setScanKeys(privateSpendKey: string, privateViewKey: string): Promise<void>;
  deriveScanKeysFromSeed(mnemonicSeed: string): Promise<{ privateSpendKey: string; privateViewKey: string; mnemonicSeed: string; address: string }>;
  generateScanKeys(): Promise<{ privateSpendKey: string; privateViewKey: string; mnemonicSeed: string; address: string }>;
  deriveAddressFromKeys(privateSpendKey: string, privateViewKey: string): Promise<string>;
  validateAddress(address: string, allowIntegrated?: boolean): Promise<{ valid: boolean; reason?: string }>;
  createIntegratedAddress(address: string, paymentId: string): Promise<{ integratedAddress: string; error?: string }>;
  sendBasicTransfer(request: {
    destination: string;
    amountAtomic: string;
    paymentId?: string;
    password: string;
    filenameHint?: string;
  }): Promise<{ txHash: string }>;
  clearScanKeys(): Promise<void>;
  initVault(password: string): Promise<void>;
  lockVault(): Promise<void>;
  setProfile(profile: DirectRpcSessionProfile): Promise<void>;
  clearProfile(): Promise<void>;
  getProfile(): Promise<DirectRpcSessionProfile | null>;
  resetScanFromHeight(height: number, timestamp?: number): Promise<void>;
}
