export const COIN_NAME = "WrkzCoin";
export const COIN_TICKER = "WRKZ";
export const COIN_DECIMALS = 2;
export const COIN_ADDRESS_PREFIX = "Wrkz";

export function formatAtomicAmount(atomic: string | number | bigint, decimals = COIN_DECIMALS): string {
  const raw = String(atomic ?? "0").trim();
  if (!/^-?\d+$/.test(raw)) {
    return "0";
  }
  const negative = raw.startsWith("-");
  const digits = negative ? raw.slice(1) : raw;
  const padded = digits.padStart(decimals + 1, "0");
  const whole = padded.slice(0, padded.length - decimals) || "0";
  const frac = padded.slice(padded.length - decimals).padEnd(decimals, "0");
  const value = decimals > 0 ? `${whole}.${frac}` : whole;
  return negative ? `-${value}` : value;
}
