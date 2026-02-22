import type { WorkerCommand, WorkerReply } from "./workerMessages";
import type {
  ImportFromKeysRequest,
  ImportFromSeedRequest,
  OpenWalletRequest,
  AddressValidationResult,
  IntegratedAddressResult,
  DerivedWalletKeys,
  ScanSyncDataBalanceRequest,
  ScanSyncDataBalanceResult,
  WalletBackupSecrets,
  WalletCreateResult,
  WasmResponse
} from "./types";

const WORKER_REQUEST_TIMEOUT_MS = 120000;

type InflightRequest = {
  resolve: (reply: WorkerReply) => void;
  reject: (error: Error) => void;
  timeoutId: ReturnType<typeof setTimeout>;
};

function createRequestId(): string {
  if (typeof crypto !== "undefined" && typeof crypto.randomUUID === "function") {
    return crypto.randomUUID();
  }
  if (typeof crypto !== "undefined" && typeof crypto.getRandomValues === "function") {
    const bytes = new Uint8Array(16);
    crypto.getRandomValues(bytes);
    const hex = Array.from(bytes, (b) => b.toString(16).padStart(2, "0")).join("");
    return `req-${hex}`;
  }
  return `req-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

export class WalletWorkerClient {
  private readonly worker: Worker;
  private readonly inflight = new Map<string, InflightRequest>();

  public constructor() {
    this.worker = new Worker(new URL("../../workers/wallet.worker.ts", import.meta.url), { type: "module" });
    this.worker.onmessage = (event: MessageEvent<WorkerReply>) => {
      const reply = event.data;
      const pending = this.inflight.get(reply.id);
      if (!pending) {
        return;
      }
      this.inflight.delete(reply.id);
      clearTimeout(pending.timeoutId);
      pending.resolve(reply);
    };
    this.worker.onerror = (event: ErrorEvent) => {
      this.rejectAll(new Error(event.message || "Worker crashed"));
    };
    this.worker.onmessageerror = () => {
      this.rejectAll(new Error("Worker message deserialization failed"));
    };
  }

  public async apiVersion(): Promise<unknown> {
    return this.sendWasm({ id: createRequestId(), command: "apiVersion" });
  }

  public async create(payload: OpenWalletRequest): Promise<WalletCreateResult> {
    return this.sendWasm<WalletCreateResult>({ id: createRequestId(), command: "create", payload });
  }

  public async restoreFromSeed(payload: ImportFromSeedRequest): Promise<WalletCreateResult> {
    return this.sendWasm<WalletCreateResult>({ id: createRequestId(), command: "restoreFromSeed", payload });
  }

  public async restoreFromKeys(payload: ImportFromKeysRequest): Promise<WalletCreateResult> {
    return this.sendWasm<WalletCreateResult>({ id: createRequestId(), command: "restoreFromKeys", payload });
  }

  public async backupSecrets(walletId: number): Promise<WalletBackupSecrets> {
    return this.sendWasm<WalletBackupSecrets>({ id: createRequestId(), command: "backupSecrets", payload: { walletId } });
  }

  public async swapNode(walletId: number, daemonHost: string, daemonPort: number, daemonSsl: boolean): Promise<unknown> {
    return this.sendWasm({ id: createRequestId(), command: "swapNode", payload: { walletId, daemonHost, daemonPort, daemonSsl } });
  }

  public async close(walletId: number): Promise<unknown> {
    return this.sendWasm({ id: createRequestId(), command: "close", payload: { walletId } });
  }

  public async status(walletId: number): Promise<Record<string, unknown>> {
    return this.sendWasm<Record<string, unknown>>({ id: createRequestId(), command: "status", payload: { walletId } });
  }

  public async deriveKeysFromSeed(mnemonicSeed: string): Promise<DerivedWalletKeys> {
    return this.sendWasm<DerivedWalletKeys>({
      id: createRequestId(),
      command: "deriveKeysFromSeed",
      payload: { mnemonicSeed }
    });
  }

  public async generateSeedKeys(): Promise<DerivedWalletKeys> {
    return this.sendWasm<DerivedWalletKeys>({
      id: createRequestId(),
      command: "generateSeedKeys"
    });
  }

  public async deriveAddressFromKeys(privateSpendKey: string, privateViewKey: string): Promise<{ address: string }> {
    return this.sendWasm<{ address: string }>({
      id: createRequestId(),
      command: "deriveAddressFromKeys",
      payload: { privateSpendKey, privateViewKey }
    });
  }

  public async validateAddress(address: string, allowIntegrated = true): Promise<AddressValidationResult> {
    return this.sendWasm<AddressValidationResult>({
      id: createRequestId(),
      command: "validateAddress",
      payload: { address, allowIntegrated }
    });
  }

  public async createIntegratedAddress(address: string, paymentId: string): Promise<IntegratedAddressResult> {
    return this.sendWasm<IntegratedAddressResult>({
      id: createRequestId(),
      command: "createIntegratedAddress",
      payload: { address, paymentId }
    });
  }

  public async sendBasic(
    walletId: number,
    destination: string,
    amountAtomic: string,
    paymentId = ""
  ): Promise<{ txHash: string }> {
    return this.sendWasm<{ txHash: string }>({
      id: createRequestId(),
      command: "sendBasic",
      payload: { walletId, destination, amountAtomic, paymentId }
    });
  }

  public async prepareBasicTransfer(
    walletId: number,
    destination: string,
    amountAtomic: string,
    paymentId = ""
  ): Promise<{
    preparedTxHash: string;
    feeAtomic: string;
    networkHeight?: number;
    txPowRequired?: boolean;
    txPowPassFeeAtomic?: string;
    txPowPassHeight?: number;
  }> {
    return this.sendWasm<{
      preparedTxHash: string;
      feeAtomic: string;
      networkHeight?: number;
      txPowRequired?: boolean;
      txPowPassFeeAtomic?: string;
      txPowPassHeight?: number;
    }>({
      id: createRequestId(),
      command: "prepareBasicTransfer",
      payload: { walletId, destination, amountAtomic, paymentId }
    });
  }

  public async sendPreparedTransfer(walletId: number, preparedTxHash: string): Promise<{ txHash: string }> {
    return this.sendWasm<{ txHash: string }>({
      id: createRequestId(),
      command: "sendPreparedTransfer",
      payload: { walletId, preparedTxHash }
    });
  }

  public async estimateBasicFee(
    walletId: number,
    destination: string,
    amountAtomic: string,
    paymentId = ""
  ): Promise<{ feeAtomic: string }> {
    return this.sendWasm<{ feeAtomic: string }>({
      id: createRequestId(),
      command: "estimateBasicFee",
      payload: { walletId, destination, amountAtomic, paymentId }
    });
  }

  public async scanSyncDataBalance(payload: ScanSyncDataBalanceRequest): Promise<ScanSyncDataBalanceResult> {
    return this.sendWasm<ScanSyncDataBalanceResult>({ id: createRequestId(), command: "scanSyncDataBalance", payload });
  }

  public async vaultInit(password: string): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultInit", payload: { password } });
  }

  public async vaultStatus(): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultStatus" });
  }

  public async vaultPut(key: string, value: string): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultPut", payload: { key, value } });
  }

  public async vaultGet(key: string): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultGet", payload: { key } });
  }

  public async vaultDelete(key: string): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultDelete", payload: { key } });
  }

  public async vaultLock(): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultLock" });
  }

  public async vaultReset(): Promise<unknown> {
    return this.send({ id: createRequestId(), command: "vaultReset" });
  }

  private rejectAll(error: Error): void {
    const inflight = Array.from(this.inflight.values());
    this.inflight.clear();
    for (const pending of inflight) {
      clearTimeout(pending.timeoutId);
      pending.reject(error);
    }
  }

  private async sendWasm<T = unknown>(message: WorkerCommand): Promise<T> {
    const result = await this.send(message);
    const response = result as WasmResponse<T> & { reason?: unknown };
    if (!response.ok) {
      const details = typeof response.data === "object" && response.data !== null ? response.data as Record<string, unknown> : {};
      const topLevelReason = typeof response.reason === "string" ? response.reason : undefined;
      const dataReason = typeof details.reason === "string" ? details.reason : undefined;
      const reason = topLevelReason ?? dataReason;
      throw new Error(reason ? `${response.error ?? "WASM command failed"} (${reason})` : (response.error ?? "WASM command failed"));
    }
    return (response.data as T) ?? ({} as T);
  }

  private async send(message: WorkerCommand): Promise<unknown> {
    const reply = await new Promise<WorkerReply>((resolve, reject) => {
      const timeoutId = setTimeout(() => {
        this.inflight.delete(message.id);
        reject(new Error(`Worker command "${message.command}" timed out after ${WORKER_REQUEST_TIMEOUT_MS}ms`));
      }, WORKER_REQUEST_TIMEOUT_MS);

      this.inflight.set(message.id, { resolve, reject, timeoutId });
      this.worker.postMessage(message);
    });

    if (!reply.ok) {
      throw new Error(reply.error ?? "Worker command failed");
    }

    return reply.result;
  }
}
