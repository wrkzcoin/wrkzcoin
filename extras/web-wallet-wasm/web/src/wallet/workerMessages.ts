import type { ImportFromKeysRequest, ImportFromSeedRequest, OpenWalletRequest } from "./types";

export type WorkerCommand =
  | { id: string; command: "apiVersion" }
  | { id: string; command: "version" }
  | { id: string; command: "open"; payload: OpenWalletRequest }
  | { id: string; command: "create"; payload: OpenWalletRequest }
  | { id: string; command: "restoreFromSeed"; payload: ImportFromSeedRequest }
  | { id: string; command: "restoreFromKeys"; payload: ImportFromKeysRequest }
  | { id: string; command: "backupSecrets"; payload: { walletId: number } }
  | { id: string; command: "close"; payload: { walletId: number } }
  | { id: string; command: "status"; payload: { walletId: number } }
  | { id: string; command: "balance"; payload: { walletId: number } }
  | { id: string; command: "swapNode"; payload: { walletId: number; daemonHost: string; daemonPort: number; daemonSsl: boolean } }
  | { id: string; command: "vaultInit"; payload: { password: string } }
  | { id: string; command: "vaultStatus" }
  | { id: string; command: "vaultPut"; payload: { key: string; value: string } }
  | { id: string; command: "vaultGet"; payload: { key: string } }
  | { id: string; command: "vaultDelete"; payload: { key: string } }
  | { id: string; command: "vaultLock" }
  | { id: string; command: "vaultReset" };

export interface WorkerReply {
  id: string;
  ok: boolean;
  result?: unknown;
  error?: string;
}
