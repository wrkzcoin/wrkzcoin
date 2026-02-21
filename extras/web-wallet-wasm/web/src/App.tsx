import { useEffect, useMemo, useState } from "react";
import { DEFAULT_NODES, DEFAULT_RPC_PORT, DEFAULT_RPC_SSL } from "./config/nodes";
import { buildNodeRpcUrl, normalizeNodeHost, type NodeEndpoint, type WalletBackupSecrets } from "./wallet/types";
import { WalletWorkerClient } from "./wallet/walletWorkerClient";

const SETTINGS_NODES_KEY = "wrkz_web_wallet_nodes_v1";
const SETTINGS_SCAN_FROM_COINBASE_KEY = "wrkz_web_wallet_scan_from_coinbase_v1";
const SETTINGS_THEME_KEY = "wrkz_web_wallet_theme_v1";
const SETTINGS_DEFAULT_NODE_ID_KEY = "wrkz_web_wallet_default_node_id_v1";

type ViewTab = "wallet" | "settings";
type ThemeMode = "light" | "dark" | "auto";
type ResolvedTheme = "light" | "dark";
type WelcomeMode = "create" | "importSeed" | "importKeys";

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

function loadDefaultNodeIdFromStorage(): string {
  try {
    return localStorage.getItem(SETTINGS_DEFAULT_NODE_ID_KEY) ?? DEFAULT_NODES[0].id;
  } catch {
    return DEFAULT_NODES[0].id;
  }
}

function getSystemTheme(): ResolvedTheme {
  if (typeof window !== "undefined" && window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
    return "dark";
  }
  return "light";
}

async function copyText(text: string): Promise<void> {
  if (navigator.clipboard?.writeText) {
    await navigator.clipboard.writeText(text);
  }
}

