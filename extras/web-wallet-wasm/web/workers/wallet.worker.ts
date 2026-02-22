import type { WorkerCommand, WorkerReply } from "../src/wallet/workerMessages";
import { deleteSecret, getSecret, initializeVault, isVaultUnlocked, lockVault, putSecret, resetVault } from "./encryptedVault";

interface WasmModuleLike {
  ccall: (ident: string, returnType: string | null, argTypes: string[], args: unknown[]) => unknown;
}

const WASM_LOAD_TIMEOUT_MS = 45000;

let modulePromise: Promise<WasmModuleLike> | null = null;
let moduleLoadStage = "idle";

function describeError(error: unknown): string {
  if (error instanceof Error) {
    const msg = error.message || "error";
    if (/^\d+$/.test(msg.trim())) {
      return `wasm_numeric_exception:${msg.trim()}`;
    }
    return msg;
  }
  if (typeof error === "string") {
    return error;
  }
  if (typeof error === "number" || typeof error === "bigint") {
    return `wasm_numeric_exception:${String(error)}`;
  }
  try {
    return JSON.stringify(error);
  } catch {
    return String(error);
  }
}

function withTimeout<T>(promise: Promise<T>, timeoutMs: number, timeoutMessage: string): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error(timeoutMessage));
    }, timeoutMs);
    promise
      .then((value) => {
        clearTimeout(timer);
        resolve(value);
      })
      .catch((error) => {
        clearTimeout(timer);
        reject(error);
      });
  });
}

async function loadWasmModule(): Promise<WasmModuleLike> {
  if (!modulePromise) {
    moduleLoadStage = "import_wallet_loader";
    modulePromise = withTimeout(
      import("../wasm/wallet.js").then((factoryModule: unknown) => {
        moduleLoadStage = "resolve_factory";
        const moduleObject = factoryModule as { default?: () => Promise<WasmModuleLike> };
        const create = moduleObject.default ?? (factoryModule as () => Promise<WasmModuleLike>);
        if (typeof create !== "function") {
          throw new Error("wasm_factory_missing");
        }
        moduleLoadStage = "create_module";
        return create();
      }),
      WASM_LOAD_TIMEOUT_MS,
      `wasm_load_timeout:${moduleLoadStage}:${WASM_LOAD_TIMEOUT_MS}ms`
    )
      .then((mod) => {
        moduleLoadStage = "module_ready";
        return mod;
      })
      .catch((error) => {
        modulePromise = null;
        const message = error instanceof Error ? error.message : String(error);
        throw new Error(`wasm_load_failed:${moduleLoadStage}:${message}`);
      });
  }

  return modulePromise;
}

async function invoke(command: string, payload: Record<string, unknown> = {}): Promise<unknown> {
  const request = JSON.stringify({ command, ...payload });
  const payloadKeys = Object.keys(payload).sort().join(",");
  for (let attempt = 0; attempt < 2; attempt += 1) {
    try {
      const mod = await loadWasmModule();
      moduleLoadStage = `invoke_${command}_attempt_${attempt + 1}`;
      const raw = mod.ccall("wallet_wasm_request", "string", ["string"], [request]);
      moduleLoadStage = `parse_${command}_attempt_${attempt + 1}`;
      return JSON.parse(String(raw));
    } catch (error) {
      const message = describeError(error);
      const fatalRuntimeTrap =
        /memory access out of bounds|unaligned accesses|unwind|RuntimeError/i.test(message)
        || /^wasm_numeric_exception:\d+$/.test(message.trim())
        || /^\d+$/.test(message.trim());
      if (!fatalRuntimeTrap || attempt > 0) {
        throw new Error(
          `invoke_failed command=${command} attempt=${attempt + 1} stage=${moduleLoadStage} payloadKeys=${payloadKeys} reason=${message}`
        );
      }
      // Reinitialize module once after a fatal wasm runtime trap.
      modulePromise = null;
      moduleLoadStage = "reloading_after_runtime_trap";
    }
  }
  throw new Error("wallet_wasm_invoke_failed");
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
    const message = describeError(error);
    reply = {
      id: msg.id,
      ok: false,
      error: `${message} (command=${msg.command}, stage=${moduleLoadStage})`
    };
  }

  postMessage(reply);
};
