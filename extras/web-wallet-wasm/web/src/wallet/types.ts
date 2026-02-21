export interface NodeEndpoint {
  id: string;
  url: string;
  priority: number;
}

export interface WasmResponse<T = unknown> {
  ok: boolean;
  code?: number;
  error?: string;
  data?: T;
}

export interface OpenWalletRequest {
  filename: string;
  password: string;
  daemonHost: string;
  daemonPort: number;
  daemonSsl: boolean;
  syncThreads?: number;
}
