export interface NodeEndpoint {
  id: string;
  host: string;
  port: number;
  ssl: boolean;
  priority: number;
}

export function normalizeNodeHost(input: string): string {
  let value = input.trim();
  value = value.replace(/^https?:\/\//i, "");
  value = value.split(/[/?#]/, 1)[0];

  if (value.startsWith("[")) {
    const end = value.indexOf("]");
    if (end > 1) {
      return value.slice(1, end);
    }
  }

  const colonCount = (value.match(/:/g) ?? []).length;
  if (colonCount === 1) {
    const [hostPart, portPart] = value.split(":");
    if (portPart && /^\d+$/.test(portPart)) {
      return hostPart;
    }
  }

  return value;
}

export function buildNodeRpcUrl(node: Pick<NodeEndpoint, "host" | "port" | "ssl">): string {
  return `${node.ssl ? "https" : "http"}://${node.host}:${node.port}`;
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
