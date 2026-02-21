import { useEffect, useMemo, useState } from "react";
import { DEFAULT_NODES, DEFAULT_RPC_PORT, DEFAULT_RPC_SSL } from "./config/nodes";
import { buildNodeRpcUrl, normalizeNodeHost, type NodeEndpoint } from "./wallet/types";
import { WalletWorkerClient } from "./wallet/walletWorkerClient";

const SETTINGS_NODES_KEY = "wrkz_web_wallet_nodes_v1";
const SETTINGS_SCAN_FROM_COINBASE_KEY = "wrkz_web_wallet_scan_from_coinbase_v1";
const SETTINGS_THEME_KEY = "wrkz_web_wallet_theme_v1";

type ViewTab = "wallet" | "settings";
type ThemeMode = "light" | "dark" | "auto";
type ResolvedTheme = "light" | "dark";

function loadNodesFromStorage(): NodeEndpoint[] {
  try {
    const raw = localStorage.getItem(SETTINGS_NODES_KEY);
    if (!raw) {
      return DEFAULT_NODES;
    }
    const parsed = JSON.parse(raw) as NodeEndpoint[];
    if (!Array.isArray(parsed) || parsed.length === 0) {
      return DEFAULT_NODES;
    }
    return parsed
      .map((item, idx) => ({
        id: String(item.id || `node-${idx + 1}`),
        host: normalizeNodeHost(String(item.host || "")),
        port: Number(item.port || DEFAULT_RPC_PORT),
        ssl: Boolean(item.ssl),
        priority: Number(item.priority || idx + 1)
      }))
      .filter((item) => item.host.length > 0 && Number.isInteger(item.port) && item.port >= 1 && item.port <= 65535);
  } catch {
    return DEFAULT_NODES;
  }
}

function loadScanFromCoinbaseFromStorage(): boolean {
  try {
    return localStorage.getItem(SETTINGS_SCAN_FROM_COINBASE_KEY) === "true";
  } catch {
    return false;
  }
}

function loadThemeFromStorage(): ThemeMode {
  try {
    const value = localStorage.getItem(SETTINGS_THEME_KEY);
    return value === "dark" || value === "light" || value === "auto" ? value : "light";
  } catch {
    return "light";
  }
}

function getSystemTheme(): ResolvedTheme {
  if (typeof window !== "undefined" && window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
    return "dark";
  }
  return "light";
}

