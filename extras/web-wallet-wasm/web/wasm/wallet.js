const PREFERRED_GENERATED_FILES = [
  "wallet_wasm.js",
  "wallet.js",
  "wallet_backend.js"
];

function notBuiltModule(reason) {
  return {
    ccall() {
      return JSON.stringify({
        ok: false,
        code: -1,
        error: "wallet_wasm_not_built",
        reason
      });
    }
  };
}

async function resolveFactory() {
  for (const file of PREFERRED_GENERATED_FILES) {
    try {
      const moduleUrl = new URL(`./generated/${file}`, import.meta.url).toString();
      const mod = await import(/* @vite-ignore */ moduleUrl);
      if (typeof mod.default === "function") {
        return mod.default;
      }
    } catch {
      // Try next candidate.
    }
  }
  return null;
}

function locateFile(path) {
  return new URL(`./generated/${path}`, import.meta.url).toString();
}

function mainScriptUrl() {
  return new URL("./generated/wallet_wasm.js", import.meta.url).toString();
}

function websocketUrlPrefix() {
  if (typeof globalThis !== "undefined" && globalThis.location && globalThis.location.protocol === "https:") {
    return "wss://";
  }
  return "ws://";
}

export default async function createWalletModule() {
  try {
    const factory = await resolveFactory();
    if (!factory) {
      return notBuiltModule("generated_module_not_found");
    }

    const module = await factory({
      noInitialRun: true,
      noExitRuntime: true,
      locateFile,
      mainScriptUrlOrBlob: mainScriptUrl(),
      websocket: {
        url: websocketUrlPrefix()
      }
    });

    if (!module || typeof module.ccall !== "function") {
      return notBuiltModule("invalid_module_shape");
    }

    try {
      module.ccall("wallet_wasm_request", "string", ["string"], ['{"command":"apiVersion"}']);
    } catch {
      return notBuiltModule("missing_wallet_wasm_request_export");
    }

    return module;
  } catch (error) {
    return notBuiltModule(error instanceof Error ? error.message : "unknown_loader_error");
  }
}
