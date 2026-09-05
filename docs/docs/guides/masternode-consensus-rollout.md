# Masternode Consensus Rollout (Draft Spec)

> **Superseded.** This early planning note predates the implementation. Constant names, the 4-week gap,
> the JSON-RPC method list and the `eligible` state described below were **not** implemented as written.
> The authoritative, implementation-accurate specification is `DRAFT_MASTERNODE.md` at the repository root
> (fork heights 4,800,000 / 5,000,000, payload formats, lifecycle rules, RPC reference). This page is kept
> for historical context only.

This document defines the staged masternode rollout plan for WrkzCoin.

## Scope

- Keep PoW algorithm unchanged.
- Add masternode lifecycle, liveness proofs, and eligibility scoring.
- Activate rewards in a second fork after a 4-week soak period.
- Preserve endpoint privacy: no public on-chain or public RPC exposure of raw `ip:port`.

## Fork Plan

### Fork A: Masternode Feature Activation

At `MN_FEATURE_FORK_HEIGHT`:

- Activate masternode transactions:
  - `mn_register`
  - `mn_update`
  - `mn_revoke`
  - `mn_heartbeat`
  - `mn_attestation`
- Activate consensus validation for:
  - strict collateral lock
  - registration bond lock and slash policy
  - one endpoint per masternode identity
  - endpoint uniqueness across active set
  - deterministic eligibility and scoring
- Reward policy remains unchanged:
  - `100% PoW miner reward`

### Fork B: Reward Activation

At `MN_REWARD_FORK_HEIGHT`:

- Keep all Fork A rules.
- Enable reward split:
  - if eligible masternode set is non-empty at height `H`: `70% masternode / 30% PoW`
  - if eligible set is empty at height `H`: `100% PoW`

### Four-Week Gap

- `DIFFICULTY_TARGET = 60` seconds
- Expected blocks per day: `1440`
- 4 weeks = `28 * 1440 = 40320` blocks
- Rule: `MN_REWARD_FORK_HEIGHT = MN_FEATURE_FORK_HEIGHT + 40320`

## Deterministic Consensus Rules

### Eligibility Cutoff

- Eligibility for block `H` is computed from finalized chain state at `H - 1`.
- New heartbeats/attestations in block `H` only affect eligibility starting at `H + 1`.
- Consensus logic must use block-height windows, not local wall-clock time.
- A masternode is reward-eligible only if health over trailing 7 days meets threshold:
  - health window = `10080` blocks
  - minimum healthy ratio = `MN_MIN_HEALTH_PERCENT`

### Reorg Safety

- Masternode state is replayed from the common ancestor on reorg.
- Reward eligibility and payout winner must be recomputed deterministically after replay.

### Endpoint and Identity Rules

- Each active masternode has exactly one active endpoint commitment.
- One endpoint commitment cannot be shared by multiple active masternodes.
- Endpoint changes require signed `mn_update` and cooldown.
- Duplicate endpoint conflicts use deterministic tie-break:
  - existing active record keeps priority
  - conflicting newer record is invalid

## Privacy Rules

- Raw endpoint (`ip:port`) must not be written on-chain.
- Raw endpoint must not be returned by public RPC.
- Public fields may include only non-sensitive identity and status data.
- Reachability may be checked by verifier quorum via private transport.
- Consensus accepts signed attestations, not raw endpoint disclosure.

## Masternode Lifecycle

States:

- `registered`
- `eligible`
- `active`
- `inactive`
- `penalized`
- `revoked`

Transitions (summary):

- `mn_register` -> `registered` (after collateral/bond checks)
- Sufficient valid liveness evidence -> `eligible` / `active`
- Missing heartbeat or failed attestations -> `inactive` / `penalized`
- `mn_revoke` or collateral invalidation -> `revoked`

Spend lock rule:

- On deactivation/revocation, collateral remains unspendable for 21 days.
- With 60-second block target, lock = `30240` blocks.

## Economic Rules

- Anti-Sybil baseline:
  - strict collateral lock
  - registration bond
  - penalties/slashing for repeated invalid liveness behavior
- Reward fallback safety:
  - if no eligible masternode exists, network always remains live with `100% PoW`

