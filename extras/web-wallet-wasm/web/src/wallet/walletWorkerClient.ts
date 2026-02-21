import type { WorkerCommand, WorkerReply } from "./workerMessages";
import type { ImportFromKeysRequest, ImportFromSeedRequest, OpenWalletRequest, WalletBackupSecrets, WalletCreateResult, WasmResponse } from "./types";

const WORKER_REQUEST_TIMEOUT_MS = 20000;

type InflightRequest = {
  resolve: (reply: WorkerReply) => void;
  reject: (error: Error) => void;
  timeoutId: ReturnType<typeof setTimeout>;
};

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
    return this.sendWasm({ id: crypto.randomUUID(), command: "apiVersion" });
  }

  public async create(payload: OpenWalletRequest): Promise<WalletCreateResult> {
    return this.sendWasm<WalletCreateResult>({ id: crypto.randomUUID(), command: "create", payload });
  }

  public async restoreFromSeed(payload: ImportFromSeedRequest): Promise<WalletCreateResult> {
    return this.sendWasm<WalletCreateResult>({ id: crypto.randomUUID(), command: "restoreFromSeed", payload });
  }

  public async restoreFromKeys(payload: ImportFromKeysRequest): Promise<WalletCreateResult> {
    return this.sendWasm<WalletCreateResult>({ id: crypto.randomUUID(), command: "restoreFromKeys", payload });
  }

  public async backupSecrets(walletId: number): Promise<WalletBackupSecrets> {
    return this.sendWasm<WalletBackupSecrets>({ id: crypto.randomUUID(), command: "backupSecrets", payload: { walletId } });
  }

  public async swapNode(walletId: number, daemonHost: string, daemonPort: number, daemonSsl: boolean): Promise<unknown> {
    return this.sendWasm({ id: crypto.randomUUID(), command: "swapNode", payload: { walletId, daemonHost, daemonPort, daemonSsl } });
  }

  public async close(walletId: number): Promise<unknown> {
    return this.sendWasm({ id: crypto.randomUUID(), command: "close", payload: { walletId } });
  }

  public async vaultInit(password: string): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultInit", payload: { password } });
  }

  public async vaultStatus(): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultStatus" });
  }

  public async vaultPut(key: string, value: string): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultPut", payload: { key, value } });
  }

  public async vaultGet(key: string): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultGet", payload: { key } });
  }

  public async vaultDelete(key: string): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultDelete", payload: { key } });
  }

  public async vaultLock(): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultLock" });
  }

  public async vaultReset(): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "vaultReset" });
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
    const response = result as WasmResponse<T>;
    if (!response.ok) {
      throw new Error(response.error ?? "WASM command failed");
    }
    return (response.data as T) ?? ({} as T);
  }

  private async send(message: WorkerCommand): Promise<unknown> {
    const reply = await new Promise<WorkerReply>((resolve, reject) => {
      const timeoutId = setTimeout(() => {
        this.inflight.delete(message.id);
        reject(new Error(`Worker command timed out after ${WORKER_REQUEST_TIMEOUT_MS}ms`));
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
