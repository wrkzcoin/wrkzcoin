import { HttpRpcClient } from "./rpcClient";
import type { NodeProbeResult, RpcNode } from "./types";

const REQUIRED_METHODS = ["getblockcount"];

export async function probeNode(node: RpcNode): Promise<NodeProbeResult> {
  const client = new HttpRpcClient({ timeoutMs: 8000, retries: 0 });
  const endpoint = `${node.ssl ? "https" : "http"}://${node.host}:${node.port}/json_rpc`;
  const started = Date.now();

  try {
    const result = await client.call<{ count?: number; status?: string }>(node, REQUIRED_METHODS[0]);
    const latencyMs = Date.now() - started;
    return {
      ok: true,
      nodeId: node.id,
      endpoint,
      supportsCors: true,
      supportsRequiredMethods: true,
      latencyMs,
      height: Number(result.count ?? 0)
    };
  } catch (error) {
    const reason = error instanceof Error ? error.message : String(error);
    return {
      ok: false,
      nodeId: node.id,
      endpoint,
      supportsCors: !reason.includes("Failed to fetch"),
      supportsRequiredMethods: false,
      reason
    };
  }
}

