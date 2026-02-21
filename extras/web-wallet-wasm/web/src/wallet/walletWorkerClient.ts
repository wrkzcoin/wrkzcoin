import type { WorkerCommand, WorkerReply } from "./workerMessages";

export class WalletWorkerClient {
  private readonly worker: Worker;
  private readonly inflight = new Map<string, (reply: WorkerReply) => void>();

  public constructor() {
    this.worker = new Worker(new URL("../../workers/wallet.worker.ts", import.meta.url), { type: "module" });
    this.worker.onmessage = (event: MessageEvent<WorkerReply>) => {
      const reply = event.data;
      const resolve = this.inflight.get(reply.id);
      if (!resolve) {
        return;
      }
      this.inflight.delete(reply.id);
      resolve(reply);
    };
  }

  public async apiVersion(): Promise<unknown> {
    return this.send({ id: crypto.randomUUID(), command: "apiVersion" });
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

  private async send(message: WorkerCommand): Promise<unknown> {
    const reply = await new Promise<WorkerReply>((resolve) => {
      this.inflight.set(message.id, resolve);
      this.worker.postMessage(message);
    });

    if (!reply.ok) {
      throw new Error(reply.error ?? "Worker command failed");
    }

    return reply.result;
  }
}
