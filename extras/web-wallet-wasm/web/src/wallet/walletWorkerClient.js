export class WalletWorkerClient {
    constructor() {
        this.inflight = new Map();
        this.worker = new Worker(new URL("../../workers/wallet.worker.ts", import.meta.url), { type: "module" });
        this.worker.onmessage = (event) => {
            const reply = event.data;
            const resolve = this.inflight.get(reply.id);
            if (!resolve) {
                return;
            }
            this.inflight.delete(reply.id);
            resolve(reply);
        };
    }
    async apiVersion() {
        return this.send({ id: crypto.randomUUID(), command: "apiVersion" });
    }
    async vaultInit(password) {
        return this.send({ id: crypto.randomUUID(), command: "vaultInit", payload: { password } });
    }
    async vaultStatus() {
        return this.send({ id: crypto.randomUUID(), command: "vaultStatus" });
    }
    async vaultPut(key, value) {
        return this.send({ id: crypto.randomUUID(), command: "vaultPut", payload: { key, value } });
    }
    async vaultGet(key) {
        return this.send({ id: crypto.randomUUID(), command: "vaultGet", payload: { key } });
    }
    async vaultDelete(key) {
        return this.send({ id: crypto.randomUUID(), command: "vaultDelete", payload: { key } });
    }
    async vaultLock() {
        return this.send({ id: crypto.randomUUID(), command: "vaultLock" });
    }
    async send(message) {
        const reply = await new Promise((resolve) => {
            this.inflight.set(message.id, resolve);
            this.worker.postMessage(message);
        });
        if (!reply.ok) {
            throw new Error(reply.error ?? "Worker command failed");
        }
        return reply.result;
    }
}
