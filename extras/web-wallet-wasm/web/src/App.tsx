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
type WalletTab =
  | "overview"
  | "transactions"
  | "balances"
  | "transfer"
  | "receive"
  | "nodes"
  | "backup";
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
  scanTimestamp: number;
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

function randomHex(length: number): string {
  const chars = "0123456789abcdef";
  const bytes = new Uint8Array(length);
  if (typeof crypto !== "undefined" && typeof crypto.getRandomValues === "function") {
    crypto.getRandomValues(bytes);
    return Array.from(bytes, (value) => chars[value % 16]).join("");
  }
  return Array.from({ length }, () => chars[Math.floor(Math.random() * 16)]).join("");
}

function parseCoinAmountToAtomic(amount: string, decimals: number): bigint | null {
  const value = amount.trim();
  if (!/^\d+(\.\d+)?$/.test(value)) {
    return null;
  }
  const [whole, frac = ""] = value.split(".");
  if (frac.length > decimals) {
    return null;
  }
  const paddedFrac = frac.padEnd(decimals, "0");
  try {
    return BigInt(whole) * (10n ** BigInt(decimals)) + BigInt(paddedFrac || "0");
  } catch {
    return null;
  }
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
  const [walletTab, setWalletTab] = useState<WalletTab>("overview");
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
  const [nodeLatencyById, setNodeLatencyById] = useState<Record<string, number | null>>({});
  const [pendingImportReview, setPendingImportReview] = useState<ImportReview | null>(null);
  const [pendingBackup, setPendingBackup] = useState<BackupState | null>(null);

  const [walletFilename, setWalletFilename] = useState<string>("my.wallet");
  const [walletPassword, setWalletPassword] = useState<string>("");
  const [walletPasswordConfirm, setWalletPasswordConfirm] = useState<string>("");
  const [scanHeight, setScanHeight] = useState<string>("0");
  const [scanTimestamp, setScanTimestamp] = useState<string>("0");
  const [mnemonicSeed, setMnemonicSeed] = useState<string>("");
  const [privateSpendKey, setPrivateSpendKey] = useState<string>("");
  const [privateViewKey, setPrivateViewKey] = useState<string>("");
  const [transferAddress, setTransferAddress] = useState<string>("");
  const [transferAmount, setTransferAmount] = useState<string>("");
  const [transferPaymentId, setTransferPaymentId] = useState<string>("");
  const [transferAddressValid, setTransferAddressValid] = useState<boolean | null>(null);
  const [transferAddressReason, setTransferAddressReason] = useState<string>("");
  const [receivePaymentId, setReceivePaymentId] = useState<string>("");
  const [receiveIntegratedAddress, setReceiveIntegratedAddress] = useState<string>("");

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
  const syncPercentRaw = directSyncStats
    ? Math.min(100, Math.max(0, (directSyncStats.syncedHeight / Math.max(1, directSyncStats.targetHeight)) * 100))
    : 0;
  const syncPercentLabel = directSyncStats
    ? (syncPercentRaw > 0 && syncPercentRaw < 0.1 ? "<0.1%" : `${syncPercentRaw.toFixed(1)}%`)
    : "0.0%";
  const receiveAddress = directProfile?.address ?? pendingBackup?.address ?? "";
  const receiveTarget = receiveIntegratedAddress || receiveAddress;
  const receiveQrSrc = receiveTarget
    ? `https://api.qrserver.com/v1/create-qr-code/?size=220x220&data=${encodeURIComponent(receiveTarget)}`
    : "";
  const resolvedTheme: ResolvedTheme = theme === "auto" ? systemTheme : theme;

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

  useEffect(() => {
    let cancelled = false;
    const value = transferAddress.trim();
    if (!value) {
      setTransferAddressValid(null);
      setTransferAddressReason("");
      return;
    }
    if (!hasExpectedAddressPrefix(value)) {
      setTransferAddressValid(false);
      setTransferAddressReason(`Address must start with ${COIN_ADDRESS_PREFIX}.`);
      return;
    }
    directRpcEngine
      .validateAddress(value, true)
      .then((result) => {
        if (cancelled) {
          return;
        }
        setTransferAddressValid(Boolean(result.valid));
        setTransferAddressReason(result.valid ? "Address is valid." : (result.reason ?? "Invalid address."));
      })
      .catch((error) => {
        if (cancelled) {
          return;
        }
        setTransferAddressValid(false);
        setTransferAddressReason(error instanceof Error ? error.message : String(error));
      });
    return () => {
      cancelled = true;
    };
  }, [transferAddress, directRpcEngine]);

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

  const validateScanTimestamp = (): number | null => {
    const parsed = Number(scanTimestamp || "0");
    if (!Number.isInteger(parsed) || parsed < 0) {
      return null;
    }
    return scanFromCoinbase ? 0 : parsed;
  };

  const copyText = async (value: string, label: string, alertOnCopy = false): Promise<void> => {
    try {
      await navigator.clipboard.writeText(value);
      setOutput(`${label} copied to clipboard.`);
      if (alertOnCopy) {
        window.alert(`${label} copied.`);
      }
    } catch {
      setOutput(`Unable to copy ${label}.`);
    }
  };

  const validateWalletAddress = async (address: string, allowIntegrated: boolean): Promise<boolean> => {
    const value = address.trim();
    if (!value) {
      setOutput("Address is required.");
      return false;
    }
    if (!hasExpectedAddressPrefix(value)) {
      setOutput(`Address must start with ${COIN_ADDRESS_PREFIX}.`);
      return false;
    }
    try {
      const result = await directRpcEngine.validateAddress(value, allowIntegrated);
      if (!result.valid) {
        setOutput(result.reason ?? "Invalid address.");
        return false;
      }
      return true;
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
      return false;
    }
  };

  const onCopyStandardReceiveAddress = async (): Promise<void> => {
    const valid = await validateWalletAddress(receiveAddress, false);
    if (!valid) {
      return;
    }
    await copyText(receiveAddress, "Standard address", true);
  };

  const onCopyIntegratedReceiveAddress = async (): Promise<void> => {
    const valid = await validateWalletAddress(receiveIntegratedAddress, true);
    if (!valid) {
      return;
    }
    await copyText(receiveIntegratedAddress, "Integrated address", true);
  };

  const onConfirmBackupSaved = (): void => {
    if (!pendingBackup) {
      setOutput("No pending backup data.");
      return;
    }
    setPendingBackup({ ...pendingBackup, confirmed: true });
    setOutput("Backup confirmed. Keep your seed and keys offline.");
  };

  const onClearBackupFromScreen = (): void => {
    if (!pendingBackup) {
      return;
    }
    setPendingBackup(null);
    setOutput("Backup material removed from screen.");
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
        hasPrivateKeys: pendingImportReview.hasPrivateKeys,
        address: pendingImportReview.address
      },
      true,
      { height: pendingImportReview.scanHeight, timestamp: pendingImportReview.scanTimestamp }
    );
    setPendingImportReview(null);
    setOutput(
      `Import confirmed. Address ${pendingImportReview.address} is syncing from height ${pendingImportReview.scanHeight} (timestamp ${pendingImportReview.scanTimestamp}).`
    );
  };

  const startDirectSession = async (
    kind: DirectRpcSessionProfile["kind"],
    sessionId: string,
    profilePatch: Partial<
      Pick<DirectRpcSessionProfile, "sourceFingerprint" | "hasMnemonicSeed" | "hasPrivateKeys" | "address">
    >,
    suppressOutput = false,
    scanOverride?: { height: number; timestamp: number }
  ): Promise<void> => {
    if (!isNodeAllowedForWallet(selectedNode)) {
      return;
    }
    const probe = await directRpcEngine.probeNode(toRpcNode(selectedNode));
    if (!probe.ok) {
      setOutput(`Direct RPC probe failed: ${probe.reason ?? "unknown_error"}`);
      return;
    }
    const parsedScanHeight = validateScanHeight();
    const parsedScanTimestamp = validateScanTimestamp();
    if (parsedScanHeight === null) {
      setOutput("Scan height must be a non-negative integer.");
      return;
    }
    if (parsedScanTimestamp === null) {
      setOutput("Scan timestamp must be a non-negative integer.");
      return;
    }
    let effectiveScanHeight = scanOverride?.height ?? parsedScanHeight;
    let effectiveScanTimestamp = scanOverride?.timestamp ?? parsedScanTimestamp;
    if (kind === "create") {
      effectiveScanHeight = Math.max(0, probe.height ?? 0);
      effectiveScanTimestamp = Math.floor(Date.now() / 1000);
    }
    if (scanFromCoinbase) {
      effectiveScanHeight = 0;
      effectiveScanTimestamp = 0;
    }
    await directRpcEngine.setProfile({
      walletId: sessionId,
      kind,
      filename: walletFilename.trim(),
      address: profilePatch.address,
      scanHeight: effectiveScanHeight,
      scanTimestamp: effectiveScanTimestamp,
      scanFromCoinbase,
      createdAt: Date.now(),
      sourceFingerprint: profilePatch.sourceFingerprint,
      hasMnemonicSeed: profilePatch.hasMnemonicSeed ?? false,
      hasPrivateKeys: profilePatch.hasPrivateKeys ?? false
    });
    const status = await directRpcEngine.start(sessionId, toRpcNode(selectedNode));
    setDirectWalletId(sessionId);
    setWalletTab("overview");
    setDirectStatus(status);
    setDirectSummary(await directRpcEngine.getSummary());
    setDirectProfile(await directRpcEngine.getProfile());
    setDirectCursor(await directRpcEngine.getCursor());
    setDirectSyncStats(await directRpcEngine.getSyncStats());
    setTxHistory(await directRpcEngine.getTransactionHistory(40));
    if (!suppressOutput) {
      setOutput(
        `Direct RPC ${kind} session started on ${buildNodeHttpUrl(selectedNode)}. Scan starts from height ${effectiveScanHeight} with timestamp ${effectiveScanTimestamp} (earlier point is used).`
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
      hasPrivateKeys: true,
      address: generated.address
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
    const parsedScanTimestamp = validateScanTimestamp();
    if (parsedScanTimestamp === null) {
      setOutput("Scan timestamp must be a non-negative integer.");
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
      scanHeight: scanFrom,
      scanTimestamp: parsedScanTimestamp
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
    const parsedScanTimestamp = validateScanTimestamp();
    if (parsedScanTimestamp === null) {
      setOutput("Scan timestamp must be a non-negative integer.");
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
      scanHeight: parsedScanHeight,
      scanTimestamp: parsedScanTimestamp
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

  const onProbeNodeLatencies = async (): Promise<void> => {
    const results = await Promise.all(
      nodes.map(async (node) => {
        try {
          const probe = await directRpcEngine.probeNode(toRpcNode(node));
          return [node.id, probe.ok ? (probe.latencyMs ?? null) : null] as const;
        } catch {
          return [node.id, null] as const;
        }
      })
    );
    const next: Record<string, number | null> = {};
    for (const [id, latency] of results) {
      next[id] = latency;
    }
    setNodeLatencyById(next);
    setOutput(`Node probe updated for ${results.length} node(s).`);
  };

  const onGenerateReceivePaymentId = (): void => {
    setReceivePaymentId(randomHex(64));
  };

  const onBuildIntegratedAddress = async (): Promise<void> => {
    const validStandard = await validateWalletAddress(receiveAddress, false);
    if (!validStandard) {
      return;
    }
    const normalizedPaymentId = receivePaymentId.trim().toLowerCase();
    if (!/^[0-9a-f]{16}$|^[0-9a-f]{64}$/.test(normalizedPaymentId)) {
      setOutput("Payment ID must be 16 or 64 hex chars.");
      return;
    }
    try {
      const integrated = await directRpcEngine.createIntegratedAddress(receiveAddress, normalizedPaymentId);
      if (!integrated) {
        setOutput("Unable to create integrated address.");
        return;
      }
      setReceiveIntegratedAddress(integrated);
      setOutput("Integrated address generated.");
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
    }
  };

  const fetchJsonRpcMethod = async (method: string): Promise<Record<string, unknown> | null> => {
    if (!selectedNode) {
      return null;
    }
    const response = await fetch(`${buildNodeHttpUrl(selectedNode)}/json_rpc`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ jsonrpc: "2.0", id: "0", method })
    });
    if (!response.ok) {
      return null;
    }
    const parsed = (await response.json()) as { result?: Record<string, unknown> };
    return parsed.result ?? null;
  };

  const onReviewTransfer = async (): Promise<void> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return;
    }
    const valid = await validateWalletAddress(transferAddress, true);
    if (!valid) {
      return;
    }
    const amountAtomic = parseCoinAmountToAtomic(transferAmount, COIN_DECIMALS);
    if (amountAtomic === null || amountAtomic <= 0n) {
      setOutput(`Transfer amount is invalid. Use up to ${COIN_DECIMALS} decimals.`);
      return;
    }

    let nodeFeeAtomic = 0n;
    let nodeFeeAddress = "";
    try {
      const feeRes = await fetch(`${buildNodeHttpUrl(selectedNode)}/fee`);
      if (feeRes.ok) {
        const payload = (await feeRes.json()) as { amount?: number | string; fee?: number | string; address?: string };
        nodeFeeAtomic = BigInt(String(payload.amount ?? payload.fee ?? 0));
        nodeFeeAddress = String(payload.address ?? "");
      }
    } catch {
      // Ignore; node fee endpoint is optional.
    }

    let networkFeeAtomic = 0n;
    if (useDynamicFee) {
      const dynamicMethods = ["getfeeinfo", "get_fee_info", "feeinfo"];
      for (const method of dynamicMethods) {
        try {
          const result = await fetchJsonRpcMethod(method);
          if (!result) {
            continue;
          }
          const candidate = result.fee ?? result.minimumFee ?? result.minFee ?? result.defaultFee;
          if (candidate !== undefined) {
            networkFeeAtomic = BigInt(String(candidate));
            break;
          }
        } catch {
          continue;
        }
      }
    }

    const totalAtomic = amountAtomic + networkFeeAtomic + nodeFeeAtomic;
    const confirmed = window.confirm(
      [
        "Confirm transfer:",
        `Recipient: ${transferAddress.trim()}`,
        `Amount: ${formatAtomicAmount(amountAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}`,
        `Network fee: ${formatAtomicAmount(networkFeeAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}`,
        `Node fee: ${formatAtomicAmount(nodeFeeAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}${nodeFeeAddress ? ` (${nodeFeeAddress})` : ""}`,
        `Total debit: ${formatAtomicAmount(totalAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}`,
        ""
      ].join("\n")
    );
    if (!confirmed) {
      setOutput("Transfer cancelled by user.");
      return;
    }

    setOutput("Transfer preflight confirmed. Submission to remote node is not enabled in this build yet.");
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

  const onLogoutSession = async (): Promise<void> => {
    if (!hasActiveWalletSession) {
      setOutput("No active wallet session.");
      return;
    }
    await directRpcEngine.lockVault().catch(() => undefined);
    setIsLocked(true);
    setLoginPassword("");
    setOutput("Logged out. Enter password to unlock.");
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
    setWalletTab("overview");
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
    setScanTimestamp("0");
    setMnemonicSeed("");
    setPrivateSpendKey("");
    setPrivateViewKey("");
    setTransferAddress("");
    setTransferAmount("");
    setTransferPaymentId("");
    setReceivePaymentId("");
    setReceiveIntegratedAddress("");
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
    setWalletTab("overview");
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
        <button
          className={`icon-button ${resolvedTheme === "light" ? "active" : ""}`}
          onClick={() => setTheme("light")}
          title="Light theme"
          aria-label="Light theme"
        >
          <span className="theme-glyph sun" aria-hidden="true" />
        </button>
        <button
          className={`icon-button ${resolvedTheme === "dark" ? "active" : ""}`}
          onClick={() => setTheme("dark")}
          title="Dark theme"
          aria-label="Dark theme"
        >
          <span className="theme-glyph moon" aria-hidden="true" />
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
                  <label className="field-label">Scan timestamp (unix, optional)</label>
                  <input type="number" min={0} value={scanTimestamp} onChange={(e) => setScanTimestamp(e.target.value)} />
                  <label className="checkbox">
                    <input type="checkbox" checked={scanFromCoinbase} onChange={(e) => setScanFromCoinbase(e.target.checked)} />
                    Scan from coinbase (force height 0)
                  </label>
                </>
              ) : (
                <p className="muted">
                  New wallet starts from current network height with current timestamp, not from block 0.
                </p>
              )}
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
              {pendingImportReview ? (
                <section className="panel compact">
                  <h3>Pending Import Review</h3>
                  <p>Address: {pendingImportReview.address}</p>
                  <p>
                    Start point: height {pendingImportReview.scanHeight}, timestamp {pendingImportReview.scanTimestamp}
                  </p>
                  <div className="actions">
                    <button onClick={onConfirmImportReview}>Confirm Import and Start Sync</button>
                    <button className="danger" onClick={() => setPendingImportReview(null)}>
                      Cancel Pending Import
                    </button>
                  </div>
                </section>
              ) : null}
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
              <div className="session-header">
                <h2>Wallet Session</h2>
                <div className="actions">
                  <button onClick={onRefreshDirectStatus}>Refresh</button>
                  <button onClick={onLogoutSession}>Logout</button>
                </div>
              </div>
              <div className="actions tab-row">
                <button className={walletTab === "overview" ? "active" : ""} onClick={() => setWalletTab("overview")}>
                  Overview
                </button>
                <button className={walletTab === "transactions" ? "active" : ""} onClick={() => setWalletTab("transactions")}>
                  Transactions
                </button>
                <button className={walletTab === "balances" ? "active" : ""} onClick={() => setWalletTab("balances")}>
                  Balances
                </button>
                <button className={walletTab === "transfer" ? "active" : ""} onClick={() => setWalletTab("transfer")}>
                  Transfer
                </button>
                <button className={walletTab === "receive" ? "active" : ""} onClick={() => setWalletTab("receive")}>
                  Receive
                </button>
                <button className={walletTab === "nodes" ? "active" : ""} onClick={() => setWalletTab("nodes")}>
                  Nodes
                </button>
                <button className={walletTab === "backup" ? "active" : ""} onClick={() => setWalletTab("backup")}>
                  Backup
                </button>
              </div>

              {walletTab === "overview" ? (
                <>
                  <section className="session-grid">
                    <article className="session-card">
                      <h3>Sync</h3>
                      <p className="metric">{syncPercentLabel}</p>
                      <p className="muted">
                        {directSyncStats ? `${directSyncStats.syncedHeight} / ${directSyncStats.targetHeight}` : "waiting"}
                      </p>
                      <p className="muted">
                        {directSyncStats ? `${directSyncStats.fetchMode}, batch ${directSyncStats.lastBatchSize}` : ""}
                      </p>
                    </article>
                    <article className="session-card">
                      <h3>Balance</h3>
                      <p className="metric">
                        {formatAtomicAmount(directSummary?.unlockedBalanceAtomic ?? "0", COIN_DECIMALS)} {COIN_TICKER}
                      </p>
                      <p className="muted">
                        Locked {formatAtomicAmount(directSummary?.lockedBalanceAtomic ?? "0", COIN_DECIMALS)} {COIN_TICKER}
                      </p>
                    </article>
                    <article className="session-card">
                      <h3>Node</h3>
                      <p className="metric-small">{directStatus.activeNodeEndpoint ?? "n/a"}</p>
                      <p className="muted">Failovers {directStatus.failoverCount ?? 0}</p>
                    </article>
                  </section>
                  <details className="panel compact">
                    <summary>Diagnostics</summary>
                    <p>Wallet ID: {directWalletId}</p>
                    {directProfile ? (
                      <p>
                        Profile: {directProfile.kind}, scan height {directProfile.scanHeight}, scan timestamp{" "}
                        {directProfile.scanTimestamp ?? 0}
                      </p>
                    ) : null}
                    {directCursor ? <p>Cursor height: {directCursor.height}</p> : null}
                    {directSyncStats?.methodsTried?.length ? <p>Methods: {directSyncStats.methodsTried.join(" -> ")}</p> : null}
                    {directSyncStats?.lastError ? <p>Last error: {directSyncStats.lastError}</p> : null}
                  </details>
                </>
              ) : null}

              {walletTab === "transactions" ? (
                <section>
                  <h3>Transaction History</h3>
                  {txHistory.length === 0 ? <p className="muted">No wallet-owned transactions scanned yet.</p> : null}
                  <ul className="node-list">
                    {txHistory.slice(0, 25).map((entry) => (
                      <li key={`${entry.txHash}-${entry.blockHeight}`} className="node-item">
                        <span className="node-meta">#{entry.blockHeight}</span>
                        <span className="node-meta">{entry.direction}</span>
                        <span className="node-meta">
                          {formatAtomicAmount(entry.netAtomic, COIN_DECIMALS)} {COIN_TICKER}
                        </span>
                        <code>{entry.txHash}</code>
                      </li>
                    ))}
                  </ul>
                </section>
              ) : null}

              {walletTab === "balances" ? (
                <section>
                  <h3>Balances and Scan Stats</h3>
                  <p>
                    Unlocked: {formatAtomicAmount(directSummary?.unlockedBalanceAtomic ?? "0", COIN_DECIMALS)} {COIN_TICKER}
                  </p>
                  <p>
                    Locked: {formatAtomicAmount(directSummary?.lockedBalanceAtomic ?? "0", COIN_DECIMALS)} {COIN_TICKER}
                  </p>
                  <p>
                    Scan mode: {directSummary?.scanMode === "wallet_owned_outputs" ? "wallet-owned outputs" : "headers only"}
                  </p>
                  <p>
                    Outputs: unspent {directSummary?.unspentOwnedOutputs ?? 0}, spent {directSummary?.spentOwnedOutputs ?? 0}
                  </p>
                  <p>
                    Headers stored: {directSummary?.scannedHeaderCount ?? 0}, scanned tx {directSummary?.scannedTransactions ?? 0}
                  </p>
                </section>
              ) : null}

              {walletTab === "transfer" ? (
                <section>
                  <h3>Transfer</h3>
                  <label className="field-label">Recipient address</label>
                  <input type="text" value={transferAddress} onChange={(e) => setTransferAddress(e.target.value)} />
                  {transferAddressValid !== null ? (
                    <p className={transferAddressValid ? "ok-badge" : "error-badge"}>
                      {transferAddressValid ? "Valid address" : `Invalid address: ${transferAddressReason}`}
                    </p>
                  ) : null}
                  <label className="field-label">Amount ({COIN_TICKER})</label>
                  <input type="text" value={transferAmount} onChange={(e) => setTransferAmount(e.target.value)} />
                  <label className="field-label">Payment ID (optional)</label>
                  <input type="text" value={transferPaymentId} onChange={(e) => setTransferPaymentId(e.target.value)} />
                  <p className="muted">
                    Automatic fee preflight is enabled. You must confirm recipient, amount, network fee and node fee before submit.
                  </p>
                  <div className="actions">
                    <button onClick={onReviewTransfer}>Review Transfer</button>
                    <button disabled>Submit Transfer (coming soon)</button>
                  </div>
                </section>
              ) : null}

              {walletTab === "receive" ? (
                <section>
                  <h3>Receive</h3>
                  <label className="field-label">Standard address</label>
                  <input type="text" value={receiveAddress} readOnly />
                  <div className="actions">
                    <button disabled={!receiveAddress} onClick={onCopyStandardReceiveAddress}>
                      Copy Address
                    </button>
                  </div>
                  <label className="field-label">Payment ID (optional for integrated address)</label>
                  <input
                    type="text"
                    value={receivePaymentId}
                    onChange={(e) => setReceivePaymentId(e.target.value)}
                    placeholder="16 or 64 hex chars"
                  />
                  <div className="actions">
                    <button onClick={onGenerateReceivePaymentId}>Generate Payment ID</button>
                    <button onClick={onBuildIntegratedAddress}>Generate Integrated Address</button>
                    <button onClick={() => setReceiveIntegratedAddress("")}>Use Standard Address</button>
                  </div>
                  {receiveIntegratedAddress ? (
                    <>
                      <label className="field-label">Integrated address</label>
                      <textarea className="input-area" value={receiveIntegratedAddress} readOnly />
                      <div className="actions">
                        <button onClick={onCopyIntegratedReceiveAddress}>
                          Copy Integrated
                        </button>
                      </div>
                    </>
                  ) : null}
                  {receiveQrSrc ? (
                    <div className="qr-wrap">
                      <img src={receiveQrSrc} alt="Receive QR" />
                    </div>
                  ) : (
                    <p className="muted">Wallet address is required to render QR.</p>
                  )}
                </section>
              ) : null}

              {walletTab === "nodes" ? (
                <section>
                  <div className="session-header">
                    <h3>Remote Nodes</h3>
                    <div className="actions">
                      <button onClick={onProbeNodeLatencies}>Probe Nodes</button>
                      <button onClick={onRefreshNodeScores}>Refresh Scores</button>
                    </div>
                  </div>
                  <ul className="node-list">
                    {nodes
                      .slice()
                      .sort((a, b) => a.priority - b.priority)
                      .map((node) => {
                        const score = nodeScores.find((item) => item.nodeId === node.id);
                        const latency = nodeLatencyById[node.id];
                        const isActive = defaultNodeId === node.id;
                        const isAlive = latency !== null || (score?.successCount ?? 0) > 0;
                        return (
                          <li key={node.id} className="node-item">
                            <code>{buildNodeHttpUrl(node)}</code>
                            <span className="node-meta">latency: {latency === null || latency === undefined ? "n/a" : `${latency} ms`}</span>
                            <span className="node-meta">score: {score?.score ?? "-"}</span>
                            <span className="node-meta">{isAlive ? "alive" : "unknown/offline"}</span>
                            {isActive ? (
                              <button className="active" disabled>
                                Active
                              </button>
                            ) : (
                              <button disabled={!isAlive} onClick={() => onUseNodeForWallet(node)}>
                                Switch
                              </button>
                            )}
                          </li>
                        );
                      })}
                  </ul>
                </section>
              ) : null}

              {walletTab === "backup" ? (
                <section>
                  <h3>Backup</h3>
                  {pendingBackup ? (
                    <>
                      <p className="muted">Save these secrets securely. Anyone with these can spend your funds.</p>
                      <label className="field-label">Address</label>
                      <textarea className="input-area" value={pendingBackup.address} readOnly />
                      <div className="actions">
                        <button onClick={() => copyText(pendingBackup.address, "Address", true)}>Copy Address</button>
                      </div>
                      <label className="field-label">Mnemonic seed</label>
                      <textarea className="input-area" value={pendingBackup.mnemonicSeed} readOnly />
                      <div className="actions">
                        <button onClick={() => copyText(pendingBackup.mnemonicSeed, "Mnemonic seed", true)}>Copy Seed</button>
                      </div>
                      <label className="field-label">Private spend key</label>
                      <input type="text" value={pendingBackup.privateSpendKey} readOnly />
                      <label className="field-label">Private view key</label>
                      <input type="text" value={pendingBackup.privateViewKey} readOnly />
                      <div className="actions">
                        <button onClick={() => copyText(pendingBackup.privateSpendKey, "Private spend key", true)}>
                          Copy Spend Key
                        </button>
                        <button onClick={() => copyText(pendingBackup.privateViewKey, "Private view key", true)}>
                          Copy View Key
                        </button>
                        <button onClick={onConfirmBackupSaved}>
                          {pendingBackup.confirmed ? "Backup Confirmed" : "I Backed This Up"}
                        </button>
                        <button className="danger" onClick={onClearBackupFromScreen}>
                          Clear From Screen
                        </button>
                      </div>
                    </>
                  ) : (
                    <p className="muted">
                      No backup payload is currently cached in this session. Backup is shown immediately after new wallet create.
                    </p>
                  )}
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
