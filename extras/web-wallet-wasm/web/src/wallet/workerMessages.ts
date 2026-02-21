export type WorkerCommand =
  | { id: string; command: "apiVersion" }
  | { id: string; command: "version" }
  | { id: string; command: "open"; payload: Record<string, unknown> }
  | { id: string; command: "status"; payload: { walletId: number } }
  | { id: string; command: "balance"; payload: { walletId: number } }
  | { id: string; command: "swapNode"; payload: { walletId: number; daemonHost: string; daemonPort: number; daemonSsl: boolean } }
  | { id: string; command: "vaultInit"; payload: { password: string } }
  | { id: string; command: "vaultStatus" }
  | { id: string; command: "vaultPut"; payload: { key: string; value: string } }
  | { id: string; command: "vaultGet"; payload: { key: string } }
  | { id: string; command: "vaultDelete"; payload: { key: string } }
  | { id: string; command: "vaultLock" };

export interface WorkerReply {
  id: string;
  ok: boolean;
  result?: unknown;
  error?: string;
}
