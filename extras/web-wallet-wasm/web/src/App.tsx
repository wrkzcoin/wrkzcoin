import { useEffect, useMemo, useRef, useState } from "react";
import { DEFAULT_NODES, DEFAULT_RPC_PORT, DEFAULT_RPC_SSL } from "./config/nodes";
import { COIN_ADDRESS_PREFIX, COIN_DECIMALS, COIN_TICKER, formatAtomicAmount } from "./config/coin";
import { RESCAN_HEIGHT_WARN_THRESHOLD } from "./config/sync";
import { WEB_WALLET_UI_VERSION } from "./config/version";
import { buildNodeHttpUrl, buildNodeRpcUrl, normalizeNodeHost, type NodeEndpoint } from "./wallet/types";
import {
  BrowserDirectRpcEngine,
  type DirectRpcSessionProfile,
  type NodeCapabilities,
  type NodeScore,
  type RpcEngineStatus,
  type RpcNode,
  type ScannerSnapshotState,
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
const SETTINGS_WELCOME_MODAL_DISMISSED_KEY = "wrkz_web_wallet_welcome_modal_dismissed_v1";
const DEFAULT_AUTO_LOGOUT_MIN = 15;
const TX_PAGE_SIZE = 10;

type ViewTab = "wallet" | "settings";
type WalletTab =
  | "overview"
  | "transactions"
  | "transfer"
  | "receive"
  | "nodes";
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
  copiedSeed: boolean;
  copiedSpend: boolean;
  copiedView: boolean;
  copiedAddress: boolean;
};

