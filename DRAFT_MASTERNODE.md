# WrkzCoin Masternode — Feature Draft

> **Status:** DRAFT — implementation under review on branch `test-mnodes-v2`; pending testnet soak and mainnet fork activation.
> Fork activation heights are configured in `src/config/CryptoNoteConfig.h` and advertised through `FORK_HEIGHTS[]`.
>
> **Revision note (Aug 2026):** fork heights moved to 5,000,000 / 5,200,000; Register payload is now **v3**
> (carries the payout *view* key); every other signed payload carries a creation **height** (anti-replay);
> the masternode reward output is a standard one-time stealth output derived from a deterministic coinbase
> tx key; ChainLock/InstantSend messages are validated against the on-chain masternode set before being
> stored or relayed; lifecycle rules were fixed (Attest allowed while Registered, Revoke allowed from Registered).

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
11. [Signing Key & Quorum Participation](#11-signing-key--quorum-participation)
12. [ChainLock (Block Finality)](#12-chainlock-block-finality)
13. [InstantSend (Transaction Finality)](#13-instantsend-transaction-finality)
14. [Reward Distribution](#14-reward-distribution)
15. [State Persistence](#15-state-persistence)
16. [Operator Manual — Registration Workflow](#16-operator-manual--registration-workflow)
17. [Operator Manual — Running & Maintaining a Masternode](#17-operator-manual--running--maintaining-a-masternode)
18. [RPC API Reference](#18-rpc-api-reference)
19. [Daemon Console Commands](#19-daemon-console-commands)
20. [Wallet Commands (zedwallet++)](#20-wallet-commands-zedwallet)
21. [IPv6 Support](#21-ipv6-support)
22. [Security Model & Known Limitations](#22-security-model--known-limitations)
23. [Configuration Reference](#23-configuration-reference)
24. [Frequently Asked Questions](#24-frequently-asked-questions)

---

## 1. Overview

WrkzCoin masternodes are full-network nodes that lock a collateral bond, commit to a publicly reachable IP endpoint, and submit regular heartbeat transactions to prove liveness. In exchange they receive a deterministic share of the block reward once the reward fork activates.

Beyond reward eligibility, active masternodes participate in two additional network services:

- **ChainLock** — A quorum of masternodes signs block hashes to achieve instant block finality, preventing 51%-attack chain reorganisations.
- **InstantSend** — A quorum of masternodes locks transaction inputs before confirmation, preventing double-spend attacks on zero-confirmation transactions.

The system is designed with the following goals:

- **Economic commitment** — Collateral is locked in-consensus; spending it requires waiting through a spend-lock period after deactivation.
- **Liveness accountability** — Health is tracked via on-chain heartbeat transactions over a rolling 7-day window. Nodes falling below the health threshold are excluded from rewards.
- **Verifier attestation** — External verifier nodes independently attest to masternode liveness on-chain, providing a second check beyond self-reported heartbeats.
- **Deterministic fairness** — The reward winner is selected deterministically from eligible candidates by a fairness algorithm that favours nodes that have received the least reward in the recent window.
- **Block finality** — ChainLock quorums (20 masternodes, 60% threshold) sign block hashes to finalize the chain against reorgs.
- **Zero-conf transaction safety** — InstantSend quorums (10 masternodes, 60% threshold) lock transaction inputs within seconds.
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
│  │  • Signing key registry (for quorums)        │   │
│  └──────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │           ChainLockManager                   │   │
│  │  • Collects votes → assembles ChainLock      │   │
│  │  • Conflict detection (reorg guard)          │   │
│  └──────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │           InstantSendManager                 │   │
│  │  • Collects IS votes → assembles IS lock     │   │
│  │  • Double-spend detection (per key image)    │   │
│  │  • Lock expiry after 60 blocks               │   │
│  └──────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │           MasternodeSigner (daemon thread)   │   │
│  │  • Watches for new blocks / mempool txs      │   │
│  │  • Checks quorum membership (deterministic)  │   │
│  │  • Signs & broadcasts ChainLock votes        │   │
│  │  • Signs & broadcasts InstantSend votes      │   │
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
  /masternodes/count  mn_registration   NOTIFY_CHAINLOCK_VOTE
  /chainlock/:height  _string cmd       NOTIFY_CHAINLOCK
  /instantsend/:hash                    NOTIFY_INSTANTSEND_VOTE
  /getinfo                              NOTIFY_INSTANTSEND_LOCK
  /getblocktemplate
```

---

## 3. Fork Heights & Rollout Schedule

The masternode feature activates in two stages to give operators time to register before rewards begin.

| Fork | Config Constant | Height | `FORK_HEIGHTS[]` index | Approximate Date* |
|------|----------------|--------|------------------------|-------------------|
| Feature fork | `MASTERNODE_FEATURE_FORK_HEIGHT` | 5,000,000 | 22 | ~March 2028 |
| Reward fork | `MASTERNODE_REWARD_FORK_HEIGHT` | 5,200,000 (Feature + 200,000 blocks ≈ 139 days) | 23 | ~July 2028 |

\* Estimated from height 3,925,097 on 2026‑02‑17 at 1,440 blocks/day; re-check against a live `/getinfo` before announcing.

Both heights are also entries in `FORK_HEIGHTS[]` (and `SOFTWARE_SUPPORTED_FORK_INDEX` was bumped to 23), so
`/getinfo` → `upgrade_heights` / `supported_height` and the daemon `status` fork countdown warn operators of
old software before each fork. A `static_assert` in `CryptoNoteConfig.h` keeps the two places in sync.

**Stage 1 — Feature fork (height 5,000,000):**
- Masternode Register, Activate, Deactivate, Penalize, Revoke, Heartbeat, Attest, and UpdateEndpoint transactions are accepted into blocks.
- State tracking begins. Health samples accumulate.
- ChainLock and InstantSend services activate (masternodes with `--mn-signing-key` begin signing).
- No reward split yet — 100% of block reward goes to PoW miner.

**Stage 2 — Reward fork (height 5,200,000, ~139 days after Feature fork):**
- 70% of block reward distributed to the selected masternode winner.
- 30% of block reward goes to PoW miner.
- Transaction fees always go 100% to the PoW miner regardless of fork stage.
- The minimum transaction unlock time drops from 15 to 3 blocks (`UNLOCK_TIME_HEIGHT_V3 = MASTERNODE_REWARD_FORK_HEIGHT`).

> Setting `MASTERNODE_FEATURE_FORK_HEIGHT` to `0` disables the feature entirely (used for dev/test builds);
> `MASTERNODE_REWARD_FORK_HEIGHT` must then be left as is (a `static_assert` rejects `0`). Testnet builds that
> want to exercise masternodes must recompile with a low feature height — the `--network testnet` profile
> does not override fork heights.

---

## 4. Consensus Parameters

All values are defined in `src/config/CryptoNoteConfig.h` under `CryptoNote::parameters`.

### Masternode Parameters

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
| `MASTERNODE_ENDPOINT_UPDATE_COOLDOWN_BLOCKS` | 10,080 (7 days) | Minimum spacing between accepted endpoint update txs |
| `MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION` | true | Whether attestation threshold must be met for rewards |
| `MASTERNODE_ATTESTATION_WINDOW_BLOCKS` | 10,080 (7 days) | Rolling window for attestation health |
| `MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW` | 24 | Minimum attestation samples needed in window |
| `MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT` | 80% | Minimum attestation health percentage |
| `MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER` | 60 blocks | Minimum spacing per verifier between accepted attestations |
| `MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST` | false | Whether verifier key allowlist is enforced |
| `MASTERNODE_ENFORCE_REGISTRATION_AUTHORITY` | false | When true only payout keys in `MASTERNODE_REGISTRATION_AUTHORITY_PUBKEYS` may register |
| `MASTERNODE_SIGNED_PAYLOAD_MAX_AGE_BLOCKS` | 120 | A signed payload's creation height may be at most this many blocks behind the including block |
| `MASTERNODE_SIGNED_PAYLOAD_FUTURE_TOLERANCE_BLOCKS` | 2 | ... and at most this many blocks ahead of it |

### ChainLock Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `CHAINLOCK_QUORUM_SIZE` | 20 | Masternodes selected per block quorum |
| `CHAINLOCK_THRESHOLD` | 12 (60%) | Minimum votes required to form a valid ChainLock |
| `CHAINLOCK_VOTE_FUTURE_WINDOW_BLOCKS` | 1 | Votes/locks for heights above `top + 1` are dropped (the quorum for H needs block H-1) |
| `CHAINLOCK_VOTE_MAX_AGE_BLOCKS` | 60 | Votes/locks for heights below `top - 60` are dropped |
| `CHAINLOCK_MAX_PENDING_VOTES_PER_HEIGHT` | 40 | Upper bound on not-yet-assembled votes kept per height (one vote per masternode per height) |
| `CHAINLOCK_PENDING_LOCK_EXPIRY_SECONDS` | 3,600 | A lock whose block never arrives stops rejecting competing blocks after this long (liveness valve) |
| `CHAINLOCK_RETENTION_BLOCKS` | 1,440 | Assembled locks older than this are dropped from memory / snapshot |
| `MASTERNODE_P2P_MAX_VOTES_PER_MESSAGE` / `_MAX_KEY_IMAGES_PER_MESSAGE` | 64 | Hard wire-level caps, enforced before any allocation |

### InstantSend Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `INSTANTSEND_QUORUM_SIZE` | 10 | Masternodes selected per transaction quorum |
| `INSTANTSEND_THRESHOLD` | 6 (60%) | Minimum votes required to form a valid IS lock |
| `INSTANTSEND_LOCK_EXPIRY_BLOCKS` | 60 | Blocks before an unconfirmed IS lock expires |
| `INSTANTSEND_MAX_INPUTS` | 10 | Maximum inputs a tx may have to qualify for IS |
| `INSTANTSEND_QUORUM_CYCLE_BLOCKS` | 60 | IS quorum rotates every 60 blocks, seeded by the block hash at the cycle start |

---

## 5. Transaction Types

Masternode operations are embedded in standard CryptoNote transactions via the `extra` field. They are identified by the 4-byte magic prefix `MN01` (bytes: `0x4D 0x4E 0x30 0x31`).

| Type byte | Name | Who sends (signature key) | Purpose |
|-----------|------|---------------------------|---------|
| `0x01` | **Register** | Operator wallet (payout key + collateral proof) | Lock collateral, commit endpoint, begin lifecycle |
| `0x02` | **Activate** | Operator (payout key) | Registered/Inactive → Active (requires attestation threshold) |
| `0x03` | **Deactivate** | Operator (payout key) | Active → Inactive; start spend-lock |
| `0x04` | **Penalize** | Operator (payout key) — no governance key exists yet | Active/Inactive → Penalized |
| `0x05` | **Revoke** | Operator (payout key) | Registered/Active/Inactive/Penalized → Revoked; start spend-lock |
| `0x06` | **Heartbeat** | Operator daemon (payout key) | Report liveness (zero-input tx) |
| `0x07` | **Attest** | Verifier (any key unless allowlist enabled) | Independently attest liveness of a Registered or Active MN |
| `0x08` | **UpdateEndpoint** | Operator (payout key) | Replace endpoint commitment in-place (7-day cooldown) |

All payloads share the header `"MN01" | type(1) | masternodeId(32)` (37 bytes). The authoritative layout and the
builder functions live in `src/cryptonotecore/MasternodeTx.h` (`buildMasternode*UnsignedPayload`), shared by the
wallet, the daemon signer and consensus.

### Signed payload anti-replay (all types except Register)

Every Heartbeat / Attest / Activate / Deactivate / Penalize / Revoke / UpdateEndpoint payload carries a
4-byte LE **creation height** right after the header (the next block height as seen by the signer). Consensus
accepts the payload only if:

- `height <= includingBlockHeight + MASTERNODE_SIGNED_PAYLOAD_FUTURE_TOLERANCE_BLOCKS` (2), and
- `includingBlockHeight - height <= MASTERNODE_SIGNED_PAYLOAD_MAX_AGE_BLOCKS` (120), and
- it is **strictly newer** than the creation height of the last accepted payload of the same kind: for
  heartbeats, the previous accepted heartbeat's creation height; for lifecycle/update payloads, the last
  accepted lifecycle payload of that masternode; for attestations, the previous attestation by the same
  verifier for that masternode. These counters are kept in the masternode state and survive revoke +
  re-register (a re-registration keeps the sample windows; only lifecycle/collateral/endpoint/keys reset).

A captured payload can therefore never be re-broadcast later by a third party (or by the operator) — e.g. an
operator cannot sign one heartbeat and replay it forever, and nobody can replay an old Deactivate/Revoke
against a re-activated masternode. Stale payloads are also purged from the mempool on every new block and are
never placed into block templates.

| Payload | Layout (after header) | Total size |
|---------|------------------------|-----------|
| Heartbeat | `height(4) \| healthy(1) ‖ sig(64)` | 106 bytes |
| Attest | `height(4) \| verifierKey(32) \| healthy(1) ‖ sig(64)` | 138 bytes |
| Activate / Deactivate / Penalize / Revoke | `height(4) ‖ sig(64)` | 105 bytes |
| UpdateEndpoint | `height(4) \| newEndpointCommitment(32) ‖ sig(64)` | 137 bytes |

### Register payload fields (v3 format)

The wallet `mn_register` command generates a **v3** Register payload. Compared with v2 it adds the **payout
view key**, so the registration commits to a full payout *address* (spend + view key). That is what allows the
coinbase to pay a standard one-time (stealth) output that the operator's wallet detects and can spend (see §14).
Earlier v1/v2 layouts are no longer accepted (no mainnet registrations exist yet).

| Field | Type | Notes |
|-------|------|-------|
| Magic `MN01` | 4 bytes | Identifies MN tx |
| Type `0x01` | 1 byte | Register |
| Masternode ID | 32 bytes (Hash) | Randomly chosen by operator |
| Payout key | 32 bytes (PublicKey) | Wallet public **spend** key; signs lifecycle/heartbeat payloads; part of payout address |
| Registration token ID | 32 bytes (Hash) | From daemon `mn_registration_string` command |
| Token expiry height | 4 bytes (LE uint32) | Must be ≥ current block height |
| Collateral amount | 8 bytes (LE uint64) | Must be ≥ `MASTERNODE_COLLATERAL_LOCK_AMOUNT` |
| Collateral global output index | 4 bytes (LE uint32) | Index in global output set |
| Collateral key image | 32 bytes (KeyImage) | Prevents double-use; must NOT be spent by the Register tx itself |
| Collateral output key | 32 bytes (PublicKey) | Used to verify collateral proof |
| Endpoint commitment | 32 bytes (Hash) | `cn_fast_hash("MNIP1|<canonical_addr>")` |
| Signing key | 32 bytes (PublicKey) | Dedicated key for ChainLock/InstantSend quorum votes (required) |
| **Payout view key** | **32 bytes (PublicKey)** | **Wallet public view key; completes the payout address** |
| Payout key signature | 64 bytes (Signature) | Signs unsigned payload with payout (spend) key |
| Collateral proof | 64 bytes | DLEQ proof binding collateral output key ↔ key image, over the unsigned payload hash |

Total: 277 bytes unsigned + 128 bytes of signatures = 405 bytes.

> **Note:** The signing private key is generated by the wallet and displayed once at registration. Save it securely — it must be provided to the daemon via `--mn-signing-key=<hex>` to enable ChainLock and InstantSend signing.

---

## 6. Masternode Lifecycle

```
                  Register tx
                      │
                      ▼
               ┌─────────────┐   Attest txs (verifiers)
               │  Registered  │◄──── Re-register (if Revoked)
               └──────┬──────┘
                      │ Activate tx (needs ≥24 attestations / 80% healthy)
                      ▼
               ┌─────────────┐
               │   Active     │◄──────────────┐
               └──────┬──────┘               │ Activate tx
                      │ Heartbeat txs         │ (re-activate)
                      │ Attest txs            │
                      │ Deactivate tx         │
                      ▼                       │
               ┌─────────────┐───────────────┘
               │  Inactive   │
               └──────┬──────┘
                      │ Penalize tx (from Active or Inactive)
                      ▼
               ┌─────────────┐
               │  Penalized  │
               └──────┬──────┘
                      │ Revoke tx (allowed from Registered, Active, Inactive, Penalized)
                      ▼
               ┌─────────────┐
               │   Revoked   │──► Re-register allowed (same mn_id; same collateral only after the spend-lock)
               └─────────────┘
```

Consensus transition rules (`Core::validateMasternodeTransactionEventWithTracker`):

| Tx | Allowed from | Notes |
|----|--------------|-------|
| Activate | Registered, Inactive | bonded + collateral binding + attestation threshold + payload height newer than last lifecycle payload |
| Deactivate | Active | starts spend-lock |
| Penalize | Active, Inactive | starts spend-lock; currently self-signed (no governance key) |
| Revoke | Registered, Active, Inactive, Penalized | starts spend-lock; the exit path for a registration that never activated |
| Heartbeat | Active | see §9 |
| Attest | Registered, Active | see §10 |
| UpdateEndpoint | Registered, Active | 7-day cooldown |

**Spend-lock:** While Registered or Active the collateral key image is spend-locked indefinitely. After Deactivate,
Penalize or Revoke it stays locked for `MASTERNODE_DEACTIVATION_SPEND_LOCK_BLOCKS` (21 days) counted from that
transaction. Transactions that attempt to spend it during this period are rejected by consensus. If the same
key image appears in more than one record (revoked then re-registered), it is locked if *any* record locks it.

**Reward eligibility:** Only nodes in **Active** status that meet both the heartbeat health threshold and attestation threshold are considered for rewards.

**Quorum eligibility:** All **Active** masternodes with a registered signing key are eligible to be selected into ChainLock and InstantSend quorums, regardless of health or attestation status.

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

A masternode daemon **automatically** submits a **Heartbeat** tx (type `0x06`) at most once per `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` blocks when `--mn-payout-key` is configured and the daemon is synchronized. No manual action or wallet is required. The tx includes:
- Masternode ID (32 bytes)
- Creation height (4 bytes LE) — next block height as seen by the signer (anti-replay, see §5)
- `healthy` flag (1 byte: `0x01` = healthy, `0x00` = unhealthy)
- Signature over the unsigned payload using the payout key

### Zero-Input Heartbeat Format

Heartbeat transactions are **zero-input, zero-output** by design — they carry no funds, pay no fee, and consume negligible block space (106-byte payload, ~115 bytes on the wire). Normal input/fee/PoW validation is bypassed for this tx type; authorization is cryptographically enforced via the payout-key signature in the `extra` field.

**Spam prevention layers** (enforced in consensus):
1. Structural parse gate — extra must match the exact MN01 type `0x06` layout (106 bytes)
2. Active status check — the MN must be in `Active` status at the time of the heartbeat
3. Rate limiting — at most one heartbeat per `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` blocks per MN ID
4. Anti-replay — the payload height must be within the signed-payload window and strictly newer than the inclusion height of the previous accepted heartbeat
5. Payout-key signature — only the registered payout-key holder can produce a valid heartbeat (checked before any pool scan)
6. Mempool deduplication — a second heartbeat for the same MN ID is rejected if one is already in the pool; stale heartbeats are purged from the pool on every block and never enter block templates

The 2B WRKZ collateral requirement makes spam attacks economically irrational even if all layers were bypassed.

### Health Calculation

The daemon tracks up to `MASTERNODE_HEALTH_WINDOW_BLOCKS` (10,080) heartbeat samples in a rolling deque. At any point:

```
health_percent = (healthy_samples_in_window / total_samples_in_window) * 100
```

A node must maintain ≥ **95%** health to be reward-eligible.

### Rate Limiting

- Minimum spacing: `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` (5 blocks) between accepted heartbeats.
- Heartbeats received too quickly (or outside the anti-replay window) are rejected with the generic `WRONG_FEE` validation error; malformed payloads are rejected with `EXTRA_TOO_LARGE`.

### Block Space Impact

Each heartbeat is a **zero-input, zero-output** transaction (~110 bytes). There is no fee and no computational PoW required from the operator. The `MasternodeSigner` heartbeat thread handles submission automatically.

With `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL = 5`, each masternode submits at most 1 heartbeat per 5 blocks. The health window (10,080 blocks) provides 2,016 opportunities per masternode; meeting the 95% threshold requires 1,916 healthy samples, allowing up to 100 missed heartbeats (~500 blocks ≈ 8 hours) per window.

**Zero-input heartbeat txs (~115 bytes each):**

| Active masternodes | Heartbeat txs/block | Block space consumed |
|---|---|---|
| 10 | 2 | ~230 B (<1%) |
| 50 | 10 | ~1.2 KB (1%) |
| 100 | 20 | ~2.3 KB (2%) |
| 500 | 100 | ~12 KB (12%) |
| 1,000 | 200 | ~23 KB (23%) |
| 4,000 | 800 | ~92 KB (92% — approaching limit) |

---

## 10. External Attestation

In addition to self-reported heartbeats, external **verifier nodes** submit Attest transactions that independently rate the masternode's liveness. Attestations are accepted for masternodes in **Registered** or **Active** status — a freshly registered masternode needs `MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW` attestations before it can Activate.

### Attestation Transaction

- Masternode ID (32 bytes)
- Creation height (4 bytes LE) — anti-replay, see §5
- Verifier public key (32 bytes)
- `healthy` flag (1 byte)
- Signature by the verifier key

A captured attestation cannot be replayed: its height must be within the signed-payload window and strictly newer than the previous accepted attestation by the same verifier for that masternode.

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

### Submitting Attestations (Wallet Command)

Any wallet can act as a verifier using the `mn_attest` command in `zedwallet++`:

```
mn_attest <mn_id_hex> <0|1>
```

- `<mn_id_hex>` — the 64-character hex masternode ID
- `<0|1>` — `1` for healthy, `0` for unhealthy

The wallet's primary spend key is used as the verifier key. Rate limiting is enforced on-chain (one accepted attestation per 60 blocks per verifier key per masternode).

---

## 11. Signing Key & Quorum Participation

Each masternode registration includes a **signing key** — a dedicated Ed25519 key pair used exclusively for ChainLock and InstantSend quorum voting. This key is separate from both the payout key and the collateral key.

### Key Generation

The signing key pair is generated by `zedwallet++` during `mn_register`. At the end of registration:

```
Masternode signing private key: <64-hex-chars>
Save this key and run the daemon with:
  --mn-signing-key=<hex> to enable ChainLock/InstantSend signing.
```

The **public** signing key is committed on-chain in the Register payload. The **private** key is shown once and must be stored by the operator.

### Daemon Configuration

To enable quorum signing, pass the signing private key to the daemon:

```bash
wrkzd --mn-signing-key=<64-hex-chars>
```

Or add it to your daemon configuration file:

```
mn-signing-key=<64-hex-chars>
```

The daemon derives the public key from the private key at startup, looks up the matching masternode registration on-chain, and starts `MasternodeSigner` background threads for ChainLock, InstantSend signing, and (optionally) automated heartbeat.

### MasternodeSigner

`MasternodeSigner` runs up to three background threads:

1. **ChainLock loop** — Monitors new blocks. For each new block tip, checks whether this masternode is in the 20-node quorum (selected deterministically from the active set using the block hash as seed). If in quorum, signs a `CLV1` vote and broadcasts it via P2P.

2. **InstantSend loop** — Monitors the transaction mempool. For each qualifying transaction (≤ 10 inputs), checks whether this masternode is in the 10-node quorum (selected using the tx hash as seed). If in quorum, signs an `ISV1` vote and broadcasts it via P2P.

3. **Heartbeat loop** *(requires `--mn-payout-key`)* — Monitors new blocks. Every `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` (5) blocks, if the masternode is Active, builds and submits a zero-input heartbeat transaction signed with the payout private key. The transaction bypasses normal fee/PoW validation; signature and rate-limit checks ensure only legitimate operators can submit.

If `--mn-signing-key` is not provided, `MasternodeSigner` does not start. If `--mn-payout-key` is not provided, the heartbeat loop does not run (the node participates in quorums but does not auto-heartbeat).

---

## 12. ChainLock (Block Finality)

ChainLocks provide **instant block finality** by having a deterministic quorum of masternodes sign the winning block hash at each height. A block with a valid ChainLock cannot be reorganised away without invalidating the quorum signatures — effectively neutralising 51% hash-rate attacks against confirmed blocks.

### How It Works

1. A new block arrives at height *H* with block hash *B*.
2. The daemon selects a quorum of up to **20** active masternodes deterministically, seeded by the hash of the
   **previous** block *P* = hash(H−1) — something neither a voter nor a forger can choose without mining it:
   ```
   sort_key(mnId) = H(mnId || P)
   quorum = first 20 by sort_key ascending        (Core::getChainLockQuorum)
   ```
3. Each quorum member (if running with `--mn-signing-key`) signs a **ChainLock vote**:
   ```
   preimage  = "CLV1" || height_LE4 || blockHash32
   signature = sign(signing_private_key, sha256(preimage))
   ```
   The vote is broadcast via `NOTIFY_CHAINLOCK_VOTE` P2P message.
4. When a node collects ≥ **12** valid votes for the same `(height, blockHash)`, a **ChainLock** is assembled and broadcast via `NOTIFY_CHAINLOCK`.
5. A confirmed ChainLock for height *H* prevents any alternative block at height *H* from being accepted by nodes that have seen the lock.

### Validation and relay policy

Votes and assembled locks are **not trusted on receipt**. Before anything is stored or relayed, `Core` checks:

- the masternode feature fork is active locally;
- `height` is within `(top − CHAINLOCK_VOTE_MAX_AGE_BLOCKS, top + 1]` (block H−1 must be known to derive the quorum);
- if we already have a block at `height`, `blockHash` must match our chain (a lock never makes a node reorg);
- `masternodeId` is a registered masternode, `signingKey` equals its on-chain signing key, and the masternode
  is in the deterministic quorum for `height` (seeded by hash(H−1), not by the voted hash);
- one vote per masternode per height — a second vote with a different hash is equivocation and is dropped;
- for assembled locks: between `CHAINLOCK_THRESHOLD` and `CHAINLOCK_QUORUM_SIZE` votes, all from **distinct**
  masternodes, each passing the checks above, each with a valid signature;
- pending votes per height are capped (`CHAINLOCK_MAX_PENDING_VOTES_PER_HEIGHT`), old pending votes are pruned and
  assembled locks older than `CHAINLOCK_RETENTION_BLOCKS` are dropped.

A vote is relayed to peers **only** if it was new and valid for us (`Added` / `Assembled`); rejected and
duplicate votes are dropped. This prevents relay loops and makes it impossible for one bad vote to be flooded
network-wide. Wire messages hard-cap the number of votes / key images before any allocation happens.

### Liveness valve

A ChainLock is enforced only when extending the main chain tip, and a lock whose block we have **not** seen
locally stops rejecting competing blocks after `CHAINLOCK_PENDING_LOCK_EXPIRY_SECONDS` (1 hour). Once the
locked block is accepted the lock is permanent. With the membership validation above, forging a lock requires
≥12 real masternode signing keys; the valve exists so that even a bad lock produced by a bug or a compromised
quorum can never stall a node forever. ChainLocks are still advisory with respect to reorgs onto a heavier chain.

### ChainLock Vote Fields

| Field | Type | Notes |
|-------|------|-------|
| `height` | uint32 | Block height being locked |
| `blockHash` | 32 bytes | Block hash being locked |
| `masternodeId` | 32 bytes | Voting masternode's ID |
| `signingKey` | 32 bytes | Masternode's registered signing public key |
| `signature` | 64 bytes | Signs the `CLV1` preimage |

### Querying ChainLocks

Via RPC:
```
GET /chainlock/<height>
```

Response:
```json
{
  "locked": true,
  "block_hash": "abcdef...",
  "vote_count": 14,
  "votes": [
    { "mn_id": "...", "signing_key": "..." },
    ...
  ],
  "status": "OK"
}
```

If no ChainLock exists for the height, `"locked": false` is returned with no vote data.

---

## 13. InstantSend (Transaction Finality)

InstantSend provides **zero-confirmation transaction safety** by having a deterministic quorum of masternodes lock transaction inputs before the transaction is included in a block. A double-spend attempt against an IS-locked input is rejected by nodes that have seen the lock.

### How It Works

1. A transaction *T* with hash *H* enters the mempool.
2. The daemon selects a quorum of up to **10** active masternodes deterministically. The quorum rotates every
   `INSTANTSEND_QUORUM_CYCLE_BLOCKS` (60) blocks and is seeded by the hash *C* of the block at the start of the
   current cycle — never by the tx hash, which the sender could grind until their own masternodes form the quorum:
   ```
   C = hash(block at top − (top mod 60))
   sort_key(mnId) = H(mnId || C)
   quorum = first 10 by sort_key ascending        (Core::getInstantSendQuorum)
   ```
3. Transactions with more than **10 inputs** do not qualify for InstantSend.
4. Each quorum member signs an **InstantSend vote**:
   ```
   preimage  = "ISV1" || txHash32
   signature = sign(signing_private_key, sha256(preimage))
   ```
   The vote is broadcast via `NOTIFY_INSTANTSEND_VOTE` P2P message.
5. When ≥ **6** valid votes for the same `txHash` are collected, an **InstantSend lock** is assembled covering all of the transaction's key images, and broadcast via `NOTIFY_INSTANTSEND_LOCK`.
6. Any subsequent transaction in the mempool spending the same key image is flagged as a double-spend conflict and rejected **from the mempool**.
7. The IS lock is cleared once the transaction confirms in a block. If the transaction is not confirmed within **60 blocks**, the lock expires and the inputs are freed.

InstantSend is a **mempool policy, not a block validity rule**: the lock set is local, non-consensus state, so
a PoW-valid block that mines a competing spend is still accepted and simply drops the lock covering that key
image. Votes and assembled locks are only accepted for transactions present in the local pool with
≤ `INSTANTSEND_MAX_INPUTS` inputs, and a lock must cover exactly that transaction's key images (the key images
are never taken from the wire); votes and locks go through the same membership / distinctness /
relay-on-accept policy as ChainLock.

### InstantSend Vote Fields

| Field | Type | Notes |
|-------|------|-------|
| `txHash` | 32 bytes | Transaction being locked |
| `masternodeId` | 32 bytes | Voting masternode's ID |
| `signingKey` | 32 bytes | Masternode's registered signing public key |
| `signature` | 64 bytes | Signs the `ISV1` preimage |

### Querying InstantSend Locks

Via RPC:
```
GET /instantsend/<txhash>
```

Response (locked):
```json
{
  "tx_hash": "abcdef...",
  "is_locked": true,
  "locked": true,
  "lock_tx_hash": "abcdef...",
  "key_images": ["...", "..."],
  "vote_count": 7,
  "status": "OK"
}
```

Response (not locked):
```json
{
  "tx_hash": "abcdef...",
  "is_locked": false,
  "locked": false,
  "lock_tx_hash": "",
  "key_images": [],
  "vote_count": 0,
  "status": "OK"
}
```

(`locked` is an alias of `is_locked`. If the queried tx is not itself locked but one of its inputs is locked
to another transaction, `lock_tx_hash` names that other transaction.)

---

## 14. Reward Distribution

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
5. Ties are broken deterministically: never-paid nodes first, then lowest `last_paid_height`, then lexicographically smallest masternode ID.

If no eligible candidate exists, the full reward goes to the PoW miner.

### Reward Output

The selected masternode winner receives a dedicated output (`outputs[1]`) in the coinbase transaction, paid to
the registered **payout address** (payout spend key `B` + payout view key `A` from the Register payload).

Because validators cannot verify a stealth output without the transaction secret key, a block that pays a
masternode reward MUST use a **deterministic coinbase tx key**:

```
r = Hs("MNCB1" || height_LE4 || previousBlockHash)      (Core::deriveMasternodeCoinbaseTxSecretKey)
R = r·G                                                   (tx public key in the coinbase extra)
P = Hs(r·A || 1)·G + B                                    (standard CryptoNote one-time output key, index 1)
```

Consensus recomputes `r`, checks that the coinbase tx public key equals `R` and that `outputs[1].key == P`
(`BLOCK_REWARD_MISMATCH` otherwise). The operator's wallet scans the coinbase exactly like any other
transaction (`a·R` = `r·A`) and can spend the output normally — every payout has a fresh one-time key and a
fresh key image. `r` is public, which only reveals what is already deterministic (who the winner is); the
miner's own output in the same coinbase uses the same `R` and is still only linkable by someone who knows the
miner's address.

The payout address is separate from the collateral output, so rewards land on a different output than the collateral.

### Masternode Set Hash

At each block height, the daemon computes a deterministic hash over the set of all active masternode IDs ordered by ID. This `masternode_set_hash` is returned in `/getblocktemplate` and `/getinfo`. Miners can supply `expected_masternode_set_hash` to detect if their view of the masternode set differs from the node's.

---

## 15. State Persistence

Masternode state is kept in memory as a `MasternodeStateTracker` object in `Core`. It is persisted in two ways:

### JSON Snapshot

Path: `<datadir>/DB/masternode_state.json`

- Written on: successful block merge (new main chain tip), blockchain rewind.
- Read on: daemon startup (before sync resumes).
- Fallback: if the snapshot is absent, invalid, or from an older chain height, the daemon replays all blocks from `MASTERNODE_FEATURE_FORK_HEIGHT` to rebuild state. This is safe but slow on long chains.
- The snapshot records `top_height` **and** `top_hash`; a snapshot that does not describe the current tip is discarded and the state is rebuilt from the chain.
- Per-masternode fields include `payout_key`, `payout_view_key` (required), `last_lifecycle_height` and `last_heartbeat_payload_height` (anti-replay counters), health / attestation (with `p` = payload height) / reward sample windows and the signing key. Unknown `status` values are rejected on load.

### DB Blob

The state is also serializable via the key prefix `m/state` in `DBUtils.h`, used for cross-validation during DB-backed cache operations.

---

## 16. Operator Manual — Registration Workflow

### Prerequisites

- A synced `wrkzd` daemon
- A `zedwallet++` wallet with at least **2,000,000,000 WRKZ** in a single unspent output **plus a separate small unlocked output** to pay for the registration transaction (the Register tx must not spend the collateral output — consensus rejects it, and the wallet refuses to build such a transaction)
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
Required collateral minimum: 2000000000.00 WRKZ (200000000000 atomic units)
Wallet CLI command: mn_register MNREG2:... <addr:port | [ipv6]:port>
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
4. Generate a fresh signing key pair for ChainLock/InstantSend quorum participation
5. Build the v3 Register payload (payout spend + view key, collateral binding, endpoint commitment, signing key) with dual signatures (payout key + collateral proof)
6. Fund the transaction from **other** outputs only — it refuses to continue if the collateral output would be consumed
7. Display a confirmation summary and prompt for approval
8. Broadcast the transaction
9. Print the **signing private key** — save it immediately for use with `--mn-signing-key`

### Step 3 — Configure the Daemon

Add the signing key and payout key to your daemon configuration:

```ini
mn-signing-key=<64-hex-chars printed at registration>
mn-payout-key=<64-hex-chars of wallet spend private key>
```

Or pass on the command line:

```bash
wrkzd --mn-signing-key=<64-hex-chars> \
      --mn-payout-key=<64-hex-chars>
```

- **`mn-signing-key`** — Enables ChainLock/InstantSend quorum participation. The public key was embedded in the Register tx.
- **`mn-payout-key`** — Enables automated heartbeat submission. This is the spend private key of the wallet address used as the payout address during `mn_register`. You can export it from `zedwallet++` with `backup` (shows the private spend key).

> **Key handling:** `--dump-config` and `--save-config` serialise these keys in plaintext. Keep config files
> with `mn-signing-key` / `mn-payout-key` readable only by the daemon user.

### Step 4 — Get attested, then Activate (Wallet)

Activation requires the attestation threshold (≥ `MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW` = 24 samples, ≥ 80 %
healthy, within the 7-day window). Verifiers can attest a **Registered** masternode with `mn_attest`; with the
verifier allowlist disabled any wallet can act as a verifier, and one verifier is rate-limited to one accepted
attestation per 60 blocks per masternode (so a single verifier needs ~1 day to provide 24 samples).

After the Register tx is confirmed and the attestation threshold is met, submit an **Activate** transaction to move the masternode from `Registered` → `Active` status:

```
mn_activate <mn_id_hex>
```

Or run `mn_activate` interactively — you will be prompted for the masternode ID.

The wallet signs the action payload with the primary spend key (the payout key registered in Step 2) and broadcasts a small transaction. The masternode transitions to `Active` status once the transaction is confirmed.

### Step 5 — Automated Heartbeat

Once active, the daemon **automatically** submits heartbeat transactions. To enable automated heartbeat, add `--mn-payout-key` to your daemon startup:

```bash
wrkzd --mn-signing-key=<signing-private-key-hex> \
      --mn-payout-key=<payout-private-key-hex>
```

The `--mn-payout-key` is the spend private key of the wallet address registered as the payout address during `mn_register`. With this key configured, the daemon's `MasternodeSigner` thread automatically submits a zero-input, zero-fee heartbeat every `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL` blocks (every ~5 minutes at 1-minute block time).

> **Security note:** The payout private key gives the ability to sign heartbeats and lifecycle transactions for your masternode. Keep it secure and do not share it. It does **not** by itself allow spending the collateral UTXO (which requires the separate key pair embedded in the collateral output).

---

## 17. Operator Manual — Running & Maintaining a Masternode

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
- Configure `--mn-payout-key` so the daemon auto-submits heartbeats every 5 blocks.
- Monitor `health_percent` via `masternodes` console command or `GET /masternodes` RPC.
- A node falling below 95% health is excluded from reward selection until health recovers (there is no automatic deactivation).
- Deactivating, penalizing or revoking a node starts the 21-day spend-lock on its collateral.

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

## 18. RPC API Reference

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
- `limit` (default 100, max 1000)
- `offset` (default 0)

**Response:**
```json
{
  "offset": 0,
  "limit": 100,
  "count": 42,
  "total": 42,
  "masternodes": [
    {
      "mn_id": "abcdef...",
      "state": "active",
      "bonded": true,
      "bond_amount": 200000000000,
      "has_collateral": true,
      "collateral_amount": 200000000000,
      "collateral_global_output_index": 12345,
      "has_endpoint_commitment": true,
      "endpoint_commitment": "deadbeef...",
      "health_percent": 98,
      "spend_locked": false,
      "last_paid_height": 5201234,
      "reward_in_fairness_window": 700000000,
      "has_signing_key": true,
      "signing_key": "..."
    }
  ],
  "status": "OK"
}
```

`state` values are lowercase: `registered`, `active`, `inactive`, `penalized`, `revoked`.

### `GET /masternode/<mn_id_hex>`

Returns a single masternode snapshot (same fields as above) or a 404-style error when unknown.

### `GET /chainlock/<height>`

Returns the ChainLock status for a given block height.

**Response (locked):**
```json
{
  "locked": true,
  "block_hash": "abcdef...",
  "vote_count": 14,
  "votes": [
    { "mn_id": "...", "signing_key": "..." }
  ],
  "status": "OK"
}
```

**Response (not locked):**
```json
{
  "locked": false,
  "status": "OK"
}
```

### `GET /instantsend/<txhash>`

Returns the InstantSend lock status for a transaction.

**Response:** see §13 (`tx_hash`, `is_locked` / `locked`, `lock_tx_hash`, `key_images`, `vote_count`, `status`).

### `GET /getinfo` — Masternode fields

```json
{
  "masternode_feature_fork_active": true,
  "masternode_reward_fork_active": false,
  "masternode_eligible_count": 12,
  "masternode_set_hash": "aabbcc...",
  "masternode_reward_winner": "mnid_hex_or_empty_string",
  "upgrade_heights": [ "...", 4500000, 5000000, 5200000 ],
  "supported_height": 5200000
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

## 19. Daemon Console Commands

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

Output includes the `MNREG2:...` token string, expiry height, required collateral minimum and the wallet command to run.

### `print_chainlocks [count]` / `print_islocks`

Dump the most recent assembled ChainLocks (height, block hash, vote count) and the current InstantSend locks.

---

## 20. Wallet Commands (zedwallet++)

All masternode lifecycle operations are performed from `zedwallet++`. Commands can be invoked with arguments inline or run interactively (the wallet will prompt for missing inputs).

### `mn_register <token> <addr:port | [ipv6]:port>`

Register a masternode using a daemon-issued token and an endpoint commitment.

```
mn_register MNREG2:abc...def:123...456:4601440 203.0.113.10:17855
mn_register MNREG2:abc...def:123...456:4601440 [2001:db8::1]:17855
```

See Section 16 for the full registration workflow.

### `mn_activate <mn_id_hex>`

Activate a registered masternode, moving it from `Registered` → `Active` status and making it eligible for rewards and quorum selection.

```
mn_activate abc...def
```

Signed with the wallet's primary spend key (the payout key). The masternode must be in `Registered` or `Inactive` status, bonded, have a valid collateral binding, and meet the attestation threshold (§10). The payload carries the wallet's current network height (anti-replay).

### `mn_deactivate <mn_id_hex>`

Voluntarily deactivate an active masternode, moving it from `Active` → `Inactive`. Starts the 21-day collateral spend-lock timer.

```
mn_deactivate abc...def
```

Use this for planned maintenance or temporary removal from the active set. To re-enter the active set run `mn_activate` again (the attestation threshold must still be met).

### `mn_revoke <mn_id_hex>`

Permanently revoke a masternode, removing it from the active set. Allowed from `Registered`, `Active`, `Inactive` and `Penalized` — so a registration that was never activated can always be unwound. Starts the 21-day collateral spend-lock timer; after it expires the collateral can be freely spent or used to register a new masternode.

```
mn_revoke abc...def
```

**Warning:** This action is irreversible. The masternode is permanently removed from consensus tracking (the same `mn_id` may be registered again later).

### `mn_attest <mn_id_hex> <0|1>`

Submit a verifier attestation for a masternode. The wallet's primary spend key acts as the verifier key.

```
mn_attest abc...def 1    # attest healthy
mn_attest abc...def 0    # attest unhealthy
```

Rate-limited to one accepted attestation per 60 blocks per verifier key per masternode.

---

## 21. IPv6 Support

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

## 22. Security Model & Known Limitations

### What the consensus layer guarantees

- **Collateral double-use is prevented** — same key image or global output index cannot be used by two active masternodes simultaneously; a Register tx cannot spend its own collateral, and a Register whose collateral is spent earlier in the same block (or by another pool transaction) is rejected.
- **Block templates are built in consensus order** — the template builder applies masternode events as it places MN transactions, so a miner never includes two MN transactions that contradict each other (e.g. Deactivate + Heartbeat for one masternode).
- **Token replay is prevented** — registration token IDs are tracked; same token cannot register two masternodes.
- **Endpoint uniqueness is enforced** — same commitment hash cannot appear in two active registrations.
- **Spend-lock is enforced** — collateral key image is tracked and spend attempts blocked while Registered/Active and for 21 days after Deactivate/Penalize/Revoke; the check is order-independent when a key image appears in several records.
- **Signed payloads are single-use** — every heartbeat / attestation / lifecycle / endpoint-update payload carries a creation height and must be strictly newer than the previous accepted one of its kind; captured payloads cannot be replayed.
- **Heartbeat signatures are verified** — only the holder of the payout key can submit heartbeats for a given masternode.
- **Attestation signatures are verified** — only the holder of the verifier key can submit attestations; per-verifier rate limiting prevents spam.
- **Reward outputs are verifiable and spendable** — the winner's one-time output key is derived from a deterministic coinbase tx key and the registered payout address; every node recomputes it, and the operator's wallet detects it like any normal output.
- **Sibling / alternative blocks are validated against the right state** — masternode state is replayed to the block's parent whenever the parent is not the current main-chain tip.
- **ChainLock/InstantSend messages are verified before storage or relay** — registered signing key, quorum membership, distinct voters, height window, wire-level size caps; only new valid votes are relayed.

### Current Limitations (planned improvements)

| Limitation | Impact | Planned fix |
|-----------|--------|-------------|
| **Attestation is not a security boundary while the verifier allowlist is off** | Any 24 keys can attest; the Activate gate and reward gate only prove that someone paid 24 tx fees | Enable `MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST` with a curated set, or require verifiers to be other Active masternodes |
| **Penalize has no governance key** | It is self-signed like every other lifecycle tx, so it cannot be used to sanction misbehaving operators | Define a governance key set (or drop the type) before mainnet |
| **No wallet `mn_update_endpoint` command** | Endpoint update operations require external tooling to build transactions | Add wallet command |
| **No wallet-side collateral protection after registration** | `transfer` / `send_all` / `optimize` may pick the collateral; the daemon rejects the tx (`WRONG_FEE`) but the failure message is opaque | Persist collateral key images in the wallet and exclude them from spendable inputs |
| **Masternode set hash not committed in block header** | Set hash is advisory-only; not consensus-enforced per block | Consider adding to coinbase extra or block header in future fork |
| **ChainLock is tip-only and advisory on reorgs** | Enforced only when extending the main tip; a heavier alt chain can still switch past a locked height; locks are not re-requested on startup; a node more than one block behind cannot validate votes for the network tip | Decide on full finality (enforce on reorg + historical lock sync) vs. advisory, and implement accordingly |
| **Quorum membership uses the local tip's active set** | Nodes whose tip differs (momentarily) may compute a different quorum and drop valid votes around activations and IS cycle boundaries | Pin the active set to a slightly older height (e.g. `top - 6`) for quorum selection |
| **Alt-chain tracker rebuild is O(blocks since fork)** | Every alternative / sibling block replays masternode state from the feature fork (after PoW is verified); with `--prune` below the fork height the replay cannot read pruned blocks | Keep per-segment tracker checkpoints; refuse `--prune` below the fork or persist periodic checkpoints |
| **Testnet profile cannot reach the fork heights** | `--network testnet` shares the mainnet fork heights / checkpoints; a fresh testnet never activates masternodes without a rebuild | Add per-network fork heights + checkpoint set to `NetworkParameters` |
| **No dedicated P2P gossip for MN heartbeat/attest messages** | Heartbeats and attestations use normal tx pool propagation | Acceptable for current design; revisit at scale |
| **Heartbeat block space scaling** | With `MASTERNODE_HEARTBEAT_MIN_BLOCK_INTERVAL=5`, 4,000 zero-input masternodes consume ~92 KB per block (saturation) | Monitor as masternode count grows; raise interval further if needed |

---

## 23. Configuration Reference

Edit `src/config/CryptoNoteConfig.h` to change any parameter. Requires recompilation.

```cpp
// Fork activation heights (both also listed in FORK_HEIGHTS[] as indices 22 / 23,
// SOFTWARE_SUPPORTED_FORK_INDEX = 23; static_asserts keep them in sync)
const uint64_t MASTERNODE_FEATURE_FORK_HEIGHT = 5000000;
const uint64_t MASTERNODE_REWARD_FORK_HEIGHT  = 5200000;

// Unlock-time V3 (min unlock 15 -> 3 blocks) activates with the reward fork
const uint64_t UNLOCK_TIME_HEIGHT_V3 = MASTERNODE_REWARD_FORK_HEIGHT;

// Signed payload anti-replay window
const uint64_t MASTERNODE_SIGNED_PAYLOAD_MAX_AGE_BLOCKS = 120;
const uint64_t MASTERNODE_SIGNED_PAYLOAD_FUTURE_TOLERANCE_BLOCKS = 2;

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

// Endpoint update cooldown — at most once per 7-day window
const uint64_t MASTERNODE_ENDPOINT_UPDATE_COOLDOWN_BLOCKS = MASTERNODE_HEALTH_WINDOW_BLOCKS;

// Attestation
const bool     MASTERNODE_REQUIRE_EXTERNAL_ATTESTATION                    = true;
const uint64_t MASTERNODE_ATTESTATION_WINDOW_BLOCKS                       = MASTERNODE_HEALTH_WINDOW_BLOCKS;
const uint64_t MASTERNODE_MIN_ATTESTATIONS_IN_WINDOW                      = 24;
const uint64_t MASTERNODE_MIN_ATTESTATION_HEALTH_PERCENT                  = 80;
const uint64_t MASTERNODE_ATTESTATION_MIN_BLOCK_INTERVAL_PER_VERIFIER     = 60;

// Verifier allowlist (set true + populate list to enforce)
const bool MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST = false;
const std::vector<std::string> MASTERNODE_VERIFIER_PUBKEY_ALLOWLIST = {};

// Registration authority allowlist (set true + populate list to enforce)
const bool MASTERNODE_ENFORCE_REGISTRATION_AUTHORITY = false;
const std::vector<std::string> MASTERNODE_REGISTRATION_AUTHORITY_PUBKEYS = {};

// ChainLock — block finality via masternode quorum signatures
const uint64_t CHAINLOCK_QUORUM_SIZE = 20;   // masternodes selected per block
const uint64_t CHAINLOCK_THRESHOLD   = 12;   // minimum votes (60%) to form a ChainLock
const uint64_t CHAINLOCK_VOTE_FUTURE_WINDOW_BLOCKS = 1;
const uint64_t CHAINLOCK_VOTE_MAX_AGE_BLOCKS = 60;
const uint64_t CHAINLOCK_MAX_PENDING_VOTES_PER_HEIGHT = CHAINLOCK_QUORUM_SIZE * 2;
const uint64_t CHAINLOCK_PENDING_LOCK_EXPIRY_SECONDS = 60 * 60;
const uint64_t CHAINLOCK_RETENTION_BLOCKS = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;
const uint64_t MASTERNODE_P2P_MAX_VOTES_PER_MESSAGE = 64;
const uint64_t MASTERNODE_P2P_MAX_KEY_IMAGES_PER_MESSAGE = 64;

// InstantSend — transaction finality via masternode quorum signatures
const uint64_t INSTANTSEND_QUORUM_SIZE       = 10;  // masternodes selected per tx
const uint64_t INSTANTSEND_THRESHOLD         = 6;   // minimum votes (60%) to form an IS lock
const uint64_t INSTANTSEND_LOCK_EXPIRY_BLOCKS = 60; // blocks before unconfirmed IS lock expires
const uint64_t INSTANTSEND_MAX_INPUTS        = 10;  // max inputs to qualify for InstantSend
const uint64_t INSTANTSEND_QUORUM_CYCLE_BLOCKS = 60; // IS quorum rotation, seeded by cycle-start block hash
```

---

---

## 24. Frequently Asked Questions

### Collateral & Funds

**Q: How does the network know I haven't spent my collateral?**

At registration, your collateral UTXO's **key image** is committed on-chain. Key images are the cryptographic fingerprints of CryptoNote coin spends — spending any UTXO reveals its unique key image in the transaction input. The consensus layer tracks all registered collateral key images in `MasternodeStateTracker`. Every transaction in every block is checked: if any input key image matches a spend-locked collateral key image, the transaction is rejected and never confirmed. The private key for the collateral UTXO is never at risk — the network simply blocks any spending attempt until the lock expires.

---

**Q: I shut down my node without de-registering. Can I recover my collateral?**

Yes — your funds are never seized or destroyed. What happens depends on how the node exits the Active state:

| Scenario | What happens to collateral |
|---|---|
| Node goes offline, health degrades to 0% | Still locked — no spend-lock timer starts until a Deactivate/Revoke tx appears on-chain |
| Registered but never activated | Still locked — submit `mn_revoke` to start the spend-lock timer |
| A Deactivate or Revoke tx is submitted | Spend-lock timer starts at that block height |
| 30,240 blocks (~21 days) pass after Deactivate/Revoke | Collateral is fully unlocked and freely spendable |

If no Deactivate/Revoke tx is ever submitted, the spend-lock timer never starts, and the collateral stays locked indefinitely (the node sits in a degraded state receiving no rewards). To free your collateral, use the `mn_deactivate` (Active only) or `mn_revoke` (any non-revoked state) wallet command in `zedwallet++`.

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

Note: **ChainLock and InstantSend quorum participation is not affected by health** — a masternode with 0% heartbeat health can still be selected into quorums and vote, as long as it is in Active status and has a valid signing key.

---

**Q: Do I need to send a heartbeat every block?**

No — and you cannot. The minimum interval is 5 blocks. Heartbeats submitted too soon are rejected. For maximum health you should send exactly one heartbeat every 5 blocks.

---

**Q: Can I pre-sign heartbeats and replay them instead of running the daemon?**

No. Every heartbeat embeds the height it was created at and consensus requires each accepted heartbeat to be newer than the previous one and at most 120 blocks old (at most 2 blocks in the future). A payload can therefore only be used once, within a short window — a live signer is required.

---

### Registration & Lifecycle

**Q: Can I change my endpoint (IP address) after registration?**

Yes — use the **UpdateEndpoint** transaction (type `0x08`). It replaces the stored endpoint commitment in-place without revoking or touching the collateral.

Rules:
- Allowed in **Active** or **Registered** status (not Inactive/Penalized/Revoked).
- The new endpoint commitment must not already be in use by another active or pending masternode.
- **Cooldown:** at most once every `MASTERNODE_ENDPOINT_UPDATE_COOLDOWN_BLOCKS` (7 days / 10,080 blocks). Attempts within the cooldown window are rejected.
- Signed with the payout key (same key used for heartbeats).

The update takes effect as soon as the UpdateEndpoint tx is confirmed in a block. The old commitment is immediately released; the new one is uniqueness-checked against all other active registrations.

---

**Q: Can two masternodes share the same endpoint commitment?**

No. The Register tx validation calls `hasEndpointCommitment`, which rejects any registration whose commitment hash already appears in an active or pending masternode. This prevents a single server from collecting multiple masternode rewards.

---

**Q: What is the difference between Deactivate and Revoke?**

| | Deactivate | Revoke |
|---|---|---|
| Allowed from | Active | Registered, Active, Inactive, Penalized |
| Resulting status | Inactive | Revoked |
| Spend-lock | Yes — 21 days | Yes — 21 days |
| Re-activate allowed? | Yes — `mn_activate` again (attestation threshold applies) | No — register again (same collateral only after the spend-lock) |
| Typical use | Temporary removal / maintenance | Permanent exit |

---

**Q: What happens to rewards while the masternode is in Registered status (before Activate)?**

Nothing — a masternode in Registered status is not eligible for rewards. Only Active masternodes are considered for reward selection. The registration establishes the collateral commitment and lets verifiers start attesting, but rewards require an explicit Activate transaction (which itself requires the attestation threshold).

---

### Rewards

**Q: What if no masternode is eligible for a reward at a given block?**

If no active masternode meets both the health threshold (≥ 95% heartbeat health) and the attestation threshold (≥ 80% attestation health with ≥ 24 samples), the full block reward goes to the PoW miner. No reward is held in reserve or rolled over.

---

**Q: Is the reward winner selection random or deterministic?**

Fully **deterministic** — no randomness is involved. The winner is selected by finding the active eligible masternode with the lowest total reward received in the last 7-day fairness window. Ties are broken first in favour of nodes that were never paid, then by the oldest `last_paid_height`, then by lexicographic comparison of masternode ID bytes. All nodes independently compute the same winner from the same chain state, ensuring consensus.

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

Yes — `MASTERNODE_ATTESTATION_ENFORCE_VERIFIER_ALLOWLIST` is currently `false`, meaning any verifier key is accepted. You can use the `mn_attest <mn_id> <0|1>` wallet command in `zedwallet++` to submit attestations. Each verifier is rate-limited to one accepted attestation per 60 blocks per masternode to prevent spam. If the allowlist is later enabled (`true`), only pre-approved verifier public keys will be accepted.

---

### ChainLock & InstantSend

**Q: Do I need to do anything to participate in ChainLock and InstantSend quorums?**

Yes — you need to pass your signing private key (generated during `mn_register`) to the daemon via `--mn-signing-key=<hex>`. Without this flag, your node will validate and relay ChainLock / InstantSend data from other nodes, but will not actively vote in quorums.

---

**Q: Does my masternode need to be healthy to participate in ChainLock/InstantSend?**

No. Quorum membership is determined by whether the masternode is **Active** and has a registered signing key on-chain. Health and attestation status only affect reward eligibility, not quorum selection.

---

**Q: How is the quorum for a block or transaction determined?**

For each block height *H*, with *P* = hash of block *H−1*:
```
sort_key(mnId) = H(mnId || P)
ChainLock quorum = first 20 active masternodes by sort_key
```

For transactions, with *C* = hash of the block at the start of the current 60-block cycle:
```
sort_key(mnId) = H(mnId || C)
InstantSend quorum = first 10 active masternodes by sort_key
```

Each height (and each 60-block IS cycle) gets an independently shuffled quorum seeded by data nobody can pick
without mining a block, so neither a forger nor a transaction sender can grind their way into a quorum.

---

**Q: What happens if a ChainLock conflicts with a reorg?**

Nodes that have received a valid ChainLock for height *H* will reject any alternative block at height *H* with a different block hash. This effectively halts any reorg attempt at or below the locked height. A reorg that tries to reorganise past a ChainLock would require either 60% of the quorum masternodes to be compromised or a protocol-level exception.

---

*This document is a work in progress. Parameters and fork heights are subject to change before mainnet activation.*
