const generatedModules = import.meta.glob("./generated/*.js");

const PREFERRED_GENERATED_FILES = [
  "./generated/wallet_wasm.js",
  "./generated/wallet.js",
  "./generated/wallet_backend.js"
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
  for (const key of PREFERRED_GENERATED_FILES) {
    const loader = generatedModules[key];
    if (!loader) {
      continue;
    }

    const mod = await loader();
    if (typeof mod.default === "function") {
      return mod.default;
    }
  }

  for (const loader of Object.values(generatedModules)) {
    const mod = await loader();
    if (typeof mod.default === "function") {
      return mod.default;
    }
  }

  return null;
}

function locateFile(path) {
  return new URL(`./generated/${path}`, import.meta.url).toString();
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
      locateFile
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
