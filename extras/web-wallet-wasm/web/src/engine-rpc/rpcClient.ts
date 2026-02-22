import type { RpcCallResult, RpcClientOptions, RpcNode } from "./types";

const DEFAULT_TIMEOUT_MS = 15000;
const DEFAULT_RETRIES = 1;

function buildNodeHttpEndpoint(node: RpcNode): string {
  const protocol = node.ssl ? "https" : "http";
  const defaultPort = node.ssl ? 443 : 80;
  const portSuffix = node.port === defaultPort ? "" : `:${node.port}`;
  return `${protocol}://${node.host}${portSuffix}/json_rpc`;
}

function buildNodeBaseEndpoint(node: RpcNode): string {
  const protocol = node.ssl ? "https" : "http";
  const defaultPort = node.ssl ? 443 : 80;
  const portSuffix = node.port === defaultPort ? "" : `:${node.port}`;
  return `${protocol}://${node.host}${portSuffix}`;
}

async function withTimeout<T>(promise: Promise<T>, timeoutMs: number): Promise<T> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  const timeoutPromise = new Promise<never>((_, reject) => {
    timer = setTimeout(() => reject(new Error(`rpc_timeout_${timeoutMs}ms`)), timeoutMs);
  });
  try {
    return await Promise.race([promise, timeoutPromise]);
  } finally {
    if (timer) {
      clearTimeout(timer);
    }
  }
}

export class HttpRpcClient {
  private readonly timeoutMs: number;
  private readonly retries: number;

  public constructor(options: RpcClientOptions = {}) {
    this.timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;
    this.retries = options.retries ?? DEFAULT_RETRIES;
  }

  public async call<T = unknown>(node: RpcNode, method: string, params?: Record<string, unknown>): Promise<T> {
    const endpoint = buildNodeHttpEndpoint(node);
    const body = {
      jsonrpc: "2.0",
      id: "0",
      method,
      ...(params ? { params } : {})
    };

    let lastError: Error | null = null;
    for (let attempt = 0; attempt <= this.retries; attempt += 1) {
      try {
        const response = await withTimeout(
          fetch(endpoint, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body)
          }),
          this.timeoutMs
        );
        if (!response.ok) {
          throw new Error(`rpc_http_${response.status}`);
        }

        const json = (await response.json()) as RpcCallResult<T>;
        if (json.error) {
          throw new Error(`rpc_error_${json.error.code}_${json.error.message}`);
        }
        return (json.result ?? ({} as T)) as T;
      } catch (error) {
        lastError = error instanceof Error ? error : new Error(String(error));
      }
    }

    throw lastError ?? new Error("rpc_call_failed");
  }

  public async postPath<T = unknown>(node: RpcNode, path: string, body: Record<string, unknown>): Promise<T> {
    const endpoint = `${buildNodeBaseEndpoint(node)}${path.startsWith("/") ? path : `/${path}`}`;
    let lastError: Error | null = null;

    for (let attempt = 0; attempt <= this.retries; attempt += 1) {
      try {
        const response = await withTimeout(
          fetch(endpoint, {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body)
          }),
          this.timeoutMs
        );
        if (!response.ok) {
          throw new Error(`rpc_http_${response.status}`);
        }
        return (await response.json()) as T;
      } catch (error) {
        lastError = error instanceof Error ? error : new Error(String(error));
      }
    }

    throw lastError ?? new Error("rpc_call_failed");
  }
}
