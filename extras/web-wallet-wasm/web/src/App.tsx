import { useEffect, useMemo, useState } from "react";
import { DEFAULT_NODES, DEFAULT_RPC_PORT, DEFAULT_RPC_SSL } from "./config/nodes";
import { COIN_ADDRESS_PREFIX, COIN_DECIMALS, COIN_TICKER, formatAtomicAmount } from "./config/coin";
import { WEB_WALLET_UI_VERSION } from "./config/version";
import { buildNodeHttpUrl, buildNodeRpcUrl, normalizeNodeHost, type NodeEndpoint } from "./wallet/types";
import {
  BrowserDirectRpcEngine,
  type DirectRpcSessionProfile,
  type NodeCapabilities,
  type NodeScore,
  type RpcEngineStatus,
  type RpcNode,
  type ScannedHeaderEntry,
  type SyncCursor,
  type SyncRuntimeStats,
  type WalletTxHistoryEntry,
  type WalletSummary as DirectRpcWalletSummary
} from "./engine-rpc";

const SETTINGS_NODES_KEY = "wrkz_web_wallet_nodes_v1";
const SETTINGS_SCAN_FROM_COINBASE_KEY = "wrkz_web_wallet_scan_from_coinbase_v1";
const SETTINGS_THEME_KEY = "wrkz_web_wallet_theme_v1";
const SETTINGS_DEFAULT_NODE_ID_KEY = "wrkz_web_wallet_default_node_id_v1";
const SETTINGS_AUTH_HASH_KEY = "wrkz_web_wallet_auth_hash_v1";
const SETTINGS_AUTO_LOGOUT_MIN_KEY = "wrkz_web_wallet_auto_logout_min_v1";
const SETTINGS_TX_DYNAMIC_FEE_KEY = "wrkz_web_wallet_tx_dynamic_fee_v1";
const SETTINGS_TX_POW_KEY = "wrkz_web_wallet_tx_pow_v1";
const SETTINGS_FUSION_ENABLED_KEY = "wrkz_web_wallet_fusion_enabled_v1";
const SETTINGS_FUSION_TARGET_KEY = "wrkz_web_wallet_fusion_target_v1";
const DEFAULT_AUTO_LOGOUT_MIN = 15;

type ViewTab = "wallet" | "settings";
type ThemeMode = "light" | "dark" | "auto";
type ResolvedTheme = "light" | "dark";
type WelcomeMode = "create" | "importSeed" | "importKeys";
type ImportReview = {
  kind: "import_seed" | "import_keys";
  sessionId: string;
  sourceFingerprint?: string;
  hasMnemonicSeed: boolean;
  hasPrivateKeys: boolean;
  address: string;
  scanHeight: number;
};
type BackupState = {
  mnemonicSeed: string;
  privateSpendKey: string;
  privateViewKey: string;
  address: string;
  confirmed: boolean;
};

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

function loadAuthHashFromStorage(): string {
  try {
    return localStorage.getItem(SETTINGS_AUTH_HASH_KEY) ?? "";
  } catch {
    return "";
  }
}

function loadAutoLogoutMinutesFromStorage(): number {
  try {
    const raw = Number(localStorage.getItem(SETTINGS_AUTO_LOGOUT_MIN_KEY) ?? DEFAULT_AUTO_LOGOUT_MIN);
    if (!Number.isInteger(raw)) {
      return DEFAULT_AUTO_LOGOUT_MIN;
    }
    return Math.min(240, Math.max(1, raw));
  } catch {
    return DEFAULT_AUTO_LOGOUT_MIN;
  }
}

function loadBoolSetting(key: string, fallback: boolean): boolean {
  try {
    const raw = localStorage.getItem(key);
    if (raw === null) {
      return fallback;
    }
    return raw === "true";
  } catch {
    return fallback;
  }
}

function loadNumberSetting(key: string, fallback: number, min: number, max: number): number {
  try {
    const raw = Number(localStorage.getItem(key) ?? fallback);
    if (!Number.isFinite(raw)) {
      return fallback;
    }
    return Math.min(max, Math.max(min, raw));
  } catch {
    return fallback;
  }
}

function getSystemTheme(): ResolvedTheme {
  if (typeof window !== "undefined" && window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
    return "dark";
  }
  return "light";
}

function normalizeSeed(seed: string): string {
  return seed
    .trim()
    .toLowerCase()
    .split(/\s+/)
    .filter((part) => part.length > 0)
    .join(" ");
}

function isHexKey(value: string): boolean {
  return /^[0-9a-fA-F]{64}$/.test(value.trim());
}

function hasExpectedAddressPrefix(address: string): boolean {
  return address.startsWith(COIN_ADDRESS_PREFIX);
}

