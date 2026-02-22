export interface NodeEndpoint {
  id: string;
  host: string;
  port: number;
  ssl: boolean;
  priority: number;
}

export function normalizeNodeHost(input: string): string {
  let value = input.trim();
  value = value.replace(/^(https?|wss?):\/\//i, "");
  value = value.split(/[/?#]/, 1)[0];

  if (value.startsWith("[")) {
    const end = value.indexOf("]");
    if (end > 1) {
      return value.slice(1, end);
    }
  }

  const colonCount = (value.match(/:/g) ?? []).length;
  if (colonCount === 1) {
    const [hostPart, portPart] = value.split(":");
    if (portPart && /^\d+$/.test(portPart)) {
      return hostPart;
    }
  }

  return value;
}

export function buildNodeRpcUrl(node: Pick<NodeEndpoint, "host" | "port" | "ssl">): string {
  const protocol = node.ssl ? "wss" : "ws";
  const defaultPort = node.ssl ? 443 : 80;
  const portSuffix = node.port === defaultPort ? "" : `:${node.port}`;
  return `${protocol}://${node.host}${portSuffix}`;
}

export function buildNodeHttpUrl(node: Pick<NodeEndpoint, "host" | "port" | "ssl">): string {
  const protocol = node.ssl ? "https" : "http";
  const defaultPort = node.ssl ? 443 : 80;
  const portSuffix = node.port === defaultPort ? "" : `:${node.port}`;
  return `${protocol}://${node.host}${portSuffix}`;
}

export interface WasmResponse<T = unknown> {
  ok: boolean;
  code?: number;
  error?: string;
  data?: T;
}

export interface OpenWalletRequest {
  filename: string;
  password: string;
  daemonHost: string;
  daemonPort: number;
  daemonSsl: boolean;
  syncThreads?: number;
}

export interface ImportFromSeedRequest extends OpenWalletRequest {
  mnemonicSeed: string;
  scanHeight: number;
}

export interface ImportFromKeysRequest extends OpenWalletRequest {
  privateSpendKey: string;
  privateViewKey: string;
  scanHeight: number;
}

export interface WalletCreateResult {
  walletId: number;
}

export interface WalletBackupSecrets {
  address: string;
  mnemonicSeed: string;
  privateViewKey: string;
  privateSpendKey: string;
}

export interface ScanSyncDataBalanceRequest {
  scannerId: string;
  privateSpendKey: string;
  privateViewKey: string;
  daemonHeight: number;
  reset?: boolean;
  scannerSnapshot?: string;
  items: unknown[];
}

export interface ScanSyncDataBalanceResult {
  unlocked: string;
  locked: string;
  unspentOwnedOutputs: number;
  spentOwnedOutputs: number;
  scannedBlocks: number;
  scannedTransactions: number;
  scannedOutputs: number;
  scannerSnapshot?: string;
  transactions?: Array<{
    txHash: string;
    blockHeight: number;
    blockTimestamp?: number;
    paymentId?: string;
    incomingAtomic: string;
    outgoingAtomic: string;
    netAtomic: string;
    direction: "incoming" | "outgoing" | "self";
  }>;
}

export interface DerivedWalletKeys {
  mnemonicSeed: string;
  privateSpendKey: string;
  privateViewKey: string;
  address: string;
}

export interface AddressValidationResult {
  valid: boolean;
  reason?: string;
}

export interface IntegratedAddressResult {
  integratedAddress: string;
  error?: string;
}

export interface BackendLogEntry {
  pretty?: string;
  message?: string;
  level?: string;
  categories?: string[];
  ts?: number;
}
