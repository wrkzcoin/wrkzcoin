export interface WalletWasmModule {
  ccall: (ident: string, returnType: string | null, argTypes: string[], args: unknown[]) => unknown;
}

export default function createWalletModule(): Promise<WalletWasmModule>;