async function sha256Hex(input: string): Promise<string> {
  if (!crypto?.subtle) {
    let hash = 5381;
    for (let i = 0; i < input.length; i += 1) {
      hash = ((hash << 5) + hash) + input.charCodeAt(i);
      hash |= 0;
    }
    return `fallback-${Math.abs(hash)}`;
  }
  const data = new TextEncoder().encode(input);
  const hashBuffer = await crypto.subtle.digest("SHA-256", data);
  const bytes = new Uint8Array(hashBuffer);
  return Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export function App(): JSX.Element {
  const directRpcEngine = useMemo(() => new BrowserDirectRpcEngine(), []);
  const [activeView, setActiveView] = useState<ViewTab>("wallet");
  const [welcomeMode, setWelcomeMode] = useState<WelcomeMode>("create");
  const [theme, setTheme] = useState<ThemeMode>(loadThemeFromStorage);
  const [systemTheme, setSystemTheme] = useState<ResolvedTheme>(getSystemTheme);
  const [output, setOutput] = useState<string>("Direct RPC wallet mode is ready.");
  const [nodes, setNodes] = useState<NodeEndpoint[]>(loadNodesFromStorage);
  const [scanFromCoinbase, setScanFromCoinbase] = useState<boolean>(loadScanFromCoinbaseFromStorage);
  const [defaultNodeId, setDefaultNodeId] = useState<string>(loadDefaultNodeIdFromStorage);
  const [authHash, setAuthHash] = useState<string>(loadAuthHashFromStorage);
  const [autoLogoutMinutes, setAutoLogoutMinutes] = useState<number>(loadAutoLogoutMinutesFromStorage);
  const [useDynamicFee, setUseDynamicFee] = useState<boolean>(() => loadBoolSetting(SETTINGS_TX_DYNAMIC_FEE_KEY, true));
  const [enableTxPow, setEnableTxPow] = useState<boolean>(() => loadBoolSetting(SETTINGS_TX_POW_KEY, true));
  const [fusionEnabled, setFusionEnabled] = useState<boolean>(() => loadBoolSetting(SETTINGS_FUSION_ENABLED_KEY, true));
  const [fusionTargetAtomic, setFusionTargetAtomic] = useState<number>(() =>
    loadNumberSetting(SETTINGS_FUSION_TARGET_KEY, 10000, 1, 10_000_000)
  );
  const [isLocked, setIsLocked] = useState<boolean>(loadAuthHashFromStorage().length > 0);
  const [lastActivityAt, setLastActivityAt] = useState<number>(Date.now());
  const [loginPassword, setLoginPassword] = useState<string>("");
  const [directWalletId, setDirectWalletId] = useState<string | null>(null);
  const [directStatus, setDirectStatus] = useState<RpcEngineStatus>({ running: false });
  const [directSummary, setDirectSummary] = useState<DirectRpcWalletSummary | null>(null);
  const [directProfile, setDirectProfile] = useState<DirectRpcSessionProfile | null>(null);
  const [directCursor, setDirectCursor] = useState<SyncCursor | null>(null);
  const [directSyncStats, setDirectSyncStats] = useState<SyncRuntimeStats | null>(null);
  const [selectedNodeCapabilities, setSelectedNodeCapabilities] = useState<NodeCapabilities | null>(null);
  const [nodeScores, setNodeScores] = useState<NodeScore[]>([]);
  const [scanHistory, setScanHistory] = useState<ScannedHeaderEntry[]>([]);
  const [txHistory, setTxHistory] = useState<WalletTxHistoryEntry[]>([]);
  const [pendingImportReview, setPendingImportReview] = useState<ImportReview | null>(null);
  const [pendingBackup, setPendingBackup] = useState<BackupState | null>(null);

  const [walletFilename, setWalletFilename] = useState<string>("my.wallet");
  const [walletPassword, setWalletPassword] = useState<string>("");
  const [walletPasswordConfirm, setWalletPasswordConfirm] = useState<string>("");
  const [scanHeight, setScanHeight] = useState<string>("0");
  const [mnemonicSeed, setMnemonicSeed] = useState<string>("");
  const [privateSpendKey, setPrivateSpendKey] = useState<string>("");
  const [privateViewKey, setPrivateViewKey] = useState<string>("");

  const [customNodeHost, setCustomNodeHost] = useState<string>("");
  const [customNodePort, setCustomNodePort] = useState<string>(String(DEFAULT_RPC_PORT));
  const [customNodeSsl, setCustomNodeSsl] = useState<boolean>(DEFAULT_RPC_SSL);

  const selectedNode = nodes.find((node) => node.id === defaultNodeId) ?? nodes[0];
  const isSecurePage = typeof window !== "undefined" && window.location.protocol === "https:";
  const hasActiveWalletSession = Boolean(
    directStatus.running &&
      directWalletId !== null &&
      directProfile?.walletId &&
      directProfile.walletId === directWalletId
  );

  const toRpcNode = (node: NodeEndpoint): RpcNode => ({
    id: node.id,
    host: node.host,
    port: node.port,
    ssl: node.ssl,
    priority: node.priority
  });

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
    localStorage.setItem(SETTINGS_AUTH_HASH_KEY, authHash);
  }, [authHash]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_AUTO_LOGOUT_MIN_KEY, String(autoLogoutMinutes));
  }, [autoLogoutMinutes]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_TX_DYNAMIC_FEE_KEY, useDynamicFee ? "true" : "false");
  }, [useDynamicFee]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_TX_POW_KEY, enableTxPow ? "true" : "false");
  }, [enableTxPow]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_FUSION_ENABLED_KEY, fusionEnabled ? "true" : "false");
  }, [fusionEnabled]);

  useEffect(() => {
    localStorage.setItem(SETTINGS_FUSION_TARGET_KEY, String(fusionTargetAtomic));
  }, [fusionTargetAtomic]);

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
    if (isLocked || !authHash || !hasActiveWalletSession) {
      return;
    }

    const onUserActivity = (): void => {
      setLastActivityAt(Date.now());
    };

    window.addEventListener("mousemove", onUserActivity);
    window.addEventListener("keydown", onUserActivity);
    window.addEventListener("mousedown", onUserActivity);
    window.addEventListener("touchstart", onUserActivity);

    return () => {
      window.removeEventListener("mousemove", onUserActivity);
      window.removeEventListener("keydown", onUserActivity);
      window.removeEventListener("mousedown", onUserActivity);
      window.removeEventListener("touchstart", onUserActivity);
    };
  }, [isLocked, authHash, hasActiveWalletSession]);

  useEffect(() => {
    if (isLocked || !authHash || !hasActiveWalletSession) {
      return;
    }
    const timer = setInterval(() => {
      const elapsed = Date.now() - lastActivityAt;
      if (elapsed >= autoLogoutMinutes * 60 * 1000) {
        setIsLocked(true);
        setLoginPassword("");
        directRpcEngine.lockVault().catch(() => undefined);
        setOutput(`Session locked after ${autoLogoutMinutes} minute(s) of inactivity.`);
      }
    }, 1000);
    return () => clearInterval(timer);
  }, [isLocked, authHash, hasActiveWalletSession, lastActivityAt, autoLogoutMinutes, directRpcEngine]);

  useEffect(() => {
    if (nodes.length === 0) {
      return;
    }
    const hasSelected = nodes.some((node) => node.id === defaultNodeId);
    if (!hasSelected) {
      setDefaultNodeId(nodes[0].id);
    }
  }, [nodes, defaultNodeId]);

  useEffect(() => {
    const pool = nodes
      .filter((node) => (isSecurePage ? node.ssl : true))
      .map((node) => toRpcNode(node))
      .sort((a, b) => a.priority - b.priority);
    directRpcEngine.configureNodePool(pool, defaultNodeId).catch(() => undefined);
  }, [nodes, defaultNodeId, isSecurePage, directRpcEngine]);

  useEffect(() => {
    Promise.all([
      directRpcEngine.getStatus().catch(() => ({ running: false } as RpcEngineStatus)),
      directRpcEngine.getSummary().catch(() => null),
      directRpcEngine.getProfile().catch(() => null),
      directRpcEngine.getCursor().catch(() => null),
      directRpcEngine.getSyncStats().catch(() => null),
      directRpcEngine.getNodeScores().catch(() => [] as NodeScore[]),
      directRpcEngine.getHistory(30).catch(() => [] as ScannedHeaderEntry[]),
      directRpcEngine.getTransactionHistory(40).catch(() => [] as WalletTxHistoryEntry[])
    ]).then(([status, summary, profile, cursor, stats, scores, history, txs]) => {
      setDirectStatus(status);
      setDirectSummary(summary);
      setDirectProfile(profile);
      setDirectCursor(cursor);
      setDirectSyncStats(stats);
      setNodeScores(scores);
      setScanHistory(history);
      setTxHistory(txs);
      setDirectWalletId(status.running && profile?.walletId ? profile.walletId : null);
    });
  }, [directRpcEngine]);

  useEffect(() => {
    if (!selectedNode) {
      setSelectedNodeCapabilities(null);
      return;
    }
    directRpcEngine
      .getNodeCapabilities(toRpcNode(selectedNode))
      .then((caps) => setSelectedNodeCapabilities(caps))
      .catch(() => setSelectedNodeCapabilities(null));
  }, [defaultNodeId, nodes, selectedNode?.host, selectedNode?.port, selectedNode?.ssl, directRpcEngine]);

  useEffect(() => {
    const timer = setInterval(() => {
      directRpcEngine
        .getStatus()
        .then((status) => setDirectStatus(status))
        .catch(() => undefined);
      directRpcEngine
        .getSummary()
        .then((summary) => setDirectSummary(summary))
        .catch(() => undefined);
      directRpcEngine
        .getCursor()
        .then((cursor) => setDirectCursor(cursor))
        .catch(() => undefined);
      directRpcEngine
        .getSyncStats()
        .then((stats) => setDirectSyncStats(stats))
        .catch(() => undefined);
      directRpcEngine
        .getNodeScores()
        .then((scores) => setNodeScores(scores))
        .catch(() => undefined);
      directRpcEngine
        .getHistory(30)
        .then((history) => setScanHistory(history))
        .catch(() => undefined);
      directRpcEngine
        .getTransactionHistory(40)
        .then((history) => setTxHistory(history))
        .catch(() => undefined);
    }, 3000);
    return () => clearInterval(timer);
  }, [directRpcEngine]);

  const isNodeAllowedForWallet = (node: NodeEndpoint | undefined): node is NodeEndpoint => {
    if (!node) {
      setOutput("No default node selected.");
      return false;
    }
    if (isSecurePage && !node.ssl) {
      setOutput("This page is HTTPS. Select an SSL node (wss/https) for wallet actions.");
      return false;
    }
    return true;
  };

  const connectNodeHttpRpc = async (node: NodeEndpoint): Promise<number> => {
    const endpoint = `${buildNodeHttpUrl(node)}/json_rpc`;
    const response = await fetch(endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ jsonrpc: "2.0", id: "0", method: "getblockcount" })
    });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status} from ${endpoint}`);
    }
    const data = (await response.json()) as { result?: { count?: number; status?: string } };
    const count = Number(data.result?.count);
    if (!Number.isFinite(count)) {
      throw new Error("Invalid getblockcount response");
    }
    return count;
  };

  const validateCommonWalletInputs = (): string | null => {
    if (!walletFilename.trim()) {
      return "Wallet filename is required.";
    }
    if (!walletPassword.trim()) {
      return "Wallet password is required.";
    }
    return null;
  };

  const validatePasswordSetup = (): string | null => {
    if (!walletPassword.trim()) {
      return "Wallet password is required.";
    }
    if (!authHash) {
      if (!walletPasswordConfirm.trim()) {
        return "Please confirm password.";
      }
      if (walletPassword !== walletPasswordConfirm) {
        return "Password and confirmation do not match.";
      }
    }
    return null;
  };

  const ensurePasswordSession = async (): Promise<string | null> => {
    const setupError = validatePasswordSetup();
    if (setupError) {
      return setupError;
    }
    const computed = await sha256Hex(`pwd:${walletPassword}`);
    if (!authHash) {
      setAuthHash(computed);
      await directRpcEngine.initVault(walletPassword).catch(() => undefined);
      setIsLocked(false);
      setLastActivityAt(Date.now());
      return null;
    }
    if (computed !== authHash) {
      return "Incorrect wallet password.";
    }
    await directRpcEngine.initVault(walletPassword).catch(() => undefined);
    setIsLocked(false);
    setLastActivityAt(Date.now());
    return null;
  };

  const validateScanHeight = (): number | null => {
    const parsed = Number(scanHeight || "0");
    if (!Number.isInteger(parsed) || parsed < 0) {
      return null;
    }
    return scanFromCoinbase ? 0 : parsed;
  };

  const copyText = async (value: string, label: string): Promise<void> => {
    try {
      await navigator.clipboard.writeText(value);
      setOutput(`${label} copied to clipboard.`);
    } catch {
      setOutput(`Unable to copy ${label}.`);
    }
  };

  const onConfirmImportReview = async (): Promise<void> => {
    if (!pendingImportReview) {
      return;
    }
    await startDirectSession(
      pendingImportReview.kind,
      pendingImportReview.sessionId,
      {
        sourceFingerprint: pendingImportReview.sourceFingerprint,
        hasMnemonicSeed: pendingImportReview.hasMnemonicSeed,
        hasPrivateKeys: pendingImportReview.hasPrivateKeys
      },
      true
    );
    setPendingImportReview(null);
    setOutput(
      `Import confirmed. Address ${pendingImportReview.address} is syncing from height ${pendingImportReview.scanHeight}.`
    );
  };

  const startDirectSession = async (
    kind: DirectRpcSessionProfile["kind"],
    sessionId: string,
    profilePatch: Partial<Pick<DirectRpcSessionProfile, "sourceFingerprint" | "hasMnemonicSeed" | "hasPrivateKeys">>,
    suppressOutput = false
  ): Promise<void> => {
    if (!isNodeAllowedForWallet(selectedNode)) {
      return;
    }
    const probe = await directRpcEngine.probeNode(toRpcNode(selectedNode));
    if (!probe.ok) {
      setOutput(`Direct RPC probe failed: ${probe.reason ?? "unknown_error"}`);
      return;
    }
    const status = await directRpcEngine.start(sessionId, toRpcNode(selectedNode));
    const parsedScanHeight = validateScanHeight();
    await directRpcEngine.setProfile({
      walletId: sessionId,
      kind,
      filename: walletFilename.trim(),
      scanHeight: parsedScanHeight ?? 0,
      scanFromCoinbase,
      createdAt: Date.now(),
      sourceFingerprint: profilePatch.sourceFingerprint,
      hasMnemonicSeed: profilePatch.hasMnemonicSeed ?? false,
      hasPrivateKeys: profilePatch.hasPrivateKeys ?? false
    });
    setDirectWalletId(sessionId);
    setDirectStatus(status);
    setDirectSummary(await directRpcEngine.getSummary());
    setDirectProfile(await directRpcEngine.getProfile());
    setDirectCursor(await directRpcEngine.getCursor());
    setDirectSyncStats(await directRpcEngine.getSyncStats());
    setTxHistory(await directRpcEngine.getTransactionHistory(40));
    if (!suppressOutput) {
      setOutput(
        `Direct RPC ${kind} session started on ${buildNodeHttpUrl(selectedNode)} (height ${
          probe.height ?? 0
        }). Sync started. Wallet-owned balance is available when private keys are provided.`
      );
    }
  };

  const onCreateWallet = async (): Promise<void> => {
    const commonError = validateCommonWalletInputs();
    if (commonError) {
      setOutput(commonError);
      return;
    }
    const passwordError = await ensurePasswordSession();
    if (passwordError) {
      setOutput(passwordError);
      return;
    }
    const parsedScanHeight = validateScanHeight();
    if (parsedScanHeight === null) {
      setOutput("Scan height must be a non-negative integer.");
      return;
    }
    let generated: Awaited<ReturnType<typeof directRpcEngine.generateScanKeys>>;
    try {
      generated = await directRpcEngine.generateScanKeys();
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
      return;
    }
    setMnemonicSeed(generated.mnemonicSeed);
    setPrivateSpendKey(generated.privateSpendKey);
    setPrivateViewKey(generated.privateViewKey);
    const sessionId = `direct-create-${Date.now()}`;
    await startDirectSession("create", sessionId, {
      hasMnemonicSeed: true,
      hasPrivateKeys: true
    }, true);
    setPendingBackup({
      mnemonicSeed: generated.mnemonicSeed,
      privateSpendKey: generated.privateSpendKey,
      privateViewKey: generated.privateViewKey,
      address: generated.address,
      confirmed: false
    });
    setOutput(`Wallet created. Backup step required before accessing session.`);
  };

  const onImportFromSeed = async (): Promise<void> => {
    const commonError = validateCommonWalletInputs();
    if (commonError) {
      setOutput(commonError);
      return;
    }
    const passwordError = await ensurePasswordSession();
    if (passwordError) {
      setOutput(passwordError);
      return;
    }
    const normalizedSeed = normalizeSeed(mnemonicSeed);
    const wordCount = normalizedSeed.length === 0 ? 0 : normalizedSeed.split(" ").length;
    if (wordCount < 12) {
      setOutput("Mnemonic seed looks invalid. Expected at least 12 words.");
      return;
    }
    const parsedScanHeight = validateScanHeight();
    if (parsedScanHeight === null) {
      setOutput("Scan height must be a non-negative integer.");
      return;
    }
    let derived: Awaited<ReturnType<typeof directRpcEngine.deriveScanKeysFromSeed>>;
    try {
      derived = await directRpcEngine.deriveScanKeysFromSeed(normalizedSeed);
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
      return;
    }
    setPrivateSpendKey(derived.privateSpendKey);
    setPrivateViewKey(derived.privateViewKey);
    const scanFrom = parsedScanHeight;
    const fingerprint = await sha256Hex(`seed:${normalizedSeed}`);
    const sessionId = `direct-seed-${fingerprint.slice(0, 16)}`;
    setPendingImportReview({
      kind: "import_seed",
      sessionId,
      sourceFingerprint: fingerprint,
      hasMnemonicSeed: true,
      hasPrivateKeys: true,
      address: derived.address,
      scanHeight: scanFrom
    });
    setOutput(`Review import details, then confirm to start sync.`);
  };

  const onImportFromKeys = async (): Promise<void> => {
    const commonError = validateCommonWalletInputs();
    if (commonError) {
      setOutput(commonError);
      return;
    }
    const passwordError = await ensurePasswordSession();
    if (passwordError) {
      setOutput(passwordError);
      return;
    }
    if (!isHexKey(privateSpendKey) || !isHexKey(privateViewKey)) {
      setOutput("Private spend/view keys must be 64-character hex values.");
      return;
    }
    const parsedScanHeight = validateScanHeight();
    if (parsedScanHeight === null) {
      setOutput("Scan height must be a non-negative integer.");
      return;
    }
    const normalizedSpend = privateSpendKey.trim().toLowerCase();
    const normalizedView = privateViewKey.trim().toLowerCase();
    const derivedAddress = await directRpcEngine.deriveAddressFromKeys(normalizedSpend, normalizedView).catch(() => "");
    const fingerprint = await sha256Hex(`keys:${normalizedSpend}:${normalizedView}`);
    const sessionId = `direct-keys-${fingerprint.slice(0, 16)}`;
    await directRpcEngine.setScanKeys(normalizedSpend, normalizedView);
    setPendingImportReview({
      kind: "import_keys",
      sessionId,
      sourceFingerprint: fingerprint,
      hasMnemonicSeed: false,
      hasPrivateKeys: true,
      address: derivedAddress || "(unable to derive)",
      scanHeight: parsedScanHeight
    });
    setOutput("Review import details, then confirm to start sync.");
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
    if (isSecurePage && !customNodeSsl) {
      setOutput("Cannot add non-SSL node while app is loaded on HTTPS.");
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
    setOutput(`Added RPC node: ${buildNodeHttpUrl(added)} (${buildNodeRpcUrl(added)})`);
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
    if (isSecurePage && !node.ssl) {
      setOutput("Cannot select non-SSL node for HTTPS page.");
      return;
    }
    setDefaultNodeId(node.id);
    if (directWalletId) {
      const status = await directRpcEngine.start(directWalletId, toRpcNode(node));
      setDirectStatus(status);
      setDirectSummary(await directRpcEngine.getSummary());
      setDirectCursor(await directRpcEngine.getCursor());
      setDirectSyncStats(await directRpcEngine.getSyncStats());
      setOutput(`Direct RPC session moved to ${buildNodeHttpUrl(node)}.`);
      return;
    }
    setOutput(`Default node set to ${buildNodeHttpUrl(node)}.`);
  };

  const onPing = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    try {
      const blockCount = await connectNodeHttpRpc(selectedNode);
      setOutput(`RPC OK via ${buildNodeHttpUrl(selectedNode)} (height ${blockCount})`);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      setOutput(`RPC check failed for ${buildNodeHttpUrl(selectedNode)}: ${message}`);
    }
  };

  const onProbeDefaultNode = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    const probe = await directRpcEngine.probeNode(toRpcNode(selectedNode));
    const caps = await directRpcEngine.refreshNodeCapabilities(toRpcNode(selectedNode));
    setSelectedNodeCapabilities(caps);
    setOutput(JSON.stringify(probe, null, 2));
  };

  const onRefreshNodeCapabilities = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    const caps = await directRpcEngine.refreshNodeCapabilities(toRpcNode(selectedNode));
    setSelectedNodeCapabilities(caps);
    setOutput(`Node capabilities refreshed for ${buildNodeHttpUrl(selectedNode)}.`);
  };

  const onRefreshDirectStatus = async (): Promise<void> => {
    const status = await directRpcEngine.getStatus();
    const summary = await directRpcEngine.getSummary();
    const profile = await directRpcEngine.getProfile();
    const cursor = await directRpcEngine.getCursor();
    const stats = await directRpcEngine.getSyncStats();
    const scores = await directRpcEngine.getNodeScores();
    const history = await directRpcEngine.getHistory(30);
    const txs = await directRpcEngine.getTransactionHistory(40);
    setDirectStatus(status);
    setDirectSummary(summary);
    setDirectProfile(profile);
    setDirectCursor(cursor);
    setDirectSyncStats(stats);
    setNodeScores(scores);
    setScanHistory(history);
    setTxHistory(txs);
    setDirectWalletId(status.running && profile?.walletId ? profile.walletId : null);
    setOutput(`Direct RPC status refreshed. Running: ${status.running ? "yes" : "no"}`);
  };

  const onRefreshNodeScores = async (): Promise<void> => {
    const scores = await directRpcEngine.getNodeScores();
    setNodeScores(scores);
    setOutput(`Node scores refreshed (${scores.length} nodes).`);
  };

  const onUnlockSession = async (): Promise<void> => {
    if (!authHash) {
      setOutput("No wallet password configured yet.");
      return;
    }
    if (!loginPassword.trim()) {
      setOutput("Enter password to unlock.");
      return;
    }
    const computed = await sha256Hex(`pwd:${loginPassword}`);
    if (computed !== authHash) {
      setOutput("Invalid password.");
      return;
    }
    await directRpcEngine.initVault(loginPassword).catch(() => undefined);
    setIsLocked(false);
    setLastActivityAt(Date.now());
    setLoginPassword("");
    setOutput("Session unlocked.");
  };

  const onStopDirectSession = async (): Promise<void> => {
    await directRpcEngine.stop();
    await directRpcEngine.clearScanKeys();
    await directRpcEngine.clearProfile();
    setDirectWalletId(null);
    setDirectStatus(await directRpcEngine.getStatus());
    setDirectSummary(await directRpcEngine.getSummary());
    setDirectProfile(await directRpcEngine.getProfile());
    setDirectCursor(await directRpcEngine.getCursor());
    setDirectSyncStats(await directRpcEngine.getSyncStats());
    setNodeScores(await directRpcEngine.getNodeScores());
    setScanHistory(await directRpcEngine.getHistory(30));
    setTxHistory(await directRpcEngine.getTransactionHistory(40));
    setPendingBackup(null);
    setPendingImportReview(null);
    setIsLocked(false);
    setOutput("Direct RPC session stopped.");
  };

  const onResetWallet = async (): Promise<void> => {
    const confirmed = window.confirm("Reset wallet local data? This clears encrypted vault data in this browser.");
    if (!confirmed) {
      return;
    }
    localStorage.removeItem(SETTINGS_NODES_KEY);
    localStorage.removeItem(SETTINGS_SCAN_FROM_COINBASE_KEY);
    localStorage.removeItem(SETTINGS_THEME_KEY);
    localStorage.removeItem(SETTINGS_DEFAULT_NODE_ID_KEY);
    setNodes(DEFAULT_NODES);
    setScanFromCoinbase(false);
    setTheme("light");
    setDefaultNodeId(DEFAULT_NODES[0].id);
    setWalletFilename("my.wallet");
    setWalletPassword("");
    setWalletPasswordConfirm("");
    setScanHeight("0");
    setMnemonicSeed("");
    setPrivateSpendKey("");
    setPrivateViewKey("");
    await directRpcEngine.stop();
    await directRpcEngine.clearScanKeys();
    await directRpcEngine.lockVault();
    await directRpcEngine.clearProfile();
    setDirectWalletId(null);
    setDirectStatus({ running: false });
    setDirectSummary(null);
    setDirectProfile(null);
    setDirectCursor(null);
    setDirectSyncStats(null);
    setNodeScores([]);
    setScanHistory([]);
    setTxHistory([]);
    setPendingBackup(null);
    setPendingImportReview(null);
    setAuthHash("");
    setIsLocked(false);
    setLoginPassword("");
    setAutoLogoutMinutes(DEFAULT_AUTO_LOGOUT_MIN);
    localStorage.removeItem(SETTINGS_AUTH_HASH_KEY);
    localStorage.removeItem(SETTINGS_AUTO_LOGOUT_MIN_KEY);
    setOutput("Wallet local vault data has been reset.");
  };

  return (
    <main className="container">
      <h1>Wrkz Web Wallet ({COIN_TICKER} WASM)</h1>
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
          {!hasActiveWalletSession ? (
            <>
              <h2>Welcome</h2>
              <p>Create or import a wallet to begin.</p>
              {isSecurePage ? (
                <p>HTTPS mode detected. Wallet actions require SSL nodes.</p>
              ) : null}
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
              {!authHash ? (
                <>
                  <label className="field-label">Confirm wallet password</label>
                  <input
                    type="password"
                    value={walletPasswordConfirm}
                    onChange={(e) => setWalletPasswordConfirm(e.target.value)}
                  />
                </>
              ) : null}
              <label className="field-label">Default remote node</label>
              <select value={defaultNodeId} onChange={(e) => setDefaultNodeId(e.target.value)}>
                {nodes
                  .slice()
                  .sort((a, b) => a.priority - b.priority)
                  .map((node) => (
                    <option key={node.id} value={node.id} disabled={isSecurePage && !node.ssl}>
                      {buildNodeHttpUrl(node)}
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
                <button onClick={onPing}>Test RPC (HTTP)</button>
              </div>
            </>
          ) : isLocked ? (
            <>
              <h2>Session Locked</h2>
              <p>Auto-logout is enabled. Enter wallet password to unlock session.</p>
              <label className="field-label">Wallet password</label>
              <input type="password" value={loginPassword} onChange={(e) => setLoginPassword(e.target.value)} />
              <div className="actions">
                <button onClick={onUnlockSession}>Unlock</button>
              </div>
            </>
          ) : (
            <>
              <h2>Wallet Session</h2>
              <p>Wallet handle: {directWalletId}</p>
              <p>
                Direct RPC status: {directStatus.running ? "running" : "stopped"}
                {directSummary ? `, daemon height ${directSummary.daemonHeight}` : ""}
              </p>
              {directStatus.activeNodeEndpoint ? (
                <p>
                  Active node: {directStatus.activeNodeEndpoint} (failovers: {directStatus.failoverCount ?? 0})
                </p>
              ) : null}
              {typeof directSummary?.scannedHeaderCount === "number" ? (
                <p>Scanned headers stored: {directSummary.scannedHeaderCount}</p>
              ) : null}
              {directSummary ? (
                <p>
                  Balance: unlocked {formatAtomicAmount(directSummary.unlockedBalanceAtomic, COIN_DECIMALS)} {COIN_TICKER}, locked{" "}
                  {formatAtomicAmount(directSummary.lockedBalanceAtomic, COIN_DECIMALS)} {COIN_TICKER}
                  {" "}({directSummary.unlockedBalanceAtomic}/{directSummary.lockedBalanceAtomic} atomic)
                  {directSummary.scanMode === "wallet_owned_outputs" ? " (wallet-owned outputs)" : " (headers-only mode)"}
                </p>
              ) : null}
              {directSummary?.scanMode !== "wallet_owned_outputs" ? (
                <p>Wallet-owned balance requires import from private keys in direct-RPC mode.</p>
              ) : null}
              {directCursor ? <p>Direct cursor height: {directCursor.height}</p> : null}
              {directSyncStats ? (
                <p>
                  Sync progress: {directSyncStats.syncedHeight}/{directSyncStats.targetHeight}, remaining{" "}
                  {directSyncStats.remainingBlocks}, last batch {directSyncStats.lastBatchStart ?? "-"}-
                  {directSyncStats.lastBatchEnd ?? "-"} ({directSyncStats.lastBatchSize}), fetch mode{" "}
                  {directSyncStats.fetchMode}, range fetch {directSyncStats.rangeFetchOk ? "ok" : "fallback"}
                </p>
              ) : null}
              {directSyncStats?.methodsTried?.length ? <p>Methods tried: {directSyncStats.methodsTried.join(" -> ")}</p> : null}
              {directSyncStats?.lastError ? <p>Last sync error: {directSyncStats.lastError}</p> : null}
              {directProfile ? (
                <p>
                  Direct profile: {directProfile.kind}, scan height {directProfile.scanHeight}, file {directProfile.filename}
                </p>
              ) : null}
              {directSummary ? (
                <p>
                  Scan stats: tx {directSummary.scannedTransactions ?? 0}, outputs {directSummary.scannedOutputs ?? 0},
                  unspent owned {directSummary.unspentOwnedOutputs ?? 0}, spent owned {directSummary.spentOwnedOutputs ?? 0}
                </p>
              ) : null}
              <p>
                Tx policy: fee {useDynamicFee ? "dynamic" : "fixed/manual"}, tx-pow {enableTxPow ? "enabled" : "disabled"},
                fusion {fusionEnabled ? `enabled (target ${fusionTargetAtomic} atomic)` : "disabled"}.
              </p>
              <button onClick={onPing}>Test RPC (HTTP)</button>
              <p>Header scan history:</p>
              {scanHistory.length > 0 ? (
                <section className="panel">
                  <h3>Scanned History (Headers)</h3>
                  <ul className="node-list">
                    {scanHistory.slice(0, 12).map((entry) => (
                      <li key={`${entry.height}-${entry.hash ?? "nohash"}`} className="node-item">
                        <span className="node-meta">height {entry.height}</span>
                        <span className="node-meta">ts {entry.timestamp ?? "-"}</span>
                        <code>{entry.hash ?? "no-hash"}</code>
                      </li>
                    ))}
                  </ul>
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
            <label className="field-label">Auto logout minutes</label>
            <input
              type="number"
              min={1}
              max={240}
              value={String(autoLogoutMinutes)}
              onChange={(e) => {
                const parsed = Number(e.target.value);
                if (Number.isInteger(parsed) && parsed >= 1 && parsed <= 240) {
                  setAutoLogoutMinutes(parsed);
                }
              }}
            />
            <p>Session locks after {autoLogoutMinutes} minute(s) of inactivity.</p>
            <label className="checkbox">
              <input type="checkbox" checked={useDynamicFee} onChange={(e) => setUseDynamicFee(e.target.checked)} />
              Use dynamic transaction fee
            </label>
            <label className="checkbox">
              <input type="checkbox" checked={enableTxPow} onChange={(e) => setEnableTxPow(e.target.checked)} />
              Enable transaction PoW
            </label>
            <label className="checkbox">
              <input type="checkbox" checked={fusionEnabled} onChange={(e) => setFusionEnabled(e.target.checked)} />
              Enable fusion optimization
            </label>
            {fusionEnabled ? (
              <>
                <label className="field-label">Fusion target (atomic units)</label>
                <input
                  type="number"
                  min={1}
                  max={10000000}
                  value={String(fusionTargetAtomic)}
                  onChange={(e) => {
                    const parsed = Number(e.target.value);
                    if (Number.isFinite(parsed) && parsed >= 1 && parsed <= 10000000) {
                      setFusionTargetAtomic(Math.floor(parsed));
                    }
                  }}
                />
              </>
            ) : null}
            <div className="actions">
              <button onClick={onProbeDefaultNode}>Probe Default Node</button>
              <button onClick={onRefreshNodeCapabilities}>Refresh Node Capabilities</button>
              <button onClick={onRefreshNodeScores}>Refresh Node Scores</button>
              <button onClick={onRefreshDirectStatus}>Refresh Direct RPC Status</button>
              <button className="danger" onClick={onStopDirectSession}>
                Stop Direct RPC Session
              </button>
            </div>
            {selectedNodeCapabilities ? (
              <p>
                Capabilities: blockcount {selectedNodeCapabilities.supportsGetBlockCount ? "yes" : "no"}, walletsync{" "}
                {selectedNodeCapabilities.supportsWalletSyncData ? "yes" : "no"}, range{" "}
                {selectedNodeCapabilities.supportsGetBlockHeadersRange ? "yes" : "no"}, by-height{" "}
                {selectedNodeCapabilities.supportsGetBlockHeaderByHeight ? "yes" : "no"}
              </p>
            ) : null}
            {nodeScores.length > 0 ? (
              <p>
                Node scores:{" "}
                {nodeScores
                  .slice(0, 4)
                  .map((score) => {
                    const cooldown = score.cooldownUntil && score.cooldownUntil > Date.now() ? " cooldown" : "";
                    return `${score.nodeId}=${score.score} (ok ${score.successCount}/fail ${score.failureCount}${cooldown})`;
                  })
                  .join(" | ")}
              </p>
            ) : null}
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
                    <code>{buildNodeHttpUrl(node)}</code>
                    <span className="node-meta">{buildNodeRpcUrl(node)}</span>
                    <span className="node-meta">priority {node.priority}</span>
                    <button className={defaultNodeId === node.id ? "active" : ""} onClick={() => onUseNodeForWallet(node)}>
                      Set Default
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
      <footer className="status-bar">
        <span>Status: {output}</span>
        <span>
          Version: {WEB_WALLET_UI_VERSION} | {COIN_TICKER} | {COIN_DECIMALS} dp
        </span>
      </footer>
      <pre>{output}</pre>
    </main>
  );
}
