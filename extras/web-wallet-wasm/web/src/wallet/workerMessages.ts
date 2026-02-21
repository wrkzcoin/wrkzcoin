import type { ImportFromKeysRequest, ImportFromSeedRequest, OpenWalletRequest, ScanSyncDataBalanceRequest } from "./types";

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
  | { id: string; command: "deriveKeysFromSeed"; payload: { mnemonicSeed: string } }
  | { id: string; command: "generateSeedKeys" }
  | { id: string; command: "deriveAddressFromKeys"; payload: { privateSpendKey: string; privateViewKey: string } }
  | { id: string; command: "validateAddress"; payload: { address: string; allowIntegrated?: boolean } }
  | { id: string; command: "createIntegratedAddress"; payload: { address: string; paymentId: string } }
  | { id: string; command: "estimateBasicFee"; payload: { walletId: number; destination: string; amountAtomic: string; paymentId?: string } }
  | { id: string; command: "sendBasic"; payload: { walletId: number; destination: string; amountAtomic: string; paymentId?: string } }
  | { id: string; command: "swapNode"; payload: { walletId: number; daemonHost: string; daemonPort: number; daemonSsl: boolean } }
  | { id: string; command: "scanSyncDataBalance"; payload: ScanSyncDataBalanceRequest }
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
