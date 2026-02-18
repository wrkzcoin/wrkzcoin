# Wallet CLI Command Reference

Source: `src/zedwallet++/Commands.cpp`

## Startup menu commands

- `open`: Open a wallet already on your system
- `create`: Create a new wallet
- `seed_restore`: Restore a wallet using seed phrase
- `key_restore`: Restore a wallet using view/spend keys
- `view_wallet`: Import a view-only wallet
- `exit`: Exit the program

## Node-down menu commands

- `try_again`
- `continue`
- `swap_node`
- `exit`

## General

- `help`
- `advanced`
- `exit`

## Wallet and network info

- `status`
- `refresh`
- `swap_node`
- `address`
- `addr` (alias)
- `balance`
- `bal` (alias)

## Transactions

- `incoming_transfers`
- `in` (alias)
- `outgoing_transfers`
- `out` (alias)
- `list_transfers`
- `txs`
- `txs_full`
- `transfer`
- `ab_send`
- `send_all`
- `get_tx_private_key`
- `check_tx <hash>`
- `decode_integrated`

## Address book

- `ab_add`
- `ab_delete`
- `ab_list`

## Address and payment tools

- `make_integrated_address`

## Security and maintenance

- `backup`
- `change_password`
- `save`
- `save_csv`
- `optimize`
- `reset`
- `set_log_level`

## View-wallet support notes

Some commands are disabled in view-only wallets. If unavailable, use:

- `help` to list currently allowed commands for your wallet mode.
