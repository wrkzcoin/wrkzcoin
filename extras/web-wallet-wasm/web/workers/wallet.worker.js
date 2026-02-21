import { deleteSecret, getSecret, initializeVault, isVaultUnlocked, lockVault, putSecret } from "./encryptedVault";
let modulePromise = null;
async function loadWasmModule() {
    if (!modulePromise) {
        modulePromise = import("../wasm/wallet.js").then((factory) => {
            const create = factory;
            return create();
        });
    }
    return modulePromise;
}
async function invoke(command, payload = {}) {
    const mod = await loadWasmModule();
    const request = JSON.stringify({ command, ...payload });
    const raw = mod.ccall("wallet_wasm_request", "string", ["string"], [request]);
    return JSON.parse(String(raw));
}
self.onmessage = async (event) => {
    const msg = event.data;
    let reply;
    try {
        let result;
        if (msg.command === "vaultInit") {
            result = await initializeVault(msg.payload.password);
        }
        else if (msg.command === "vaultStatus") {
            result = { unlocked: isVaultUnlocked() };
        }
        else if (msg.command === "vaultPut") {
            await putSecret(msg.payload.key, msg.payload.value);
            result = { stored: true };
        }
        else if (msg.command === "vaultGet") {
            const value = await getSecret(msg.payload.key);
            result = { value };
        }
        else if (msg.command === "vaultDelete") {
            await deleteSecret(msg.payload.key);
            result = { deleted: true };
        }
        else if (msg.command === "vaultLock") {
            lockVault();
            result = { locked: true };
        }
        else {
            const payload = "payload" in msg ? msg.payload : {};
            result = await invoke(msg.command, payload);
        }
        reply = { id: msg.id, ok: true, result };
    }
    catch (error) {
        reply = {
            id: msg.id,
            ok: false,
            error: error instanceof Error ? error.message : "Unknown worker error"
        };
    }
    postMessage(reply);
};
