import type { NodeEndpoint } from "../wallet/types";

export const DEFAULT_RPC_PORT = 17856;
export const DEFAULT_RPC_SSL = false;

export const DEFAULT_NODES: NodeEndpoint[] = [
  { id: "primary", host: "wrkz.bot.tips", port: DEFAULT_RPC_PORT, ssl: DEFAULT_RPC_SSL, priority: 1 },
  { id: "secondary", host: "node2.example.com", port: DEFAULT_RPC_PORT, ssl: DEFAULT_RPC_SSL, priority: 2 },
  { id: "tertiary", host: "node3.example.com", port: DEFAULT_RPC_PORT, ssl: DEFAULT_RPC_SSL, priority: 3 }
];
