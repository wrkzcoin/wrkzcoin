import { useMemo, useState } from "react";
import { WalletWorkerClient } from "./wallet/walletWorkerClient";

export function App(): JSX.Element {
  const client = useMemo(() => new WalletWorkerClient(), []);
  const [output, setOutput] = useState<string>("Wallet worker is ready.");
  const [vaultPassword, setVaultPassword] = useState<string>("");
  const [secretValue, setSecretValue] = useState<string>("");

  const onPing = async (): Promise<void> => {
    const res = await client.apiVersion();
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultInit = async (): Promise<void> => {
    const res = await client.vaultInit(vaultPassword);
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultStatus = async (): Promise<void> => {
    const res = await client.vaultStatus();
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultStoreDemo = async (): Promise<void> => {
    const res = await client.vaultPut("demo_seed", secretValue);
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultLoadDemo = async (): Promise<void> => {
    const res = await client.vaultGet("demo_seed");
    setOutput(JSON.stringify(res, null, 2));
  };

  const onVaultLock = async (): Promise<void> => {
    const res = await client.vaultLock();
    setOutput(JSON.stringify(res, null, 2));
  };

  return (
    <main className="container">
      <h1>Wrkz Web Wallet (WASM)</h1>
      <p>Scaffold with wallet worker bindings and multi-node config hooks.</p>
      <button onClick={onPing}>Check WASM API Version</button>
      <section className="panel">
        <h2>Encrypted Vault (Worker + IndexedDB)</h2>
        <input
          type="password"
          placeholder="Vault password"
          value={vaultPassword}
          onChange={(e) => setVaultPassword(e.target.value)}
        />
        <input
          type="text"
          placeholder="Demo seed or secret"
          value={secretValue}
          onChange={(e) => setSecretValue(e.target.value)}
        />
        <div className="actions">
          <button onClick={onVaultInit}>Init/Unlock Vault</button>
          <button onClick={onVaultStatus}>Vault Status</button>
          <button onClick={onVaultStoreDemo}>Store Demo Secret</button>
          <button onClick={onVaultLoadDemo}>Load Demo Secret</button>
          <button onClick={onVaultLock}>Lock Vault</button>
        </div>
      </section>
      <pre>{output}</pre>
    </main>
  );
}
