import type { WorkerCommand, WorkerReply } from "../src/wallet/workerMessages";
import { deleteSecret, getSecret, initializeVault, isVaultUnlocked, lockVault, putSecret, resetVault } from "./encryptedVault";

interface WasmModuleLike {
  ccall: (ident: string, returnType: string | null, argTypes: string[], args: unknown[]) => unknown;
}

let modulePromise: Promise<WasmModuleLike> | null = null;

async function loadWasmModule(): Promise<WasmModuleLike> {
  if (!modulePromise) {
    modulePromise = import("../wasm/wallet.js").then((factoryModule: unknown) => {
      const moduleObject = factoryModule as { default?: () => Promise<WasmModuleLike> };
      const create = moduleObject.default ?? (factoryModule as () => Promise<WasmModuleLike>);
      if (typeof create !== "function") {
        throw new Error("wasm_factory_missing");
      }
      return create();
    });
  }

  return modulePromise;
}

async function invoke(command: string, payload: Record<string, unknown> = {}): Promise<unknown> {
  const mod = await loadWasmModule();
  const request = JSON.stringify({ command, ...payload });
  const raw = mod.ccall("wallet_wasm_request", "string", ["string"], [request]);
  return JSON.parse(String(raw));
}

self.onmessage = async (event: MessageEvent<WorkerCommand>): Promise<void> => {
  const msg = event.data;
  let reply: WorkerReply;

  try {
    let result: unknown;
    if (msg.command === "vaultInit") {
      result = await initializeVault(msg.payload.password);
    } else if (msg.command === "vaultStatus") {
      result = { unlocked: isVaultUnlocked() };
    } else if (msg.command === "vaultPut") {
      await putSecret(msg.payload.key, msg.payload.value);
      result = { stored: true };
    } else if (msg.command === "vaultGet") {
      const value = await getSecret(msg.payload.key);
      result = { value };
    } else if (msg.command === "vaultDelete") {
      await deleteSecret(msg.payload.key);
      result = { deleted: true };
    } else if (msg.command === "vaultLock") {
      lockVault();
      result = { locked: true };
    } else if (msg.command === "vaultReset") {
      await resetVault();
      result = { reset: true };
    } else {
      const payload = "payload" in msg ? msg.payload : {};
      result = await invoke(msg.command, payload as Record<string, unknown>);
    }

    reply = { id: msg.id, ok: true, result };
  } catch (error) {
    let message = "Unknown worker error";
    if (error instanceof Error) {
      message = error.message;
    } else if (typeof error === "string") {
      message = error;
    } else {
      try {
        message = JSON.stringify(error);
      } catch {
        message = String(error);
      }
    }
    reply = {
      id: msg.id,
      ok: false,
      error: message
    };
  }

  postMessage(reply);
};
