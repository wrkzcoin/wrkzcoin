import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
import { useMemo, useState } from "react";
import { WalletWorkerClient } from "./wallet/walletWorkerClient";
export function App() {
    const client = useMemo(() => new WalletWorkerClient(), []);
    const [output, setOutput] = useState("Wallet worker is ready.");
    const [vaultPassword, setVaultPassword] = useState("");
    const [secretValue, setSecretValue] = useState("");
    const onPing = async () => {
        const res = await client.apiVersion();
        setOutput(JSON.stringify(res, null, 2));
    };
    const onVaultInit = async () => {
        const res = await client.vaultInit(vaultPassword);
        setOutput(JSON.stringify(res, null, 2));
    };
    const onVaultStatus = async () => {
        const res = await client.vaultStatus();
        setOutput(JSON.stringify(res, null, 2));
    };
    const onVaultStoreDemo = async () => {
        const res = await client.vaultPut("demo_seed", secretValue);
        setOutput(JSON.stringify(res, null, 2));
    };
    const onVaultLoadDemo = async () => {
        const res = await client.vaultGet("demo_seed");
        setOutput(JSON.stringify(res, null, 2));
    };
    const onVaultLock = async () => {
        const res = await client.vaultLock();
        setOutput(JSON.stringify(res, null, 2));
    };
    return (_jsxs("main", { className: "container", children: [_jsx("h1", { children: "Wrkz Web Wallet (WASM)" }), _jsx("p", { children: "Scaffold with wallet worker bindings and multi-node config hooks." }), _jsx("button", { onClick: onPing, children: "Check WASM API Version" }), _jsxs("section", { className: "panel", children: [_jsx("h2", { children: "Encrypted Vault (Worker + IndexedDB)" }), _jsx("input", { type: "password", placeholder: "Vault password", value: vaultPassword, onChange: (e) => setVaultPassword(e.target.value) }), _jsx("input", { type: "text", placeholder: "Demo seed or secret", value: secretValue, onChange: (e) => setSecretValue(e.target.value) }), _jsxs("div", { className: "actions", children: [_jsx("button", { onClick: onVaultInit, children: "Init/Unlock Vault" }), _jsx("button", { onClick: onVaultStatus, children: "Vault Status" }), _jsx("button", { onClick: onVaultStoreDemo, children: "Store Demo Secret" }), _jsx("button", { onClick: onVaultLoadDemo, children: "Load Demo Secret" }), _jsx("button", { onClick: onVaultLock, children: "Lock Vault" })] })] }), _jsx("pre", { children: output })] }));
}
