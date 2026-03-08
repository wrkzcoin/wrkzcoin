# WrkzCoin Masternode — Feature Draft

> **Status:** DRAFT — implementation complete, pending mainnet fork activation.
> Fork activation heights are configured in `src/config/CryptoNoteConfig.h`.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture Summary](#2-architecture-summary)
3. [Fork Heights & Rollout Schedule](#3-fork-heights--rollout-schedule)
4. [Consensus Parameters](#4-consensus-parameters)
5. [Transaction Types](#5-transaction-types)
6. [Masternode Lifecycle](#6-masternode-lifecycle)
7. [Collateral & Bond](#7-collateral--bond)
8. [Endpoint Commitment (One-IP Binding)](#8-endpoint-commitment-one-ip-binding)
9. [Health & Heartbeat System](#9-health--heartbeat-system)
10. [External Attestation](#10-external-attestation)
11. [Reward Distribution](#11-reward-distribution)
12. [State Persistence](#12-state-persistence)
13. [Operator Manual — Registration Workflow](#13-operator-manual--registration-workflow)
14. [Operator Manual — Running & Maintaining a Masternode](#14-operator-manual--running--maintaining-a-masternode)
15. [RPC API Reference](#15-rpc-api-reference)
16. [Daemon Console Commands](#16-daemon-console-commands)
17. [IPv6 Support](#17-ipv6-support)
18. [Security Model & Known Limitations](#18-security-model--known-limitations)
19. [Configuration Reference](#19-configuration-reference)
20. [Frequently Asked Questions](#20-frequently-asked-questions)

---

## 1. Overview

WrkzCoin masternodes are full-network nodes that lock a collateral bond, commit to a publicly reachable IP endpoint, and submit regular heartbeat transactions to prove liveness. In exchange they receive a deterministic share of the block reward once the reward fork activates.

The system is designed with the following goals:

- **Economic commitment** — Collateral is locked in-consensus; spending it requires waiting through a spend-lock period after deactivation.
- **Liveness accountability** — Health is tracked via on-chain heartbeat transactions over a rolling 7-day window. Nodes falling below the health threshold are excluded from rewards.
- **Verifier attestation** — External verifier nodes independently attest to masternode liveness on-chain, providing a second check beyond self-reported heartbeats.
- **Deterministic fairness** — The reward winner is selected deterministically from eligible candidates by a fairness algorithm that favours nodes that have received the least reward in the recent window.
- **IPv4 and IPv6 support** — Endpoint commitments accept both `IPv4:port` and `[IPv6]:port` notation.

---

## 2. Architecture Summary

```
┌─────────────────────────────────────────────────────┐
│                     Daemon (Core)                   │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │           MasternodeStateTracker             │   │
│  │  • Lifecycle state machine (per node)        │   │
│  │  • Health window (rolling 7-day samples)     │   │
│  │  • Attestation window (external verifiers)   │   │
│  │  • Fairness reward accounting                │   │
│  │  • Collateral spend-lock enforcement         │   │
│  └──────────────────────────────────────────────┘   │
│                         ▲                           │
│          Block validation / applyBlock              │
│                         │                           │
│  ┌──────────────────────────────────────────────┐   │
│  │          MasternodeTx parser                 │   │
│  │  Reads MN01-prefixed extra fields from txs   │   │
│  └──────────────────────────────────────────────┘   │
│                                                     │
│  Persistence: DB/masternode_state.json (JSON blob)  │
│  Rebuild: replay all blocks from chain on startup   │
└─────────────────────────────────────────────────────┘
         │ RPC             │ Console          │ P2P
         ▼                 ▼                  ▼
  /masternodes        masternodes cmd    Normal tx pool
  /masternodes/count  mn_registration   (no dedicated
  /getinfo            _string cmd        gossip layer)
  /getblocktemplate
```

---

## 3. Fork Heights & Rollout Schedule

The masternode feature activates in two stages to give operators time to register before rewards begin.

| Fork | Config Constant | Height | Approximate Date |
|------|----------------|--------|-----------------|
| Feature fork | `MASTERNODE_FEATURE_FORK_HEIGHT` | 4,600,000 | TBD |
| Reward fork | `MASTERNODE_REWARD_FORK_HEIGHT` | Feature + 43,200 blocks (~30 days) | TBD |

**Stage 1 — Feature fork (height 4,600,000):**
- Masternode Register, Activate, Deactivate, Penalize, Revoke, Heartbeat, and Attest transactions are accepted into blocks.
- State tracking begins. Health samples accumulate.
- No reward split yet — 100% of block reward goes to PoW miner.

**Stage 2 — Reward fork (~30 days after Feature fork):**
- 70% of block reward distributed to the selected masternode winner.
- 30% of block reward goes to PoW miner.
- Transaction fees always go 100% to the PoW miner regardless of fork stage.

> Setting either constant to `0` disables the feature entirely (used on testnet during development).

---

## 4. Consensus Parameters

All values are defined in `src/config/CryptoNoteConfig.h` under `CryptoNote::parameters`.

| Parameter | Value | Description |
|-----------|-------|-------------|
| `MASTERNODE_HEALTH_WINDOW_BLOCKS` | 10,080 (7 days) | Rolling window for health percentage calculation |
| `MASTERNODE_MIN_HEALTH_PERCENT` | 95% | Minimum heartbeat health to be reward-eligible |
| `MASTERNODE_FAIRNESS_WINDOW_BLOCKS` | 10,080 (7 days) | Window used for fairness reward accounting |
| `MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS` | 30,240 (21 days) | Collateral spend-lock after deactivation/revocation |
| `MASTERNODE_REWARD_PERCENT` | 70% | Masternode share of distributable block reward |
| `MASTERNODE_REGISTRATION_BOND_AMOUNT` | 200,000,000,000 atomic (2,000,000,000 WRKZ) | Minimum collateral output required at registration |
| `MASTERNODE_COLLATERAL_LOCK_AMOUNT` | 200,000,000,000 atomic (2,000,000,000 WRKZ) | Same as bond amount (equal in current config) |
| `MASTERNODE_REGISTRATION_TOKEN_TTL_BLOCKS` | 1,440 (1 day) | Registration token validity window |
| `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` | 5 blocks | Minimum spacing between accepted heartbeat txs |
| `MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION` | true | Whether attestation threshold must be met for rewards |
| `MASTERNODE_ATTESTATION_WINDOW_BLOCKS` | 10,080 (7 days) | Rolling window for attestation health |
| `MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW` | 24 | Minimum attestation samples needed in window |
| `MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT` | 80% | Minimum attestation health percentage |
| `MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER` | 60 blocks | Minimum spacing per verifier between accepted attestations |
| `MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST` | false | Whether verifier key allowlist is enforced |

---

## 5. Transaction Types

Masternode operations are embedded in standard CryptoNote transactions via the `extra` field. They are identified by the 4-byte magic prefix `MN01` (bytes: `0x4D 0x4E 0x30 0x31`).

| Type byte | Name | Who sends | Purpose |
|-----------|------|-----------|---------|
| `0x01` | **Register** | Operator wallet | Lock collateral, commit endpoint, begin lifecycle |
| `0x02` | **Activate** | Governance / operator | Transition from Registered → Active |
| `0x03` | **Deactivate** | Governance / operator | Transition Active → Inactive; start spend-lock |
| `0x04` | **Penalize** | Governance | Transition Active/Inactive → Penalized |
| `0x05` | **Revoke** | Governance / operator | Permanently revoke; start spend-lock |
| `0x06` | **Heartbeat** | Operator daemon | Report liveness once per block |
| `0x07` | **Attest** | Verifier node | Independently attest masternode liveness |

### Register payload fields

| Field | Type | Notes |
|-------|------|-------|
| Magic `MN01` | 4 bytes | Identifies MN tx |
| Type `0x01` | 1 byte | Register |
| Masternode ID | 32 bytes (Hash) | Randomly chosen by operator |
| Payout key | 32 bytes (PublicKey) | Receives MN reward outputs |
| Registration token ID | 32 bytes (Hash) | From daemon `mn_registration_string` command |
| Token expiry height | 4 bytes (LE uint32) | Must be ≥ current block height |
| Collateral amount | 8 bytes (LE uint64) | Must be ≥ `MASTERNODE_COLLATERAL_LOCK_AMOUNT` |
| Collateral global output index | 4 bytes (LE uint32) | Index in global output set |
| Collateral key image | 32 bytes (KeyImage) | Prevents double-use |
| Collateral output key | 32 bytes (PublicKey) | Used to verify collateral signature |
| Endpoint commitment | 32 bytes (Hash) | `cn_fast_hash("MNIP1|<canonical_addr>")` |
| Payout key signature | 64 bytes (Signature) | Signs unsigned payload with payout key |
| Collateral signature | 64 bytes (Signature) | Signs unsigned payload with collateral output key |

---

## 6. Masternode Lifecycle

```
                  Register tx
                      │
                      ▼
               ┌─────────────┐
               │  Registered  │◄──── Re-register (if Revoked)
               └──────┬──────┘
                      │ Activate tx
                      ▼
               ┌─────────────┐
         ┌────►│   Active     │◄────────────┐
         │     └──────┬──────┘             │
         │            │ Heartbeat txs       │ Reactivate?
         │            │ Attest txs          │ (not implemented)
         │            │                    │
         │     Deactivate tx          (manual)
         │            │
         │            ▼
         │     ┌─────────────┐
         │     │  Inactive   │
         │     └──────┬──────┘
         │            │ Penalize tx
         │            ▼
         │     ┌─────────────┐
         │     │  Penalized  │
         │     └──────┬──────┘
         │            │ Revoke tx
         │            ▼
         │     ┌─────────────┐
         └─────│   Revoked   │──► Re-register allowed
               └─────────────┘
```

**Spend-lock:** After Deactivate or Revoke, the collateral key image is spend-locked for `MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS` (21 days). Transactions that attempt to spend it during this period are rejected by consensus.

**Reward eligibility:** Only nodes in **Active** status that meet both the heartbeat health threshold and attestation threshold are considered for rewards.

---

## 7. Collateral & Bond

- The operator must hold an unspent output of at least **2,000,000,000 WRKZ** (200,000,000,000 atomic units) in their wallet.
- This output's key image is committed on-chain in the Register transaction.
- The consensus layer checks the key image against all previously registered masternodes to prevent double-registration.
- The output is **not frozen at the network level** — but attempting to spend it while the masternode is active or within the spend-lock window will result in a transaction rejected from blocks.
- After the spend-lock window expires (21 days post-deactivation), the collateral output can be freely spent.

> Recommendation: Keep collateral in a **dedicated wallet** used exclusively for masternode registration. This reduces the risk of accidentally spending it.

---

## 8. Endpoint Commitment (One-IP Binding)

Each masternode commits to exactly one public IP endpoint at registration time. The commitment is a hash, so the actual IP address is never stored on-chain — only the 32-byte `cn_fast_hash` of the preimage.

### Commitment Scheme

```
preimage  = "MNIP1|" + canonical_endpoint
commitment = cn_fast_hash(preimage)
```

### Canonical Endpoint Formats

| Address type | Input format | Canonical form | Example preimage |
|---|---|---|---|
| IPv4 | `1.2.3.4:17855` | `1.2.3.4:17855` | `MNIP1\|1.2.3.4:17855` |
| IPv6 | `[2001:db8::1]:17855` | `[2001:db8::1]:17855` | `MNIP1\|[2001:db8::1]:17855` |

Rules for canonicalization:
- IPv4: octets are normalized (no leading zeros), port is decimal.
- IPv6: address is lowercased; bracket notation `[addr]:port` is always used. The address is not expanded to full form (compressed notation as entered is preserved).
- Port `0` and ports above `65535` are rejected.

The commitment uniqueness is enforced on-chain — no two active or pending masternodes may share the same endpoint commitment. This prevents a single physical server from registering as multiple masternodes.

---

## 9. Health & Heartbeat System

### Heartbeat Transactions

A masternode operator sends a **Heartbeat** tx (type `0x06`) at most once per `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` blocks. The tx includes:
- Masternode ID (32 bytes)
- `healthy` flag (1 byte: `0x01` = healthy, `0x00` = unhealthy)
- Signature over the unsigned payload using the payout key

### Health Calculation

The daemon tracks up to `MASTERNODE_HEALTH_WINDOW_BLOCKS` (10,080) heartbeat samples in a rolling deque. At any point:

```
health_percent = (healthy_samples_in_window / total_samples_in_window) * 100
```

A node must maintain ≥ **95%** health to be reward-eligible.

### Rate Limiting

- Minimum spacing: `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` (5 blocks) between accepted heartbeats.
- Heartbeats received too quickly are rejected with `EXTRA_TOO_LARGE` validation error.

### Block Space Impact

Each heartbeat is a full CryptoNote transaction. With a fee-bearing input/output structure the tx is roughly **500–1,000 bytes**; a zero-input transaction-PoW heartbeat is roughly **110 bytes** but requires computational work from the operator.

With `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL = 5`, each masternode submits at most 1 heartbeat per 5 blocks. The health window (10,080 blocks) provides 2,016 opportunities per masternode; meeting the 95% threshold requires 1,916 healthy samples, allowing up to 100 missed heartbeats (~500 blocks ≈ 8 hours) per window. The block granted full reward zone is **100,000 bytes**.

**Fee-bearing heartbeat txs (~500 bytes each):**

| Active masternodes | Heartbeat txs/block | Block space consumed |
|---|---|---|
| 10 | 2 | ~1 KB (1%) |
| 50 | 10 | ~5 KB (5%) |
| 100 | 20 | ~10 KB (10%) |
| 200 | 40 | ~20 KB (20%) |
| 500 | 100 | ~50 KB (50%) |
| 1,000 | 200 | ~100 KB (100% — user txs squeezed out) |

---

## 10. External Attestation

In addition to self-reported heartbeats, external **verifier nodes** submit Attest transactions that independently rate the masternode's liveness.

### Attestation Transaction

- Masternode ID (32 bytes)
- Verifier public key (32 bytes)
- `healthy` flag (1 byte)
- Signature by the verifier key

### Attestation Health Calculation

Attestation samples are tracked separately from heartbeats in a `MASTERNODE_ATTESTATION_WINDOW_BLOCKS` (10,080) rolling window.

```
attest_health_percent = (healthy_attest_samples_in_window / total_attest_samples_in_window) * 100
```

To be eligible for rewards, a masternode must have:
- At least `MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW` (24) attestation samples in the window, AND
- `attest_health_percent` ≥ `MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT` (80%)

### Rate Limiting

- Each individual verifier is limited to one accepted attestation per `MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER` (60) blocks.
- This prevents a single verifier from flooding the state.

### Verifier Allowlist

`MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST` is currently `false` — any verifier key is accepted. When set to `true`, only keys in `MASTERNODE_VERIFIER_PUBKEY_ALLOWLIST` will be accepted; an empty allowlist rejects all attestations.

---

## 11. Reward Distribution

### Two-Fork Design

```
Before Feature fork:   100% → PoW miner (masternodes not active)
After Feature fork:    100% → PoW miner (masternodes register, no rewards yet)
After Reward fork:      70% → Masternode winner
                        30% → PoW miner
Transaction fees:      100% → PoW miner always
```

### Winner Selection

At each block after the Reward fork:

1. All masternodes in **Active** status are collected as candidates.
2. Candidates not meeting the heartbeat health threshold (95%) are excluded.
3. Candidates not meeting the attestation threshold (80%, 24 samples) are excluded.
4. Among remaining eligible candidates, the one with the **lowest total reward received in the last 7-day window** is selected as the winner.
5. Ties are broken deterministically by masternode ID hash comparison.

If no eligible candidate exists, the full reward goes to the PoW miner.

### Reward Output

The selected masternode winner receives a dedicated output in the coinbase transaction sent to their registered **payout key** address. The payout key is separate from the collateral key, so rewards go to a different address than the collateral.

### Masternode Set Hash

At each block height, the daemon computes a deterministic hash over the set of all active masternode IDs ordered by ID. This `masternode_set_hash` is returned in `/getblocktemplate` and `/getinfo`. Miners can supply `expected_masternode_set_hash` to detect if their view of the masternode set differs from the node's.

---

## 12. State Persistence

Masternode state is kept in memory as a `MasternodeStateTracker` object in `Core`. It is persisted in two ways:

### JSON Snapshot

Path: `<datadir>/DB/masternode_state.json`

- Written on: successful block merge (new main chain tip), blockchain rewind.
- Read on: daemon startup (before sync resumes).
- Fallback: if the snapshot is absent, invalid, or from an older chain height, the daemon replays all blocks from genesis to rebuild state. This is safe but slow on long chains.

### DB Blob

The state is also serializable via the key prefix `m/state` in `DBUtils.h`, used for cross-validation during DB-backed cache operations.

---

## 13. Operator Manual — Registration Workflow

### Prerequisites

- A synced `wrkzd` daemon
- A `zedwallet++` wallet with at least **2,000,000,000 WRKZ** in a single unspent output plus enough for transaction fees
- A publicly reachable server with a static IP address (IPv4 or IPv6) and an open P2P port

### Step 1 — Generate a Registration Token (Daemon)

On the daemon console, run:

```
mn_registration_string
```

Or to use a specific masternode ID:

```
mn_registration_string <mn_id_hex>
```

Output:
```
Masternode registration token:
MNREG2:<mn_id_hex>:<token_id_hex>:<expires_at_height>

Token expires at height: <height>
Required bond amount: 2000000000.00 WRKZ
```

The token encodes:
- A 32-byte masternode ID (random or operator-supplied)
- A 32-byte registration token ID (random)
- An expiry block height (current height + 1,440 blocks ≈ 24 hours)

> The token expires after approximately 24 hours. Complete registration before then.

### Step 2 — Register (Wallet)

In `zedwallet++`:

```
mn_register <token> <endpoint>
```

**IPv4 example:**
```
mn_register MNREG2:abc...def:123...456:4601440 203.0.113.10:17855
```

**IPv6 example:**
```
mn_register MNREG2:abc...def:123...456:4601440 [2001:db8::1]:17855
```

Or run `mn_register` interactively — you will be prompted for the token and endpoint.

The wallet will:
1. Parse and validate the token
2. Canonicalize the endpoint and compute the commitment hash
3. Select a qualifying collateral output (≥ 2,000,000,000 WRKZ)
4. Build the Register transaction with dual signatures (payout key + collateral key)
5. Display a confirmation summary and prompt for approval
6. Broadcast the transaction

### Step 3 — Activate (pending implementation)

After the Register tx is confirmed, an **Activate** transaction must be submitted to move the masternode from `Registered` → `Active` status. This step is currently a governance operation; a wallet command (`mn_activate`) is planned.

### Step 4 — Maintain

Once active, the operator daemon must submit **Heartbeat** transactions regularly (at least every `1/0.95` ≈ every block to maintain 95% health). A wallet command (`mn_heartbeat`) for this is planned.

---

## 14. Operator Manual — Running & Maintaining a Masternode

### Recommended Server Configuration

| Resource | Minimum | Recommended |
|----------|---------|-------------|
| CPU | 2 cores | 4 cores |
| RAM | 2 GB | 4 GB |
| Disk | 20 GB SSD | 40 GB SSD |
| Network | 10 Mbit/s | 100 Mbit/s |
| IP | Static IPv4 or IPv6 | Static dual-stack |

### Network Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 17855 | TCP | P2P (mainnet default) |
| 17856 | TCP | RPC (default) |

For IPv6 P2P, use:
```
wrkzd --p2p-bind-ipv6-address :: --p2p-bind-port-ipv6 17855
```

### Health Maintenance

- Keep your masternode online continuously.
- Ensure heartbeat transactions are submitted every block.
- Monitor `health_percent` via `masternodes` console command or `GET /masternodes` RPC.
- A node falling below 95% health is excluded from reward selection until health recovers.
- A node deactivated after poor health incurs a 21-day spend-lock on its collateral.

### Monitoring

Check masternode status via daemon console:
```
masternodes          # list first 20 masternodes
masternodes 5 0      # list first 5 masternodes
```

Or via RPC:
```
GET /masternodes?limit=20&offset=0
GET /masternodes/count
```

---

## 15. RPC API Reference

All endpoints are available on the standard RPC port (default 17856). IPv6 RPC is supported via `--rpc-bind-ipv6-address` and `--rpc-use-ipv6`.

### `GET /masternodes/count`

Returns the total number of tracked masternodes.

**Response:**
```json
{
  "count": 42,
  "status": "OK"
}
```

### `GET /masternodes?limit=N&offset=M`

Returns a paginated list of masternode snapshots.

**Query params:**
- `limit` (default 20, max unbounded)
- `offset` (default 0)

**Response:**
```json
{
  "total": 42,
  "masternodes": [
    {
      "mn_id": "abcdef...",
      "state": "Active",
      "bonded": true,
      "bond_amount": 200000000000,
      "collateral_amount": 200000000000,
      "collateral_global_output_index": 12345,
      "has_endpoint_commitment": true,
      "endpoint_commitment": "deadbeef...",
      "health_percent": 98,
      "spend_locked": false,
      "last_paid_height": 4601234,
      "reward_in_fairness_window": 700000000
    }
  ],
  "status": "OK"
}
```

### `GET /getinfo` — Masternode fields

```json
{
  "masternode_feature_fork_active": true,
  "masternode_reward_fork_active": false,
  "masternode_eligible_count": 12,
  "masternode_set_hash": "aabbcc...",
  "masternode_reward_winner": "mnid_hex_or_null"
}
```

### `POST /json_rpc` — `getblocktemplate` — Masternode fields

Supply `expected_masternode_set_hash` to detect set divergence:

```json
{
  "jsonrpc": "2.0",
  "method": "getblocktemplate",
  "params": {
    "wallet_address": "Wrkz...",
    "reserve_size": 8,
    "expected_masternode_set_hash": "aabbcc..."
  }
}
```

Response includes:
```json
{
  "masternode_eligible_count": 12,
  "masternode_set_hash": "aabbcc...",
  "masternode_reward_winner": "mnid_hex_or_null"
}
```

If `expected_masternode_set_hash` is provided and does not match the node's computed hash, the call returns error code `-6` with a mismatch message.

---

## 16. Daemon Console Commands

### `masternodes [limit] [offset]`

Display a paginated table of masternodes.

```
masternodes           # shows first 20
masternodes 5         # shows first 5
masternodes 10 20     # shows 10 starting at offset 20
```

### `mn_registration_string [mn_id_hex]`

Generate a registration token for use with `mn_register` in zedwallet++.

```
mn_registration_string
mn_registration_string 1234abcd...
```

Output includes the `MNREG2:...` token string, expiry height, and required bond amount.

---

## 17. IPv6 Support

WrkzCoin masternodes fully support IPv6 for both network connectivity and endpoint commitments.

### P2P (Network layer)

The daemon supports IPv6 peer connections natively. To enable IPv6 P2P listening:

```bash
wrkzd \
  --p2p-bind-ipv6-address :: \
  --p2p-bind-port-ipv6 17855
```

### RPC

To enable IPv6 RPC alongside the standard IPv4 listener:

```bash
wrkzd \
  --rpc-bind-ipv6-address :: \
  --rpc-use-ipv6
```

IPv6 RPC bind failure is non-fatal — a warning is printed and IPv4 RPC continues normally.

### Endpoint Commitment

The masternode endpoint commitment supports both IPv4 and IPv6 addresses. Use bracket notation for IPv6:

```
# IPv4
mn_register <token> 203.0.113.10:17855

# IPv6
mn_register <token> [2001:db8::1]:17855
```

The canonical preimage is:
- IPv4: `MNIP1|1.2.3.4:17855`
- IPv6: `MNIP1|[2001:db8::1]:17855` (address is lowercased)

Only the `cn_fast_hash` of the preimage is stored on-chain. The uniqueness check prevents two masternodes from sharing an endpoint, regardless of address family.

---

## 18. Security Model & Known Limitations

### What the consensus layer guarantees

- **Collateral double-use is prevented** — same key image or global output index cannot be used by two active masternodes simultaneously.
- **Token replay is prevented** — registration token IDs are tracked; same token cannot register two masternodes.
- **Endpoint uniqueness is enforced** — same commitment hash cannot appear in two active registrations.
- **Spend-lock is enforced** — collateral key image is tracked and spend attempts blocked for 21 days post-deactivation.
- **Heartbeat signatures are verified** — only the holder of the payout key can submit heartbeats for a given masternode.
- **Attestation signatures are verified** — only the holder of the verifier key can submit attestations; per-verifier rate limiting prevents spam.

### Current Limitations (planned improvements)

| Limitation | Impact | Planned fix |
|-----------|--------|-------------|
| **Collateral UTXO not verified at mempool acceptance** | The output key is verified against the UTXO set during block validation (`extractKeyOutputKeys`), but mempool acceptance does not perform this check — a register tx referencing a non-existent output can enter the mempool and will only be rejected at block inclusion | Add UTXO set lookup during `validateMasternodeTransactionEvent` when `checkPoolTokenReplay=true` |
| **No wallet `mn_activate` / `mn_deactivate` / `mn_heartbeat` / `mn_revoke` commands** | Governance operations require external tooling to build transactions | Add wallet commands |
| **Verifier allowlist is stubbed off** | Any key can attest; permissioned verifier governance not yet enforced | Enable via config before mainnet if required |
| **Masternode set hash not committed in block header** | Set hash is advisory-only; not consensus-enforced per block | Consider adding to coinbase extra or block header in future fork |
| **No dedicated P2P gossip for MN messages** | Heartbeats and attestations use normal tx pool propagation | Acceptable for current design; revisit at scale |
| **Heartbeat block space scaling** | With `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL=5`, 100 fee-bearing masternodes consume ~10% of the 100 KB block reward zone per block; saturation requires ~1,000 masternodes | Monitor as masternode count grows; raise interval further if needed |

---

## 19. Configuration Reference

Edit `src/config/CryptoNoteConfig.h` to change any parameter. Requires recompilation.

```cpp
// Fork activation heights
const uint64_t MASTERNODE_FEATURE_FORK_HEIGHT = 4600000;
const uint64_t MASTERNODE_REWARD_FORK_HEIGHT   = MASTERNODE_FEATURE_FORK_HEIGHT + 30 * EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;

// Reward split
const uint64_t MASTERNODE_REWARD_PERCENT = 70; // percent of distributable reward to MN winner

// Collateral — 2,000,000,000 WRKZ × 100 (2 decimal places) = 200,000,000,000 atomic units
const uint64_t MASTERNODE_REGISTRATION_BOND_AMOUNT  = 200'000'000'000;
const uint64_t MASTERNODE_COLLATERAL_LOCK_AMOUNT     = MASTERNODE_REGISTRATION_BOND_AMOUNT;

// Health
const uint64_t MASTERNODE_HEALTH_WINDOW_BLOCKS   = 7 * EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;
const uint64_t MASTERNODE_MIN_HEALTH_PERCENT      = 95;

// Fairness
const uint64_t MASTERNODE_FAIRNESS_WINDOW_BLOCKS  = MASTERNODE_HEALTH_WINDOW_BLOCKS;

// Spend lock
const uint64_t MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS = 21 * EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;

// Registration token
const uint64_t MASTERNODE_REGISTRATION_TOKEN_TTL_BLOCKS = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;

// Heartbeat — 1 per 5 blocks; 2,016 opportunities per 7-day health window
const uint64_t MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL = 5;

// Attestation
const bool     MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION                    = true;
const uint64_t MASTERNODE_ATTESTATION_WINDOW_BLOCKS                       = MASTERNODE_HEALTH_WINDOW_BLOCKS;
const uint64_t MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW                      = 24;
const uint64_t MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT                  = 80;
const uint64_t MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER     = 60;

// Verifier allowlist (set true + populate list to enforce)
const bool MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST = false;
const std::vector<std::string> MASTERNODE_VERIFIER_PUBKEY_ALLOWLIST = {};
```

---

---

## 20. Frequently Asked Questions

### Collateral & Funds

**Q: How does the network know I haven't spent my collateral?**

At registration, your collateral UTXO's **key image** is committed on-chain. Key images are the cryptographic fingerprints of CryptoNote coin spends — spending any UTXO reveals its unique key image in the transaction input. The consensus layer tracks all registered collateral key images in `MasternodeStateTracker`. Every transaction in every block is checked: if any input key image matches a spend-locked collateral key image, the transaction is rejected and never confirmed. The private key for the collateral UTXO is never at risk — the network simply blocks any spending attempt until the lock expires.

---

**Q: I shut down my node without de-registering. Can I recover my collateral?**

Yes — your funds are never seized or destroyed. What happens depends on how the node exits the Active state:

| Scenario | What happens to collateral |
|---|---|
| Node goes offline, health degrades to 0% | Still locked — no spend-lock timer starts until a Deactivate/Revoke tx appears on-chain |
| A Deactivate or Revoke tx is submitted | Spend-lock timer starts at that block height |
| 30,240 blocks (~21 days) pass after Deactivate/Revoke | Collateral is fully unlocked and freely spendable |

If no Deactivate/Revoke tx is ever submitted, the spend-lock timer never starts, and the collateral stays locked indefinitely (the node sits in a degraded state receiving no rewards). To free your collateral you need either a wallet `mn_deactivate` or `mn_revoke` command — these are planned but not yet implemented. In the interim, a governance-level Revoke tx can be used.

---

**Q: Can I use the same collateral for two masternodes?**

No. The Register tx validation checks `hasCollateralKeyImage` and `hasCollateralOutpoint` — the same key image or (amount, global output index) pair cannot appear in two active or pending registrations. If you try, the second register tx is rejected.

---

**Q: Can I move my collateral to a different UTXO without revoking?**

No. The collateral binding is permanent for the lifetime of the masternode registration. To change collateral you must Revoke the existing masternode (wait out the spend-lock), then Register again with the new collateral output.

---

### Health & Heartbeats

**Q: What happens if I miss some heartbeats?**

Each missed heartbeat reduces your health percentage over the 7-day rolling window (10,080 blocks). With `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL = 5`, there are 2,016 heartbeat opportunities per window. To maintain ≥ 95% health you must submit at least 1,916 healthy heartbeats — meaning you can miss at most **100 consecutive heartbeats (~500 blocks ≈ 8 hours)** before falling below the threshold. Once below 95%, you are excluded from reward selection until health recovers within the rolling window.

---

**Q: What happens if my health drops to 0%?**

You stop receiving rewards but the masternode remains in Active status. The collateral is not automatically seized or locked. Health can recover if you resume sending heartbeats — old zero-health samples age out of the 7-day window over time. No Deactivate tx is issued automatically by the network (automatic deactivation on sustained health failure is a planned future improvement).

---

**Q: Do I need to send a heartbeat every block?**

No — and you cannot. The minimum interval is 5 blocks. Heartbeats submitted too soon are rejected. For maximum health you should send exactly one heartbeat every 5 blocks.

---

### Registration & Lifecycle

**Q: Can I change my endpoint (IP address) after registration?**

Not without re-registering. The endpoint commitment is embedded in the Register tx and cannot be updated in-place. To change your public IP you must Revoke the existing masternode and submit a new Register tx with the new endpoint commitment. A future upgrade could add an Update tx type.

---

**Q: Can two masternodes share the same endpoint commitment?**

No. The Register tx validation calls `hasEndpointCommitment`, which rejects any registration whose commitment hash already appears in an active or pending masternode. This prevents a single server from collecting multiple masternode rewards.

---

**Q: What is the difference between Deactivate and Revoke?**

| | Deactivate | Revoke |
|---|---|---|
| Resulting status | Inactive | Revoked |
| Spend-lock | Yes — 21 days | Yes — 21 days |
| Re-register allowed? | Not directly — must Revoke first | Yes — after spend-lock expires |
| Typical use | Temporary removal / maintenance | Permanent exit |

---

**Q: What happens to rewards while the masternode is in Registered status (before Activate)?**

Nothing — a masternode in Registered status is not eligible for rewards. Only Active masternodes are considered for reward selection. The registration establishes the collateral commitment and begins health tracking, but rewards require an explicit Activate transaction.

---

### Rewards

**Q: What if no masternode is eligible for a reward at a given block?**

If no active masternode meets both the health threshold (≥ 95% heartbeat health) and the attestation threshold (≥ 80% attestation health with ≥ 24 samples), the full block reward goes to the PoW miner. No reward is held in reserve or rolled over.

---

**Q: Is the reward winner selection random or deterministic?**

Fully **deterministic** — no randomness is involved. The winner is selected by finding the active eligible masternode with the lowest total reward received in the last 7-day fairness window. Ties are broken by lexicographic comparison of masternode ID bytes. Both nodes independently compute the same winner from the same chain state, ensuring consensus.

---

**Q: Are transaction fees shared with masternodes?**

No. Transaction fees always go 100% to the PoW miner, regardless of whether the reward fork is active.

---

**Q: Can I use a different address to receive rewards than the one holding collateral?**

Yes — this is by design. The **payout key** (where rewards are sent) is separate from the **collateral output key**. You specify the payout key at registration time. It is recommended to keep them in separate wallets so reward income doesn't interact with the spend-locked collateral UTXO.

---

### Attestation

**Q: What is external attestation and do I need to worry about it?**

External attestation is a second liveness check performed by independent **verifier nodes** that submit Attest transactions on-chain. To be reward-eligible your masternode must have ≥ 24 attestation samples in the last 7-day window with ≥ 80% healthy attestations. If `MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION = true` (the current default), a masternode with zero attestation samples will not receive rewards even with 100% heartbeat health. As an operator you cannot control who attests to your node — verifier infrastructure needs to be operational on the network. If no verifiers are running and attestation is required, **no masternode will receive rewards**.

---

**Q: Can I run my own verifier node?**

Yes — `MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST` is currently `false`, meaning any verifier key is accepted. You can run a verifier that submits Attest transactions. Each verifier is rate-limited to one accepted attestation per 60 blocks per masternode to prevent spam. If the allowlist is later enabled (`true`), only pre-approved verifier public keys will be accepted.

---

*This document is a work in progress. Parameters and fork heights are subject to change before mainnet activation.*