export function App(): JSX.Element {
  const client = useMemo(() => new WalletWorkerClient(), []);
  const [activeView, setActiveView] = useState<ViewTab>("wallet");
  const [theme, setTheme] = useState<ThemeMode>(loadThemeFromStorage);
  const [systemTheme, setSystemTheme] = useState<ResolvedTheme>(getSystemTheme);
  const [output, setOutput] = useState<string>("Wallet worker is ready.");
  const [vaultPassword, setVaultPassword] = useState<string>("");
  const [secretValue, setSecretValue] = useState<string>("");
  const [nodes, setNodes] = useState<NodeEndpoint[]>(loadNodesFromStorage);
  const [scanFromCoinbase, setScanFromCoinbase] = useState<boolean>(loadScanFromCoinbaseFromStorage);
  const [customNodeHost, setCustomNodeHost] = useState<string>("");
  const [customNodePort, setCustomNodePort] = useState<string>(String(DEFAULT_RPC_PORT));
  const [customNodeSsl, setCustomNodeSsl] = useState<boolean>(DEFAULT_RPC_SSL);

  useEffect(() => {
    localStorage.setItem(SETTINGS_NODES_KEY, JSON.stringify(nodes));
  }, [nodes]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_SCAN_FROM_COINBASE_KEY, scanFromCoinbase ? "true" : "false");
  }, [scanFromCoinbase]);

  useEffect(() => {
    if (typeof window === "undefined" || !window.matchMedia) {
      return;
    }

    const query = window.matchMedia("(prefers-color-scheme: dark)");
    const onChange = (event: MediaQueryListEvent): void => {
      setSystemTheme(event.matches ? "dark" : "light");
    };

    setSystemTheme(query.matches ? "dark" : "light");
    if (query.addEventListener) {
      query.addEventListener("change", onChange);
      return () => query.removeEventListener("change", onChange);
    }

    query.addListener(onChange);
    return () => query.removeListener(onChange);
  }, []);

  useEffect(() => {
    const resolvedTheme: ResolvedTheme = theme === "auto" ? systemTheme : theme;
    localStorage.setItem(SETTINGS_THEME_KEY, theme);
    document.documentElement.setAttribute("data-theme", resolvedTheme);
  }, [theme, systemTheme]);

  const onPing = async (): Promise<void> => {
    const res = await client.apiVersion();
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultInit = async (): Promise<void> => {
    const res = await client.vaultInit(vaultPassword);
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultStatus = async (): Promise<void> => {
    const res = await client.vaultStatus();
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultStoreDemo = async (): Promise<void> => {
    const res = await client.vaultPut("demo_seed", secretValue);
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultLoadDemo = async (): Promise<void> => {
    const res = await client.vaultGet("demo_seed");
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultLock = async (): Promise<void> => {
    const res = await client.vaultLock();
    setOutput(JSON.stringify(res, null, 2));
  };

  const onAddCustomNode = (): void => {
    const host = normalizeNodeHost(customNodeHost);
    const port = Number(customNodePort);

    if (!host) {
      setOutput("Node host is required.");
      return;
    }
    if (!Number.isInteger(port) || port < 1 || port > 65535) {
      setOutput("Port must be an integer between 1 and 65535.");
      return;
    }

    const priority = nodes.reduce((max, item) => Math.max(max, item.priority), 0) + 1;
    const added: NodeEndpoint = {
      id: `custom-${Date.now()}`,
      host,
      port,
      ssl: customNodeSsl,
      priority
    };

    setNodes((prev) => [...prev, added]);
    setCustomNodeHost("");
    setOutput(`Added RPC node: ${buildNodeRpcUrl(added)}`);
  };

  const onRemoveNode = (id: string): void => {
    setNodes((prev) => prev.filter((item) => item.id !== id));
  };

  const onResetNodes = (): void => {
    setNodes(DEFAULT_NODES);
    setOutput("RPC nodes reset to defaults.");
  };

  const onResetWallet = async (): Promise<void> => {
    const confirmed = window.confirm("Reset wallet local data? This clears encrypted vault data in this browser.");
    if (!confirmed) {
      return;
    }

    await client.vaultReset();
    setOutput("Wallet local vault data has been reset.");
  };

  return (
    <main className="container">
      <h1>Wrkz Web Wallet (WASM)</h1>
      <p>Scaffold with wallet worker bindings and multi-node config hooks.</p>
      <div className="actions">
        <button className={activeView === "wallet" ? "active" : ""} onClick={() => setActiveView("wallet")}>
          Wallet
        </button>
        <button className={activeView === "settings" ? "active" : ""} onClick={() => setActiveView("settings")}>
          Settings
        </button>
      </div>
      {activeView === "wallet" ? (
        <section className="panel">
          <h2>Encrypted Vault (Worker + IndexedDB)</h2>
          <button onClick={onPing}>Check WASM API Version</button>
          <input
            type="password"
            placeholder="Vault password"
            value={vaultPassword}
            onChange={(e) => setVaultPassword(e.target.value)}
          />
          <input
            type="text"
            placeholder="Demo seed or secret"
            value={secretValue}
            onChange={(e) => setSecretValue(e.target.value)}
          />
          <div className="actions">
            <button onClick={onVaultInit}>Init/Unlock Vault</button>
            <button onClick={onVaultStatus}>Vault Status</button>
            <button onClick={onVaultStoreDemo}>Store Demo Secret</button>
            <button onClick={onVaultLoadDemo}>Load Demo Secret</button>
            <button onClick={onVaultLock}>Lock Vault</button>
          </div>
        </section>
      ) : (
        <>
          <section className="panel">
            <h2>Settings</h2>
            <label className="field-label" htmlFor="theme-select">
              Theme
            </label>
            <select id="theme-select" value={theme} onChange={(e) => setTheme(e.target.value as ThemeMode)}>
              <option value="auto">Auto (system)</option>
              <option value="light">Light</option>
              <option value="dark">Dark</option>
            </select>
            <label className="checkbox">
              <input
                type="checkbox"
                checked={scanFromCoinbase}
                onChange={(e) => setScanFromCoinbase(e.target.checked)}
              />
              Scan from coinbase (scan from height 0)
            </label>
            <div className="actions">
              <button className="danger" onClick={onResetWallet}>
                Reset Wallet Local Data
              </button>
            </div>
          </section>
          <section className="panel">
            <h2>RPC Nodes</h2>
            <p>Enter domain/IP without http:// or https://. Default port is {DEFAULT_RPC_PORT}.</p>
            <div className="node-inputs">
              <input
                type="text"
                placeholder="Node domain or IP"
                value={customNodeHost}
                onChange={(e) => setCustomNodeHost(e.target.value)}
              />
              <input
                type="number"
                min={1}
                max={65535}
                placeholder="Port"
                value={customNodePort}
                onChange={(e) => setCustomNodePort(e.target.value)}
              />
              <label className="checkbox">
                <input
                  type="checkbox"
                  checked={customNodeSsl}
                  onChange={(e) => setCustomNodeSsl(e.target.checked)}
                />
                Use SSL
              </label>
              <button onClick={onAddCustomNode}>Add Custom Node</button>
              <button onClick={onResetNodes}>Reset Nodes</button>
            </div>
            <ul className="node-list">
              {nodes
                .slice()
                .sort((a, b) => a.priority - b.priority)
                .map((node) => (
                  <li key={node.id} className="node-item">
                    <code>{buildNodeRpcUrl(node)}</code>
                    <span className="node-meta">priority {node.priority}</span>
                    <button className="danger" onClick={() => onRemoveNode(node.id)}>
                      Remove
                    </button>
                  </li>
                ))}
            </ul>
          </section>
        </>
      )}
      <pre>{output}</pre>
    </main>
  );
}