### Reward Fairness Scheduler

- Per-block payout is to one masternode, not all active masternodes.
- Winner selection is deterministic and fairness-equalizing over time:
  - build eligible set at `H - 1`
  - select candidate with lowest cumulative masternode rewards in fairness window
  - deterministic tie-break by oldest `last_paid_height`, then lowest `mn_id`
- This prevents one node from repeatedly receiving rewards and converges toward equal payout across healthy active nodes.

## Initial Constants (Proposed)

These values are placeholders for testnet tuning:

- `MN_HEARTBEAT_INTERVAL_BLOCKS = 60`
- `MN_MISSED_HEARTBEAT_LIMIT = 6`
- `MN_ENDPOINT_UPDATE_COOLDOWN_BLOCKS = 720`
- `MN_MIN_CONFIRMATIONS_COLLATERAL = 720`
- `MN_BOND_LOCK_BLOCKS = 20160`
- `MN_DEACTIVATION_SPEND_LOCK_BLOCKS = 30240`
- `MN_ELIGIBILITY_GRACE_BLOCKS = 120`
- `MN_HEALTH_WINDOW_BLOCKS = 10080`
- `MN_MIN_HEALTH_PERCENT = 95`
- `MN_MIN_ATTESTATIONS = 3`
- `MN_ATTESTATION_WINDOW_BLOCKS = 30`
- `MN_FAIRNESS_WINDOW_BLOCKS = 10080`

## RPC and CLI (Fork A Scope)

Add public-safe APIs:

- `get_masternode_count`
- `get_masternodes` (paginated)
- `get_masternode` (`mn_id`)
- `GET /masternodes/count`
- `GET /masternodes?limit=<n>&offset=<n>`

Public-safe fields:

- `mn_id`
- `state`
- `score`
- `registration_height`
- `last_heartbeat_height`
- `last_attested_height`
- `last_paid_height`
- `collateral_amount`
- `bond_amount`
- `protocol_version`

Consensus observability fields (recommended for pools and infra):

- `masternode_eligible_count`
- `masternode_set_hash`
- `masternode_reward_winner` (empty string if none)

Not public:

- raw `ip:port`
- any direct pubkey-to-endpoint mapping

Mining safety rule:

- `getblocktemplate` should only be served when daemon is synced.
- Pool callers should pass `expected_masternode_set_hash` in `getblocktemplate` params and reject work if mismatch is returned.

## Risk Register and Mitigations

1. Consensus divergence at boundary heights
- Mitigation: strict `H - 1` cutoff and deterministic replay.

2. Endpoint privacy leakage
- Mitigation: no endpoint in chain or public RPC; redaction tests for logs/metrics.

3. Verifier false negatives/collusion
- Mitigation: rotating verifier set, threshold attestations, penalties for bad behavior.

4. Reward mode flapping (70/30 vs 100/0)
- Mitigation: grace windows and missed-heartbeat tolerance.

5. Centralization pressure from 70% MN rewards
- Mitigation: collateral/bond calibration via testnet participation data before mainnet finalization.

6. Reward capture by a small subset of nodes
- Mitigation: deterministic fairness scheduler with cumulative-reward tracking and strict tie-break rules.

## Rollout Checklist

1. Finalize `MN_FEATURE_FORK_HEIGHT`.
2. Set `MN_REWARD_FORK_HEIGHT = MN_FEATURE_FORK_HEIGHT + 40320`.
3. Ship upgrade with both heights announced.
4. Run testnet through both fork points.
5. Verify:
   - no consensus splits under reorg tests
   - no endpoint leaks in RPC/log output
   - stable eligibility and fallback behavior
6. Activate on mainnet.

## Formal Masternode Payload (Consensus)

Masternode transactions are carried in tx extra arbitrary-data payload:

- Magic: `MN01` (4 bytes)
- Type: 1 byte
- Masternode ID: 32 bytes (`Crypto::Hash`)
- Optional fields by type

Type map:

- `1` register: includes payout key (`Crypto::PublicKey`, 32 bytes)
- `2` activate
- `3` deactivate
- `4` penalize
- `5` revoke
- `6` heartbeat: includes health flag (`0` or `1`)
