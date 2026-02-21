import type { NodeEndpoint } from "../wallet/types";

export const DEFAULT_RPC_PORT = 17856;
export const DEFAULT_RPC_SSL = false;

export const DEFAULT_NODES: NodeEndpoint[] = [
  { id: "primary", host: "node-fin.wrkz.work", port: 443, ssl: true, priority: 1 }
];
