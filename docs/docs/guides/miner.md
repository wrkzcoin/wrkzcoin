# The Miner App

`miner` is the bundled reference CPU miner. It drives `getblocktemplate` and
`submitblock` on a daemon directly and needs no stratum port.

It is a **plain reference implementation with no assembly path**. For real
hashing throughput, run a stock miner against the daemon's stratum server
instead — see [Solo Mining](solo-mining.md).

```bash
miner --daemon-address 127.0.0.1:17856 --address <your Wrkz address> --threads 8
```

The address is where the block reward is paid.

## Options

Defined in `src/miner/MiningConfig.cpp`.

### Daemon

| Option | Default | Meaning |
| --- | --- | --- |
| `--daemon-address <host:port>` | | Daemon to mine against. **Overrides `--daemon-host` and `--daemon-rpc-port`.** An absolute path, an `@name` or an `ipc://path` connects over the daemon's [local IPC socket](ipc-and-console.md) instead |
| `--daemon-host <host>` | `127.0.0.1` | Daemon host |
| `--daemon-rpc-port <port>` | `17856` | Daemon RPC port |
| `--scan-time <s>` | `1` | How often to poll the chain for a new block to mine on |
| `--daemon-timeout <s>` | `10` | How long to wait on a daemon request before giving up |
| `--retry-interval <s>` | `1` | How long to wait before asking again after a failed request |

### Mining

| Option | Default | Meaning |
| --- | --- | --- |
| `--address <address>` | | The address the reward is paid to. Required unless benchmarking |
| `--threads <n>` | hardware concurrency | Mining threads. Going above what the hardware reports is allowed but warned about |
| `--limit <n>` | `0` | Mine this exact number of blocks, then stop. `0` means no limit |
| `--benchmark <s>` | `0` | Hash for this many seconds, report the rate, and exit. **Needs no daemon and no address** — it measures the proof of work alone |
| `--hash-rate-interval <s>` | `60` | How often to report the hash rate. `0` turns the report off |
| `--first-block-timestamp <t>` | `0` | Set the timestamp of the first mined block. `0` leaves it unchanged |
| `--block-timestamp-interval <n>` | `0` | Timestamp step for each subsequent block. Only valid together with `--first-block-timestamp` |

`--threads 0` is rejected. `--first-block-timestamp` and
`--block-timestamp-interval` exist for test networks, not for mining the real
chain.

!!! tip "The best thread count is set by cache, not by cores"
    CryptoNight's scratchpad is what limits throughput. More threads than your
    L3 cache can hold scratchpads for makes the hash rate go *down*.

## Benchmarking without a daemon

```bash
miner --benchmark 30 --threads 8
```

This measures the proof-of-work function in isolation, which is the right way
to compare a build (see the `-DARCH=native` performance mode in
[Building from Source](building.md#performance-local-benchmarking-only)) without
a node in the way.

## What changed in 0.4.8

- `--limit` no longer terminates the process or relaunches it; it stops cleanly
  after the requested blocks.
- Ctrl+C is handled.
- The reported hash rate is real — it was previously wrong.
- The submit reply is trusted rather than second-guessed.
- CryptoNight keeps its scratchpad for the life of the thread, which roughly
  doubled hash rate across every binary that hashes.

## `cryptotest`

`cryptotest` is a self-test, not a miner. Run with no arguments it hashes known
input with every algorithm in the tree and **aborts if any digest does not match
the expected value**, then runs the container, encoding, payment ID and wallet
crypto test suites. Worth doing after changing `ARCH`, the linker or the
toolchain — a build that mines wrong is otherwise silent until it produces
rejected blocks.

```bash
cryptotest                       # correctness
cryptotest --benchmark           # hash rates
cryptotest --benchmark -i 50000  # minimum 1,000 iterations
```
