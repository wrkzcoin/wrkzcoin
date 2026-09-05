# Using Wallet CLI

Implementation references:

- CLI args: `src/zedwallet++/ParseArguments.cpp`
- Command catalog: `src/zedwallet++/Commands.cpp`

This project's interactive wallet CLI binary is `wrkz-wallet`.

## Start the CLI

Show help:

```bash
./wrkz-wallet --help
```

Open an existing wallet from command line:

```bash
./wrkz-wallet \
  --wallet-file wallet.wallet \
  --password "wallet-pass" \
  --remote-daemon 127.0.0.1:17856 \
  --threads 4
```

Useful startup options:

- `--wallet-file <file>`
- `--password <pass>`
- `--remote-daemon <host:port>`
- `--threads <n>`
- `--scan-coinbase-transactions`
- `--log-level <n>`
- `--log-file <file>`
- `--ssl` (when SSL support is compiled)

`--remote-daemon` also accepts an IPv6 address in brackets
(`[2001:db8::1]:17856`) and, on POSIX, a local IPC socket — an absolute path, an
`@name` in the abstract namespace, or an `ipc://path`. See
[Local IPC and Console](ipc-and-console.md).

## Startup menu actions

When no wallet is preselected, common startup choices include:

- `open`
- `create`
- `seed_restore`
- `key_restore`
- `view_wallet`
- `exit`

## Common interactive commands

General/help:

- `help`
- `advanced`
- `exit`

Status and sync:

- `status`
- `refresh`
- `swap_node`

Wallet info:

- `address`
- `balance`

Transactions:

- `transfer`
- `send_all`
- `sweep`
- `sweep_all`
- `incoming_transfers`
- `outgoing_transfers`
- `list_transfers`
- `txs`
- `txs_full`
- `get_tx_private_key`
- `check_tx <hash>`
- `decode_integrated`

Address and payment tools:

- `make_integrated_address`

Address book:

- `ab_add`
- `ab_delete`
- `ab_list`
- `ab_send`

Security and maintenance:

- `backup`
- `change_password`
- `save`
- `save_csv`
- `reset`
- `set_log_level`

## Example session flow

1. Start:

```bash
./wrkz-wallet --remote-daemon 127.0.0.1:17856
```

2. Create or open wallet in startup menu:

- `create` or `open`

3. Check readiness:

- `status`
- `balance`
- `address`

4. Send funds:

- `transfer`

5. Confirm activity:

- `txs`
- `incoming_transfers`
- `outgoing_transfers`

6. Save and exit:

- `save`
- `exit`
