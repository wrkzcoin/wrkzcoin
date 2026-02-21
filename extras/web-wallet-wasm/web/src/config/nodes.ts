import type { NodeEndpoint } from "../wallet/types";

export const DEFAULT_NODES: NodeEndpoint[] = [
  { id: "primary", url: "https://wrkz.bot.tips:443", priority: 1 },
  { id: "secondary", url: "http://wrkz.bot.tips:443", priority: 2 },
  { id: "tertiary", url: "https://node3.example.com", priority: 3 }
];