type SyncHealthState = {
  avgBlocksPerSec: number;
  etaSeconds: number | null;
  stagnantPolls: number;
  lastDeltaBlocks: number;
  lastDeltaSeconds: number;
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

function loadWelcomeModalDismissed(): boolean {
  try {
    return localStorage.getItem(SETTINGS_WELCOME_MODAL_DISMISSED_KEY) === "true";
  } catch {
    return false;
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

function formatAgeFromTs(ts?: number): string {
  if (!ts || ts <= 0) {
    return "n/a";
  }
  const deltaSec = Math.max(0, Math.floor((Date.now() - ts) / 1000));
  if (deltaSec < 60) {
    return `${deltaSec}s ago`;
  }
  const mins = Math.floor(deltaSec / 60);
  const secs = deltaSec % 60;
  return `${mins}m ${secs}s ago`;
}

function formatEta(seconds: number | null): string {
  if (seconds === null || !Number.isFinite(seconds) || seconds < 0) {
    return "n/a";
  }
  if (seconds < 60) {
    return `${Math.floor(seconds)}s`;
  }
  const mins = Math.floor(seconds / 60);
  const secs = Math.floor(seconds % 60);
  if (mins < 60) {
    return `${mins}m ${secs}s`;
  }
  const hours = Math.floor(mins / 60);
  const remMins = mins % 60;
  return `${hours}h ${remMins}m`;
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
  const [directSnapshotState, setDirectSnapshotState] = useState<ScannerSnapshotState | null>(null);
  const [directSyncStats, setDirectSyncStats] = useState<SyncRuntimeStats | null>(null);
  const [selectedNodeCapabilities, setSelectedNodeCapabilities] = useState<NodeCapabilities | null>(null);
  const [nodeScores, setNodeScores] = useState<NodeScore[]>([]);
  const [scanHistory, setScanHistory] = useState<ScannedHeaderEntry[]>([]);
  const [txHistory, setTxHistory] = useState<WalletTxHistoryEntry[]>([]);
  const [nodeLatencyById, setNodeLatencyById] = useState<Record<string, number | null>>({});
  const [pendingImportReview, setPendingImportReview] = useState<ImportReview | null>(null);
  const [pendingBackup, setPendingBackup] = useState<BackupState | null>(null);
  const [copiedFlash, setCopiedFlash] = useState<Record<string, boolean>>({});
  const [welcomeModalOpen, setWelcomeModalOpen] = useState<boolean>(() => !loadWelcomeModalDismissed());
  const [hideWelcomeModalNextTime, setHideWelcomeModalNextTime] = useState<boolean>(false);
  const [mobileMenuOpen, setMobileMenuOpen] = useState<boolean>(false);

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
  const [backupSeedConfirmInput, setBackupSeedConfirmInput] = useState<string>("");
  const [rescanHeightInput, setRescanHeightInput] = useState<string>("0");
  const [txPage, setTxPage] = useState<number>(1);
  const [txHeightFilterInput, setTxHeightFilterInput] = useState<string>("");
  const [syncHealth, setSyncHealth] = useState<SyncHealthState>({
    avgBlocksPerSec: 0,
    etaSeconds: null,
    stagnantPolls: 0,
    lastDeltaBlocks: 0,
    lastDeltaSeconds: 0
  });
  const lastSyncRef = useRef<{ syncedHeight: number; updatedAt: number; walletId: string } | null>(null);
  const bootstrapDoneRef = useRef<boolean>(false);

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
  const unlockedAtomic = BigInt(directSummary?.unlockedBalanceAtomic ?? "0");
  const maxTransferAmount = formatAtomicAmount(unlockedAtomic.toString(), COIN_DECIMALS);
  const txHeightFilter = Number(txHeightFilterInput.trim() || "0");
  const filteredTxHistory = useMemo(() => {
    if (!Number.isInteger(txHeightFilter) || txHeightFilter <= 0) {
      return txHistory;
    }
    return txHistory.filter((entry) => entry.blockHeight >= txHeightFilter);
  }, [txHistory, txHeightFilter]);
  const txTotalPages = Math.max(1, Math.ceil(filteredTxHistory.length / TX_PAGE_SIZE));
  const txCurrentPage = Math.min(txPage, txTotalPages);
  const txPageItems = useMemo(() => {
    const start = (txCurrentPage - 1) * TX_PAGE_SIZE;
    return filteredTxHistory.slice(start, start + TX_PAGE_SIZE);
  }, [filteredTxHistory, txCurrentPage]);
  const formatTxType = (direction: WalletTxHistoryEntry["direction"]): string => {
    if (direction === "incoming") {
      return "Incoming";
    }
    if (direction === "outgoing") {
      return "Outgoing";
    }
    return "Outgoing + Change";
  };
  const formatTxTime = (timestamp?: number): string => {
    if (!timestamp || timestamp <= 0) {
      return "-";
    }
    try {
      return new Date(timestamp * 1000).toLocaleString();
    } catch {
      return String(timestamp);
    }
  };
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
    setTxPage(1);
  }, [txHeightFilterInput, txHistory.length]);

  useEffect(() => {
    if (!directSyncStats || !directWalletId) {
      setSyncHealth({
        avgBlocksPerSec: 0,
        etaSeconds: null,
        stagnantPolls: 0,
        lastDeltaBlocks: 0,
        lastDeltaSeconds: 0
      });
      lastSyncRef.current = null;
      return;
    }

    const prev = lastSyncRef.current;
    if (!prev || prev.walletId !== directWalletId) {
      const initialEta = directSyncStats.remainingBlocks > 0 ? null : 0;
      setSyncHealth({
        avgBlocksPerSec: 0,
        etaSeconds: initialEta,
        stagnantPolls: 0,
        lastDeltaBlocks: 0,
        lastDeltaSeconds: 0
      });
      lastSyncRef.current = {
        syncedHeight: directSyncStats.syncedHeight,
        updatedAt: directSyncStats.updatedAt,
        walletId: directWalletId
      };
      return;
    }

    const deltaBlocks = Math.max(0, directSyncStats.syncedHeight - prev.syncedHeight);
    const deltaSeconds = Math.max(0, (directSyncStats.updatedAt - prev.updatedAt) / 1000);
    setSyncHealth((current) => {
      const instantBps = deltaSeconds > 0 ? deltaBlocks / deltaSeconds : 0;
      const nextAvg = instantBps > 0 ? (current.avgBlocksPerSec > 0 ? (current.avgBlocksPerSec * 0.7) + (instantBps * 0.3) : instantBps) : current.avgBlocksPerSec;
      const remaining = Math.max(0, directSyncStats.remainingBlocks);
      const nextEta = remaining === 0 ? 0 : (nextAvg > 0 ? (remaining / nextAvg) : null);
      return {
        avgBlocksPerSec: nextAvg,
        etaSeconds: nextEta,
        stagnantPolls: deltaBlocks === 0 ? current.stagnantPolls + 1 : 0,
        lastDeltaBlocks: deltaBlocks,
        lastDeltaSeconds: deltaSeconds
      };
    });

    lastSyncRef.current = {
      syncedHeight: directSyncStats.syncedHeight,
      updatedAt: directSyncStats.updatedAt,
      walletId: directWalletId
    };
  }, [directSyncStats, directWalletId]);

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
    if (bootstrapDoneRef.current) {
      return;
    }
    if (nodes.length === 0) {
      return;
    }
    bootstrapDoneRef.current = true;
    let cancelled = false;
    const bootstrap = async (): Promise<void> => {
      const [savedStatus, savedProfile] = await Promise.all([
        directRpcEngine.getStatus().catch(() => ({ running: false } as RpcEngineStatus)),
        directRpcEngine.getProfile().catch(() => null)
      ]);

      if (cancelled) {
        return;
      }

      if (savedStatus.running && savedProfile?.walletId) {
        const eligibleNodes = nodes.filter((node) => (isSecurePage ? node.ssl : true));
        const resumeNode =
          eligibleNodes.find((node) => node.id === savedStatus.nodeId) ??
          eligibleNodes.find((node) => node.id === defaultNodeId) ??
          eligibleNodes[0];
        if (resumeNode) {
          try {
            await directRpcEngine.start(savedProfile.walletId, toRpcNode(resumeNode));
            if (!cancelled) {
              setOutput(`Session auto-resumed on ${buildNodeHttpUrl(resumeNode)}.`);
            }
          } catch (error) {
            await directRpcEngine.stop().catch(() => undefined);
            if (!cancelled) {
              setOutput(`Auto-resume failed: ${error instanceof Error ? error.message : String(error)}`);
            }
          }
        } else if (!cancelled) {
          setOutput("Auto-resume skipped: no eligible node available.");
        }
      }

      const [status, summary, profile, cursor, snapshotState, stats, scores, history, txs] = await Promise.all([
        directRpcEngine.getStatus().catch(() => ({ running: false } as RpcEngineStatus)),
        directRpcEngine.getSummary().catch(() => null),
        directRpcEngine.getProfile().catch(() => null),
        directRpcEngine.getCursor().catch(() => null),
        directRpcEngine.getScannerSnapshotState().catch(() => null),
        directRpcEngine.getSyncStats().catch(() => null),
        directRpcEngine.getNodeScores().catch(() => [] as NodeScore[]),
        directRpcEngine.getHistory(30).catch(() => [] as ScannedHeaderEntry[]),
        directRpcEngine.getTransactionHistory(40).catch(() => [] as WalletTxHistoryEntry[])
      ]);

      if (cancelled) {
        return;
      }

      setDirectStatus(status);
      setDirectSummary(summary);
      setDirectProfile(profile);
      setDirectCursor(cursor);
      setDirectSnapshotState(snapshotState);
      setDirectSyncStats(stats);
      setNodeScores(scores);
      setScanHistory(history);
      setTxHistory(txs);
      setDirectWalletId(status.running && profile?.walletId ? profile.walletId : null);
    };

    bootstrap().catch((error) => {
      if (!cancelled) {
        setOutput(`Startup failed: ${error instanceof Error ? error.message : String(error)}`);
      }
    });

    return () => {
      cancelled = true;
    };
  }, [directRpcEngine, nodes, defaultNodeId, isSecurePage]);

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
        .getScannerSnapshotState()
        .then((snapshotState) => setDirectSnapshotState(snapshotState))
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

  const validateImportStartPoint = (height: number): string | null => {
    if (scanFromCoinbase) {
      return null;
    }
    if (height <= 0) {
      return "For import, set scan height to a value greater than 0.";
    }
    return null;
  };

  const triggerCopyFlash = (key: string): void => {
    setCopiedFlash((prev) => ({ ...prev, [key]: true }));
    window.setTimeout(() => {
      setCopiedFlash((prev) => ({ ...prev, [key]: false }));
    }, 1400);
  };

  const copyText = async (value: string, label: string, flashKey?: string): Promise<boolean> => {
    try {
      await navigator.clipboard.writeText(value);
      setOutput(`${label} copied to clipboard.`);
      if (flashKey) {
        triggerCopyFlash(flashKey);
      }
      return true;
    } catch {
      setOutput(`Unable to copy ${label}.`);
      return false;
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
    await copyText(receiveAddress, "Standard address", "receive-standard");
  };

  const onCopyIntegratedReceiveAddress = async (): Promise<void> => {
    const valid = await validateWalletAddress(receiveIntegratedAddress, true);
    if (!valid) {
      return;
    }
    await copyText(receiveIntegratedAddress, "Integrated address", "receive-integrated");
  };

  const markBackupCopied = (field: "seed" | "spend" | "view" | "address"): void => {
    setPendingBackup((prev) => {
      if (!prev) {
        return prev;
      }
      if (field === "seed") {
        return { ...prev, copiedSeed: true };
      }
      if (field === "spend") {
        return { ...prev, copiedSpend: true };
      }
      if (field === "view") {
        return { ...prev, copiedView: true };
      }
      return { ...prev, copiedAddress: true };
    });
  };

  const onCopyBackupSecret = async (
    field: "seed" | "spend" | "view" | "address",
    value: string,
    label: string
  ): Promise<void> => {
    const copied = await copyText(value, label, `backup-${field}`);
    if (copied) {
      markBackupCopied(field);
    }
  };

  const onConfirmBackupGate = (): void => {
    if (!pendingBackup) {
      setOutput("No pending backup data.");
      return;
    }
    const copiedAll =
      pendingBackup.copiedSeed && pendingBackup.copiedSpend && pendingBackup.copiedView && pendingBackup.copiedAddress;
    if (!copiedAll) {
      setOutput("Please copy address, seed, spend key and view key first.");
      return;
    }
    if (normalizeSeed(backupSeedConfirmInput) !== normalizeSeed(pendingBackup.mnemonicSeed)) {
      setOutput("Seed confirmation mismatch. Re-enter your seed to continue.");
      return;
    }
    setPendingBackup({ ...pendingBackup, confirmed: true });
    setBackupSeedConfirmInput("");
    setOutput("Backup confirmed. Wallet session unlocked.");
  };

  const onConfirmImportReview = async (): Promise<void> => {
    if (!pendingImportReview) {
      return;
    }
    setOutput("Starting import sync session...");
    try {
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
        { height: pendingImportReview.scanHeight }
      );
      setPendingImportReview(null);
      setOutput(
        `Import confirmed. Address ${pendingImportReview.address} is syncing from height ${pendingImportReview.scanHeight}.`
      );
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
    }
  };

  const startDirectSession = async (
    kind: DirectRpcSessionProfile["kind"],
    sessionId: string,
    profilePatch: Partial<
      Pick<DirectRpcSessionProfile, "sourceFingerprint" | "hasMnemonicSeed" | "hasPrivateKeys" | "address">
    >,
    suppressOutput = false,
    scanOverride?: { height: number }
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
    if (parsedScanHeight === null) {
      setOutput("Scan height must be a non-negative integer.");
      return;
    }
    let effectiveScanHeight = scanOverride?.height ?? parsedScanHeight;
    let effectiveScanTimestamp = 0;
    if (kind === "create") {
      effectiveScanHeight = Math.max(0, probe.height ?? 0);
      effectiveScanTimestamp = 0;
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
    setDirectSnapshotState(await directRpcEngine.getScannerSnapshotState());
    setDirectSyncStats(await directRpcEngine.getSyncStats());
    setTxHistory(await directRpcEngine.getTransactionHistory(40));
    if (!suppressOutput) {
      setOutput(
        `Direct RPC ${kind} session started on ${buildNodeHttpUrl(selectedNode)}. Scan starts from height ${effectiveScanHeight}.`
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
    setOutput("Creating wallet keys...");
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
    try {
      await startDirectSession("create", sessionId, {
        hasMnemonicSeed: true,
        hasPrivateKeys: true,
        address: generated.address
      }, true);
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
      return;
    }
    setPendingBackup({
      mnemonicSeed: generated.mnemonicSeed,
      privateSpendKey: generated.privateSpendKey,
      privateViewKey: generated.privateViewKey,
      address: generated.address,
      confirmed: false,
      copiedSeed: false,
      copiedSpend: false,
      copiedView: false,
      copiedAddress: false
    });
    setBackupSeedConfirmInput("");
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
    const startPointError = validateImportStartPoint(parsedScanHeight);
    if (startPointError) {
      setOutput(startPointError);
      return;
    }
    setOutput("Deriving keys from mnemonic seed...");
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
    const startPointError = validateImportStartPoint(parsedScanHeight);
    if (startPointError) {
      setOutput(startPointError);
      return;
    }
    setOutput("Validating private keys...");
    const normalizedSpend = privateSpendKey.trim().toLowerCase();
    const normalizedView = privateViewKey.trim().toLowerCase();
    try {
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
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
    }
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

  const onResetScanFromHeight = async (): Promise<void> => {
    if (!hasActiveWalletSession || !directWalletId) {
      setOutput("Open a wallet session first.");
      return;
    }
    const parsed = Number(rescanHeightInput.trim());
    if (!Number.isInteger(parsed) || parsed < 0) {
      setOutput("Rescan height must be a non-negative integer.");
      return;
    }
    if (parsed > RESCAN_HEIGHT_WARN_THRESHOLD) {
      const largeConfirm = window.confirm(
        `Rescan from height ${parsed} is more than ${RESCAN_HEIGHT_WARN_THRESHOLD}. Continue?`
      );
      if (!largeConfirm) {
        setOutput("Rescan cancelled.");
        return;
      }
    }

    const networkHeight = Number(directSummary?.daemonHeight ?? 0);
    if (networkHeight > 0 && parsed > networkHeight) {
      const reenter = window.confirm(
        `Rescan height ${parsed} is beyond network height ${networkHeight}. Press OK to re-enter, or Cancel to abort.`
      );
      if (reenter) {
        setOutput(`Height is beyond network height (${networkHeight}). Please re-enter height.`);
      } else {
        setOutput("Rescan cancelled.");
      }
      return;
    }

    await directRpcEngine.resetScanFromHeight(parsed, 0);
    const status = await directRpcEngine.getStatus();
    const summary = await directRpcEngine.getSummary();
    const cursor = await directRpcEngine.getCursor();
    const stats = await directRpcEngine.getSyncStats();
    const history = await directRpcEngine.getHistory(30);
    const txs = await directRpcEngine.getTransactionHistory(40);
    setDirectStatus(status);
    setDirectSummary(summary);
    setDirectCursor(cursor);
    setDirectSyncStats(stats);
    setScanHistory(history);
    setTxHistory(txs);
    setWalletTab("overview");
    setOutput(`Rescan reset to height ${parsed}. Sync restarted.`);
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

  const onGenerateReceivePaymentIdShort = (): void => {
    setReceivePaymentId(randomHex(16));
  };

  const onGenerateReceivePaymentIdLong = (): void => {
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
      const result = await directRpcEngine.createIntegratedAddress(receiveAddress, normalizedPaymentId);
      if (!result.integratedAddress) {
        setOutput(result.error ? `Unable to create integrated address: ${result.error}` : "Unable to create integrated address.");
        return;
      }
      setReceiveIntegratedAddress(result.integratedAddress);
      setOutput("Integrated address generated.");
    } catch (error) {
      setOutput(error instanceof Error ? error.message : String(error));
    }
  };

  const runTransferPreflight = async (walletPasswordForPreflight?: string): Promise<{
    recipient: string;
    paymentId: string;
    amountAtomic: bigint;
    networkFeeAtomic: bigint;
    totalAtomic: bigint;
    preparedTxHash: string;
  } | null> => {
    if (!selectedNode) {
      setOutput("No default node selected.");
      return null;
    }
    const valid = await validateWalletAddress(transferAddress, true);
    if (!valid) {
      return null;
    }
    const amountAtomic = parseCoinAmountToAtomic(transferAmount, COIN_DECIMALS);
    if (amountAtomic === null || amountAtomic <= 0n) {
      setOutput(`Transfer amount is invalid. Use up to ${COIN_DECIMALS} decimals.`);
      return null;
    }
    const paymentId = transferPaymentId.trim();
    if (paymentId.length > 0 && !/^[0-9a-fA-F]{16}$|^[0-9a-fA-F]{64}$/.test(paymentId)) {
      setOutput("Payment ID must be 16 or 64 hex chars.");
      return null;
    }

    let networkFeeAtomic = 0n;
    let preparedTxHash = "";
    try {
      const prepared = await directRpcEngine.prepareBasicTransfer({
        destination: transferAddress.trim(),
        amountAtomic: amountAtomic.toString(),
        paymentId,
        password: walletPasswordForPreflight ?? "",
        filenameHint: walletFilename
      });
      preparedTxHash = prepared.preparedTxHash;
      networkFeeAtomic = BigInt(prepared.feeAtomic);
    } catch (error) {
      const reason = error instanceof Error ? error.message : String(error);
      setOutput(`Unable to prepare transfer fee: ${reason}`);
      return null;
    }

    if (networkFeeAtomic <= 0n) {
      await directRpcEngine.discardPreparedTransfer().catch(() => undefined);
      setOutput("Unable to prepare transfer fee: wallet returned zero estimated fee.");
      return null;
    }

    const totalAtomic = amountAtomic + networkFeeAtomic;
    if (totalAtomic > unlockedAtomic) {
      await directRpcEngine.discardPreparedTransfer().catch(() => undefined);
      setOutput(
        `Amount + fees exceed available balance (${maxTransferAmount} ${COIN_TICKER}). ` +
        `Required: ${formatAtomicAmount(totalAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}.`
      );
      return null;
    }
    const recipient = transferAddress.trim();
    return { recipient, paymentId, amountAtomic, networkFeeAtomic, totalAtomic, preparedTxHash };
  };

  const onSubmitTransfer = async (): Promise<void> => {
    const password = walletPassword.trim() || window.prompt("Enter wallet password to prepare and submit transaction:")?.trim() || "";
    if (!password) {
      setOutput("Wallet password is required to submit transfer.");
      return;
    }
    const preflight = await runTransferPreflight(password);
    if (!preflight) {
      return;
    }
    const confirmed = window.confirm(
      [
        "Submit transfer now?",
        `Recipient: ${preflight.recipient}`,
        `Amount: ${formatAtomicAmount(preflight.amountAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}`,
        `Network fee: ${formatAtomicAmount(preflight.networkFeeAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}`,
        `Total debit: ${formatAtomicAmount(preflight.totalAtomic.toString(), COIN_DECIMALS)} ${COIN_TICKER}`
      ].join("\n")
    );
    if (!confirmed) {
      await directRpcEngine.discardPreparedTransfer().catch(() => undefined);
      setOutput("Transfer submission cancelled by user.");
      return;
    }

    try {
      const sent = await directRpcEngine.submitPreparedTransfer(preflight.preparedTxHash);
      setOutput(`Transfer submitted. Tx hash: ${sent.txHash}`);
      setTransferAmount("");
      setTransferPaymentId("");
    } catch (error) {
      await directRpcEngine.discardPreparedTransfer().catch(() => undefined);
      setOutput(error instanceof Error ? `Transfer failed: ${error.message}` : `Transfer failed: ${String(error)}`);
    }
  };

  const onTransferAmountChange = (value: string): void => {
    const normalized = value.replace(",", ".").trim();
    if (normalized === "") {
      setTransferAmount("");
      return;
    }
    if (!/^\d*(\.\d{0,2})?$/.test(normalized)) {
      return;
    }
    const parsedAtomic = parseCoinAmountToAtomic(normalized, COIN_DECIMALS);
    if (parsedAtomic === null) {
      return;
    }
    if (parsedAtomic > unlockedAtomic) {
      setTransferAmount(maxTransferAmount);
      return;
    }
    setTransferAmount(normalized);
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
    const firstConfirm = window.confirm("Reset wallet local data? This action is destructive and cannot be undone.");
    if (!firstConfirm) {
      return;
    }
    const secondConfirm = window.confirm(
      "Final warning: this will delete wallet session data, encrypted vault data, and local settings in this browser. Continue?"
    );
    if (!secondConfirm) {
      return;
    }
    const passwordPrompt = window.prompt("Enter wallet password to confirm reset:");
    if (passwordPrompt === null) {
      setOutput("Wallet reset cancelled.");
      return;
    }
    if (authHash) {
      const computed = await sha256Hex(`pwd:${passwordPrompt}`);
      if (computed !== authHash) {
        setOutput("Reset aborted: incorrect password.");
        return;
      }
    } else if (passwordPrompt.trim().length === 0) {
      setOutput("Reset aborted: password confirmation is required.");
      return;
    }
    // Keep user preferences (theme, nodes, default node, scan defaults, tx/fusion toggles).
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
    setRescanHeightInput("0");
    await directRpcEngine.stop();
    await directRpcEngine.clearScanKeys();
    await directRpcEngine.lockVault();
    await directRpcEngine.clearProfile();
    setDirectWalletId(null);
    setDirectStatus({ running: false });
    setDirectSummary(null);
    setDirectProfile(null);
    setDirectCursor(null);
    setDirectSnapshotState(null);
    setDirectSyncStats(null);
    setNodeScores([]);
    setScanHistory([]);
    setTxHistory([]);
    setPendingBackup(null);
    setPendingImportReview(null);
    setWalletTab("overview");
    setActiveView("wallet");
    setWelcomeMode("create");
    setAuthHash("");
    setIsLocked(false);
    setLoginPassword("");
    localStorage.removeItem(SETTINGS_AUTH_HASH_KEY);
    // Keep welcome modal dismissal preference too.
    setWelcomeModalOpen(true);
    setHideWelcomeModalNextTime(false);
    setOutput("Wallet local vault data has been reset. Preferences were kept.");
  };

  const onDismissWelcomeModal = (): void => {
    if (hideWelcomeModalNextTime) {
      localStorage.setItem(SETTINGS_WELCOME_MODAL_DISMISSED_KEY, "true");
    }
    setWelcomeModalOpen(false);
  };

  const onSelectView = (view: ViewTab): void => {
    setActiveView(view);
    setMobileMenuOpen(false);
  };

  const onSelectWalletTab = (tab: WalletTab): void => {
    setWalletTab(tab);
    setMobileMenuOpen(false);
  };

  const disableScanHeightInput = scanFromCoinbase;

  return (
    <main className="container">
      {!hasActiveWalletSession && welcomeModalOpen ? (
        <div className="modal-backdrop" role="dialog" aria-modal="true" aria-labelledby="welcome-modal-title">
          <section className="modal-card">
            <h2 id="welcome-modal-title">Before You Start</h2>
            <ul className="modal-list">
              <li>This is a non-custodial web wallet. You must backup and safely store your seed and private keys.</li>
              <li>No wallet logs or database are stored on our server. Wallet data stays in your browser storage.</li>
              <li>You can point the remote node to your own node endpoint.</li>
              <li>We will not be able to recover your wallet, seed, or keys for you.</li>
              <li>Use a strong password and verify node URLs before creating, importing, or sending funds.</li>
            </ul>
            <label className="checkbox">
              <input
                type="checkbox"
                checked={hideWelcomeModalNextTime}
                onChange={(e) => setHideWelcomeModalNextTime(e.target.checked)}
              />
              Do not show this message again on this browser
            </label>
            <div className="actions">
              <button onClick={onDismissWelcomeModal}>I Understand</button>
            </div>
          </section>
        </div>
      ) : null}
      <div className="title-row">
        <h1>Web Wallet</h1>
        <button
          className="mobile-menu-toggle"
          onClick={() => setMobileMenuOpen((prev) => !prev)}
          aria-label={mobileMenuOpen ? "Close menu" : "Open menu"}
          title={mobileMenuOpen ? "Close menu" : "Open menu"}
        >
          {mobileMenuOpen ? "Close" : "Menu"}
        </button>
      </div>
      <p>Browser wallet with onboarding, encrypted local vault, and configurable RPC nodes.</p>
      <div className="top-controls desktop-controls">
        <div className="actions">
          <button className={activeView === "wallet" ? "active" : ""} onClick={() => onSelectView("wallet")}>
            <span className="menu-icon icon-wallet" aria-hidden="true" />
            Wallet
          </button>
          <button className={activeView === "settings" ? "active" : ""} onClick={() => onSelectView("settings")}>
            <span className="menu-icon icon-settings" aria-hidden="true" />
            Settings
          </button>
          <button
            className="icon-button"
            onClick={() => setTheme(resolvedTheme === "dark" ? "light" : "dark")}
            title={resolvedTheme === "dark" ? "Switch to light theme" : "Switch to dark theme"}
            aria-label={resolvedTheme === "dark" ? "Switch to light theme" : "Switch to dark theme"}
          >
            <span className={`theme-glyph ${resolvedTheme === "dark" ? "moon" : "sun"}`} aria-hidden="true" />
          </button>
          {hasActiveWalletSession && !isLocked ? (
            <>
              <button onClick={onRefreshDirectStatus}>
                <span className="menu-icon icon-refresh" aria-hidden="true" />
                Refresh
              </button>
              <button onClick={onLogoutSession}>
                <span className="menu-icon icon-logout" aria-hidden="true" />
                Logout
              </button>
            </>
          ) : null}
        </div>
      </div>
      {mobileMenuOpen ? <div className="mobile-menu-overlay" onClick={() => setMobileMenuOpen(false)} /> : null}
      <aside className={`mobile-side-menu${mobileMenuOpen ? " open" : ""}`} aria-hidden={!mobileMenuOpen}>
        <div className="mobile-side-menu-inner">
          <h3>Menu</h3>
          <div className="actions">
            <button className={activeView === "wallet" ? "active" : ""} onClick={() => onSelectView("wallet")}>
              <span className="menu-icon icon-wallet" aria-hidden="true" />
              Wallet
            </button>
            <button className={activeView === "settings" ? "active" : ""} onClick={() => onSelectView("settings")}>
              <span className="menu-icon icon-settings" aria-hidden="true" />
              Settings
            </button>
            <button
              className="icon-button"
              onClick={() => setTheme(resolvedTheme === "dark" ? "light" : "dark")}
              title={resolvedTheme === "dark" ? "Switch to light theme" : "Switch to dark theme"}
              aria-label={resolvedTheme === "dark" ? "Switch to light theme" : "Switch to dark theme"}
            >
              <span className={`theme-glyph ${resolvedTheme === "dark" ? "moon" : "sun"}`} aria-hidden="true" />
            </button>
            {hasActiveWalletSession && !isLocked ? (
              <>
                <button
                  onClick={async () => {
                    await onRefreshDirectStatus();
                    setMobileMenuOpen(false);
                  }}
                >
                  <span className="menu-icon icon-refresh" aria-hidden="true" />
                  Refresh
                </button>
                <button
                  onClick={async () => {
                    await onLogoutSession();
                    setMobileMenuOpen(false);
                  }}
                >
                  <span className="menu-icon icon-logout" aria-hidden="true" />
                  Logout
                </button>
              </>
            ) : null}
          </div>
          {activeView === "wallet" && hasActiveWalletSession && !isLocked && !(pendingBackup && !pendingBackup.confirmed) ? (
            <>
              <h3>Wallet Tabs</h3>
              <div className="actions">
                <button className={walletTab === "overview" ? "active" : ""} onClick={() => onSelectWalletTab("overview")}>
                  <span className="menu-icon icon-overview" aria-hidden="true" />
                  Overview
                </button>
                <button
                  className={walletTab === "transactions" ? "active" : ""}
                  onClick={() => onSelectWalletTab("transactions")}
                >
                  <span className="menu-icon icon-transactions" aria-hidden="true" />
                  Transactions
                </button>
                <button className={walletTab === "transfer" ? "active" : ""} onClick={() => onSelectWalletTab("transfer")}>
                  <span className="menu-icon icon-transfer" aria-hidden="true" />
                  Transfer
                </button>
                <button className={walletTab === "receive" ? "active" : ""} onClick={() => onSelectWalletTab("receive")}>
                  <span className="menu-icon icon-receive" aria-hidden="true" />
                  Receive
                </button>
                <button className={walletTab === "nodes" ? "active" : ""} onClick={() => onSelectWalletTab("nodes")}>
                  <span className="menu-icon icon-nodes" aria-hidden="true" />
                  Nodes
                </button>
              </div>
            </>
          ) : null}
        </div>
      </aside>

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
                  <input
                    type="number"
                    min={0}
                    value={scanHeight}
                    disabled={disableScanHeightInput}
                    onChange={(e) => setScanHeight(e.target.value)}
                  />
                  <label className="checkbox">
                    <input type="checkbox" checked={scanFromCoinbase} onChange={(e) => setScanFromCoinbase(e.target.checked)} />
                    Scan from coinbase (force height 0)
                  </label>
                </>
              ) : (
                <p className="muted">
                  New wallet starts from current network height (height-only sync), not from block 0.
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
                    Start point: height {pendingImportReview.scanHeight}
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
              </div>
              {pendingBackup && !pendingBackup.confirmed ? (
                <section className="panel compact">
                  <h3>Backup Required</h3>
                  <p className="muted">Copy all secrets and confirm your mnemonic seed before wallet usage.</p>
                  <label className="field-label">Address</label>
                  <div className="address-box">
                    <code>{pendingBackup.address}</code>
                  </div>
                  <div className="actions">
                    <button
                      className={copiedFlash["backup-address"] ? "copied" : ""}
                      onClick={() => onCopyBackupSecret("address", pendingBackup.address, "Address")}
                    >
                      Copy Address
                    </button>
                  </div>
                  <label className="field-label">Mnemonic seed</label>
                  <textarea className="input-area" value={pendingBackup.mnemonicSeed} readOnly />
                  <div className="actions">
                    <button
                      className={copiedFlash["backup-seed"] ? "copied" : ""}
                      onClick={() => onCopyBackupSecret("seed", pendingBackup.mnemonicSeed, "Mnemonic seed")}
                    >
                      Copy Seed
                    </button>
                  </div>
                  <label className="field-label">Private spend key</label>
                  <input type="text" value={pendingBackup.privateSpendKey} readOnly />
                  <div className="actions">
                    <button
                      className={copiedFlash["backup-spend"] ? "copied" : ""}
                      onClick={() => onCopyBackupSecret("spend", pendingBackup.privateSpendKey, "Private spend key")}
                    >
                      Copy Spend Key
                    </button>
                  </div>
                  <label className="field-label">Private view key</label>
                  <input type="text" value={pendingBackup.privateViewKey} readOnly />
                  <div className="actions">
                    <button
                      className={copiedFlash["backup-view"] ? "copied" : ""}
                      onClick={() => onCopyBackupSecret("view", pendingBackup.privateViewKey, "Private view key")}
                    >
                      Copy View Key
                    </button>
                  </div>
                  <label className="field-label">Confirm seed (paste full mnemonic)</label>
                  <textarea
                    className="input-area"
                    value={backupSeedConfirmInput}
                    onChange={(e) => setBackupSeedConfirmInput(e.target.value)}
                    placeholder="Re-enter the same mnemonic seed"
                  />
                  <div className="actions">
                    <button onClick={onConfirmBackupGate}>I Confirm Backup and Seed</button>
                  </div>
                </section>
              ) : (
                <>
                  <div className="actions tab-row desktop-tab-row">
                    <button className={walletTab === "overview" ? "active" : ""} onClick={() => onSelectWalletTab("overview")}>
                      <span className="menu-icon icon-overview" aria-hidden="true" />
                      Overview
                    </button>
                    <button className={walletTab === "transactions" ? "active" : ""} onClick={() => onSelectWalletTab("transactions")}>
                      <span className="menu-icon icon-transactions" aria-hidden="true" />
                      Transactions
                    </button>
                    <button className={walletTab === "transfer" ? "active" : ""} onClick={() => onSelectWalletTab("transfer")}>
                      <span className="menu-icon icon-transfer" aria-hidden="true" />
                      Transfer
                    </button>
                    <button className={walletTab === "receive" ? "active" : ""} onClick={() => onSelectWalletTab("receive")}>
                      <span className="menu-icon icon-receive" aria-hidden="true" />
                      Receive
                    </button>
                    <button className={walletTab === "nodes" ? "active" : ""} onClick={() => onSelectWalletTab("nodes")}>
                      <span className="menu-icon icon-nodes" aria-hidden="true" />
                      Nodes
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
                        Profile: {directProfile.kind}, scan height {directProfile.scanHeight}
                      </p>
                    ) : null}
                    {directCursor ? <p>Cursor height: {directCursor.height}</p> : null}
                    {directSnapshotState ? (
                      <p>
                        Snapshot height: {directSnapshotState.snapshotHeight} | Cursor (snapshot wallet):{" "}
                        {directSnapshotState.cursorHeight} | Last snapshot: {formatAgeFromTs(directSnapshotState.updatedAt)}
                      </p>
                    ) : (
                      <p>Snapshot height: n/a</p>
                    )}
                    {directSyncStats ? (
                      <>
                        <div className="sync-health">
                          <p>
                            Target: {directSyncStats.targetHeight} | Remaining: {directSyncStats.remainingBlocks} | Last
                            update: {formatAgeFromTs(directSyncStats.updatedAt)}
                            {Date.now() - directSyncStats.updatedAt > 120000 ? " (possible stall)" : ""}
                          </p>
                          <p>
                            Speed: {syncHealth.avgBlocksPerSec > 0 ? `${syncHealth.avgBlocksPerSec.toFixed(2)} blk/s` : "warming up"}
                            {" | "}ETA: {formatEta(syncHealth.etaSeconds)}
                          </p>
                          <p>
                            Cursor movement: {syncHealth.lastDeltaBlocks} blocks in{" "}
                            {syncHealth.lastDeltaSeconds > 0 ? `${syncHealth.lastDeltaSeconds.toFixed(1)}s` : "n/a"} {" | "}
                            stagnant polls: {syncHealth.stagnantPolls}
                          </p>
                          <p>
                            Last batch: {directSyncStats.lastBatchStart ?? "-"} {"->"} {directSyncStats.lastBatchEnd ?? "-"} (
                            {directSyncStats.lastBatchSize}), mode {directSyncStats.fetchMode}
                          </p>
                        </div>
                      </>
                    ) : null}
                    {directSyncStats?.methodsTried?.length ? <p>Methods: {directSyncStats.methodsTried.join(" -> ")}</p> : null}
                    {directSyncStats?.lastError ? <p>Last error: {directSyncStats.lastError}</p> : null}
                  </details>
                </>
              ) : null}

              {walletTab === "transactions" ? (
                <section>
                  <h3>Transaction History</h3>
                  <div className="actions">
                    <label className="field-label tx-filter-label" htmlFor="tx-height-filter">
                      Min height filter
                    </label>
                    <input
                      id="tx-height-filter"
                      className="tx-filter-input"
                      type="number"
                      min={0}
                      value={txHeightFilterInput}
                      onChange={(e) => setTxHeightFilterInput(e.target.value)}
                      placeholder="0 = all heights"
                    />
                  </div>
                  {filteredTxHistory.length === 0 ? <p className="muted">No wallet-owned transactions scanned yet.</p> : null}
                  {filteredTxHistory.length > 0 ? (
                    <>
                      <div className="tx-table-wrap">
                        <table className="tx-table">
                          <thead>
                            <tr>
                              <th>Height</th>
                              <th>Time</th>
                              <th>Type</th>
                              <th>In</th>
                              <th>Out</th>
                              <th>Net</th>
                              <th>Payment ID</th>
                              <th>Tx Hash</th>
                            </tr>
                          </thead>
                          <tbody>
                            {txPageItems.map((entry) => (
                              <tr key={`${entry.txHash}-${entry.blockHeight}`}>
                                <td>#{entry.blockHeight}</td>
                                <td>{formatTxTime(entry.blockTimestamp)}</td>
                                <td>{formatTxType(entry.direction)}</td>
                                <td className="tx-num">
                                  {formatAtomicAmount(entry.incomingAtomic, COIN_DECIMALS)} {COIN_TICKER}
                                </td>
                                <td className="tx-num">
                                  {formatAtomicAmount(entry.outgoingAtomic, COIN_DECIMALS)} {COIN_TICKER}
                                </td>
                                <td className="tx-num">
                                  {formatAtomicAmount(entry.netAtomic, COIN_DECIMALS)} {COIN_TICKER}
                                </td>
                                <td>
                                  {entry.paymentId ? <code>{entry.paymentId}</code> : <span className="node-meta">-</span>}
                                </td>
                                <td>
                                  <code>{entry.txHash}</code>
                                </td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                      <div className="actions tx-pager">
                        <button disabled={txCurrentPage <= 1} onClick={() => setTxPage(1)}>
                          First
                        </button>
                        <button disabled={txCurrentPage <= 1} onClick={() => setTxPage((prev) => Math.max(1, prev - 1))}>
                          Prev
                        </button>
                        <span className="node-meta">
                          Page {txCurrentPage} / {txTotalPages}
                        </span>
                        <button
                          disabled={txCurrentPage >= txTotalPages}
                          onClick={() => setTxPage((prev) => Math.min(txTotalPages, prev + 1))}
                        >
                          Next
                        </button>
                        <button disabled={txCurrentPage >= txTotalPages} onClick={() => setTxPage(txTotalPages)}>
                          Last
                        </button>
                      </div>
                      <p className="muted">
                        Note: Incoming = funds received, Outgoing = funds sent, Self = outgoing transaction with wallet change
                        returned (shown as "Outgoing + Change").
                      </p>
                    </>
                  ) : null}
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
                  <input type="text" value={transferAmount} onChange={(e) => onTransferAmountChange(e.target.value)} />
                  <p className="muted">
                    Max available: {maxTransferAmount} {COIN_TICKER}. Amount accepts numbers only with up to {COIN_DECIMALS} decimals.
                  </p>
                  <label className="field-label">Payment ID (optional)</label>
                  <input type="text" value={transferPaymentId} onChange={(e) => setTransferPaymentId(e.target.value)} />
                  <p className="muted">
                    Transfer will run automatic preflight, then ask for confirmation before sending.
                  </p>
                  <div className="actions">
                    <button onClick={onSubmitTransfer}>Review & Submit Transfer</button>
                  </div>
                </section>
              ) : null}

              {walletTab === "receive" ? (
                <section>
                  <h3>Receive</h3>
                  <label className="field-label">Standard address</label>
                  <div className="address-box">
                    <code>{receiveAddress || "Wallet address is not available yet."}</code>
                  </div>
                  <div className="actions">
                    <button
                      className={copiedFlash["receive-standard"] ? "copied" : ""}
                      disabled={!receiveAddress}
                      onClick={onCopyStandardReceiveAddress}
                    >
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
                    <button onClick={onGenerateReceivePaymentIdShort}>Generate Short Payment ID</button>
                    <button onClick={onGenerateReceivePaymentIdLong}>Generate Long Payment ID</button>
                    <button onClick={onBuildIntegratedAddress}>Generate Integrated Address</button>
                    <button onClick={() => setReceiveIntegratedAddress("")}>Use Standard Address</button>
                  </div>
                  {receiveIntegratedAddress ? (
                    <>
                      <label className="field-label">Integrated address</label>
                      <textarea className="input-area" value={receiveIntegratedAddress} readOnly />
                      <div className="actions">
                        <button
                          className={copiedFlash["receive-integrated"] ? "copied" : ""}
                          onClick={onCopyIntegratedReceiveAddress}
                        >
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
                </>
              )}

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
            {hasActiveWalletSession ? (
              <section className="settings-block">
                <h3>Sync Controls</h3>
                <label className="field-label">Rescan wallet from height</label>
                <input
                  type="number"
                  min={0}
                  value={rescanHeightInput}
                  onChange={(e) => setRescanHeightInput(e.target.value)}
                />
                <p>
                  If height is above {RESCAN_HEIGHT_WARN_THRESHOLD}, confirmation will be required. Network height:{" "}
                  {directSummary?.daemonHeight ?? "unknown"}.
                </p>
                <div className="actions settings-actions">
                  <button onClick={onResetScanFromHeight}>Reset Scan Height</button>
                </div>
              </section>
            ) : null}
            <section className="settings-block">
              <h3>Node Diagnostics</h3>
              <div className="actions settings-actions">
                <button onClick={onProbeDefaultNode}>Probe Default Node</button>
                <button onClick={onRefreshNodeCapabilities}>Refresh Node Capabilities</button>
                <button onClick={onRefreshNodeScores}>Refresh Node Scores</button>
                <button onClick={onRefreshDirectStatus}>Refresh Direct RPC Status</button>
              </div>
            </section>
            <section className="settings-block">
              <h3>Session Control</h3>
              <div className="actions settings-actions">
                <button className="danger" onClick={onStopDirectSession}>
                  Stop Direct RPC Session
                </button>
              </div>
            </section>
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
            <section className="settings-block danger-zone">
              <h3>Danger Zone</h3>
              <div className="actions settings-actions">
                <button className="danger" onClick={onResetWallet}>
                  Reset Wallet Local Data
                </button>
              </div>
            </section>
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