export function App(): JSX.Element {
  const client = useMemo(() => new WalletWorkerClient(), []);
  const [activeView, setActiveView] = useState<ViewTab>("wallet");
  const [welcomeMode, setWelcomeMode] = useState<WelcomeMode>("create");
  const [theme, setTheme] = useState<ThemeMode>(loadThemeFromStorage);
  const [systemTheme, setSystemTheme] = useState<ResolvedTheme>(getSystemTheme);
  const [output, setOutput] = useState<string>("Wallet worker is ready.");
  const [nodes, setNodes] = useState<NodeEndpoint[]>(loadNodesFromStorage);
  const [scanFromCoinbase, setScanFromCoinbase] = useState<boolean>(loadScanFromCoinbaseFromStorage);
  const [defaultNodeId, setDefaultNodeId] = useState<string>(loadDefaultNodeIdFromStorage);
  const [walletId, setWalletId] = useState<number | null>(null);
  const [backup, setBackup] = useState<WalletBackupSecrets | null>(null);

  const [walletFilename, setWalletFilename] = useState<string>("my.wallet");
  const [walletPassword, setWalletPassword] = useState<string>("");
  const [scanHeight, setScanHeight] = useState<string>("0");
  const [mnemonicSeed, setMnemonicSeed] = useState<string>("");
  const [privateSpendKey, setPrivateSpendKey] = useState<string>("");
  const [privateViewKey, setPrivateViewKey] = useState<string>("");

  const [customNodeHost, setCustomNodeHost] = useState<string>("");
  const [customNodePort, setCustomNodePort] = useState<string>(String(DEFAULT_RPC_PORT));
  const [customNodeSsl, setCustomNodeSsl] = useState<boolean>(DEFAULT_RPC_SSL);

  const selectedNode = nodes.find((node) => node.id === defaultNodeId) ?? nodes[0];

  useEffect(() => {
    localStorage.setItem(SETTINGS_NODES_KEY, JSON.stringify(nodes));
  }, [nodes]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_SCAN_FROM_COINBASE_KEY, scanFromCoinbase ? "true" : "false");
  }, [scanFromCoinbase]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_DEFAULT_NODE_ID_KEY, defaultNodeId);
  }, [defaultNodeId]);

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

  useEffect(() => {
    if (nodes.length === 0) {
      return;
    }
    const hasSelected = nodes.some((node) => node.id === defaultNodeId);
    if (!hasSelected) {
      setDefaultNodeId(nodes[0].id);
    }
  }, [nodes, defaultNodeId]);

  const effectiveScanHeight = scanFromCoinbase ? 0 : Number(scanHeight || "0");

  const onCreateWallet = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    const created = await client.create({
      filename: walletFilename,
      password: walletPassword,
      daemonHost: selectedNode.host,
      daemonPort: selectedNode.port,
      daemonSsl: selectedNode.ssl,
      syncThreads: 2
    });
    setWalletId(created.walletId);
    const secrets = await client.backupSecrets(created.walletId);
    setBackup(secrets);
    setOutput("Wallet created. Save your backup secrets before continuing.");
  };

  const onImportFromSeed = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    const created = await client.restoreFromSeed({
      mnemonicSeed,
      filename: walletFilename,
      password: walletPassword,
      scanHeight: Number.isInteger(effectiveScanHeight) && effectiveScanHeight >= 0 ? effectiveScanHeight : 0,
      daemonHost: selectedNode.host,
      daemonPort: selectedNode.port,
      daemonSsl: selectedNode.ssl,
      syncThreads: 2
    });
    setWalletId(created.walletId);
    setBackup(null);
    setOutput(`Wallet imported from seed. Scan height: ${scanFromCoinbase ? 0 : effectiveScanHeight}.`);
  };

  const onImportFromKeys = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    const created = await client.restoreFromKeys({
      privateSpendKey,
      privateViewKey,
      filename: walletFilename,
      password: walletPassword,
      scanHeight: Number.isInteger(effectiveScanHeight) && effectiveScanHeight >= 0 ? effectiveScanHeight : 0,
      daemonHost: selectedNode.host,
      daemonPort: selectedNode.port,
      daemonSsl: selectedNode.ssl,
      syncThreads: 2
    });
    setWalletId(created.walletId);
    setBackup(null);
    setOutput(`Wallet imported from keys. Scan height: ${scanFromCoinbase ? 0 : effectiveScanHeight}.`);
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
    setDefaultNodeId(DEFAULT_NODES[0].id);
    setOutput("RPC nodes reset to defaults.");
  };

  const onUseNodeForWallet = async (node: NodeEndpoint): Promise<void> => {
    setDefaultNodeId(node.id);
    if (walletId === null) {
      setOutput(`Default node set to ${buildNodeRpcUrl(node)}.`);
      return;
    }
    await client.swapNode(walletId, node.host, node.port, node.ssl);
    setOutput(`Active wallet swapped to ${buildNodeRpcUrl(node)}.`);
  };

  const onPing = async (): Promise<void> => {
    const res = await client.apiVersion();
    setOutput(JSON.stringify(res, null, 2));
  };

  const onResetWallet = async (): Promise<void> => {
    const confirmed = window.confirm("Reset wallet local data? This clears encrypted vault data in this browser.");
    if (!confirmed) {
      return;
    }
    await client.vaultReset();
    if (walletId !== null) {
      await client.close(walletId).catch(() => undefined);
    }
    setWalletId(null);
    setBackup(null);
    setOutput("Wallet local vault data has been reset.");
  };

  return (
    <main className="container">
      <h1>Wrkz Web Wallet (WASM)</h1>
      <p>Browser wallet with onboarding, encrypted local vault, and configurable RPC nodes.</p>
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
          {walletId === null ? (
            <>
              <h2>Welcome</h2>
              <p>Create or import a wallet to begin.</p>
              <div className="actions">
                <button className={welcomeMode === "create" ? "active" : ""} onClick={() => setWelcomeMode("create")}>
                  Create Wallet
                </button>
                <button className={welcomeMode === "importSeed" ? "active" : ""} onClick={() => setWelcomeMode("importSeed")}>
                  Import From Seed
                </button>
                <button className={welcomeMode === "importKeys" ? "active" : ""} onClick={() => setWelcomeMode("importKeys")}>
                  Import From Keys
                </button>
              </div>
              <label className="field-label">Wallet filename</label>
              <input type="text" value={walletFilename} onChange={(e) => setWalletFilename(e.target.value)} />
              <label className="field-label">Wallet password</label>
              <input type="password" value={walletPassword} onChange={(e) => setWalletPassword(e.target.value)} />
              <label className="field-label">Default remote node</label>
              <select value={defaultNodeId} onChange={(e) => setDefaultNodeId(e.target.value)}>
                {nodes
                  .slice()
                  .sort((a, b) => a.priority - b.priority)
                  .map((node) => (
                    <option key={node.id} value={node.id}>
                      {buildNodeRpcUrl(node)}
                    </option>
                  ))}
              </select>
              {welcomeMode !== "create" ? (
                <>
                  <label className="field-label">Scan height</label>
                  <input type="number" min={0} value={scanHeight} onChange={(e) => setScanHeight(e.target.value)} />
                  <label className="checkbox">
                    <input type="checkbox" checked={scanFromCoinbase} onChange={(e) => setScanFromCoinbase(e.target.checked)} />
                    Scan from coinbase (force height 0)
                  </label>
                </>
              ) : null}
              {welcomeMode === "importSeed" ? (
                <>
                  <label className="field-label">Mnemonic seed</label>
                  <textarea
                    className="input-area"
                    value={mnemonicSeed}
                    onChange={(e) => setMnemonicSeed(e.target.value)}
                    placeholder="25-word seed phrase"
                  />
                </>
              ) : null}
              {welcomeMode === "importKeys" ? (
                <>
                  <label className="field-label">Private spend key</label>
                  <input type="text" value={privateSpendKey} onChange={(e) => setPrivateSpendKey(e.target.value)} />
                  <label className="field-label">Private view key</label>
                  <input type="text" value={privateViewKey} onChange={(e) => setPrivateViewKey(e.target.value)} />
                </>
              ) : null}
              <div className="actions">
                {welcomeMode === "create" ? <button onClick={onCreateWallet}>Create Wallet</button> : null}
                {welcomeMode === "importSeed" ? <button onClick={onImportFromSeed}>Import From Seed</button> : null}
                {welcomeMode === "importKeys" ? <button onClick={onImportFromKeys}>Import From Keys</button> : null}
              </div>
            </>
          ) : (
            <>
              <h2>Wallet Session</h2>
              <p>Wallet handle: {walletId}</p>
              <button onClick={onPing}>Check WASM API Version</button>
              {backup ? (
                <section className="panel">
                  <h3>Backup Secrets</h3>
                  <p>Copy and store these secrets offline now.</p>
                  <div className="backup-row">
                    <strong>Address</strong>
                    <code>{backup.address}</code>
                    <button onClick={() => copyText(backup.address)}>Copy</button>
                  </div>
                  <div className="backup-row">
                    <strong>Mnemonic Seed</strong>
                    <code>{backup.mnemonicSeed}</code>
                    <button onClick={() => copyText(backup.mnemonicSeed)}>Copy</button>
                  </div>
                  <div className="backup-row">
                    <strong>Private View Key</strong>
                    <code>{backup.privateViewKey}</code>
                    <button onClick={() => copyText(backup.privateViewKey)}>Copy</button>
                  </div>
                  <div className="backup-row">
                    <strong>Private Spend Key</strong>
                    <code>{backup.privateSpendKey}</code>
                    <button onClick={() => copyText(backup.privateSpendKey)}>Copy</button>
                  </div>
                </section>
              ) : null}
            </>
          )}
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
              <input type="checkbox" checked={scanFromCoinbase} onChange={(e) => setScanFromCoinbase(e.target.checked)} />
              Scan from coinbase by default
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
                <input type="checkbox" checked={customNodeSsl} onChange={(e) => setCustomNodeSsl(e.target.checked)} />
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
                    <button className={defaultNodeId === node.id ? "active" : ""} onClick={() => onUseNodeForWallet(node)}>
                      {walletId === null ? "Set Default" : "Use For Wallet"}
                    </button>
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
