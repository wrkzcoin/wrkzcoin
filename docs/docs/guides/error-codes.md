# Wallet Error Codes

Every wallet surface built on `walletbackend` reports failures with the same
numeric code: `wrkz-wallet-api` returns it as `errorCode` in the response body,
`wrkz-service` as the JSON-RPC error code, `wrkz-wallet` prints the message, and
the C API returns it from `wallet_*` calls (`wallet_error_code_to_string` maps
it back to the text below).

Defined in `src/errors/Errors.h`, with the messages in `src/errors/Errors.cpp`.
Codes are stable: a value is never reused for a different meaning.

!!! note "These are not HTTP status codes"
    A wallet-api call that fails validation answers HTTP `400` with an
    `errorCode` from this table in the body. A sweep answers HTTP `200` with a
    per-transaction `errorCode` for each attempt that failed. Read the body, not
    just the status line.

| Code | Name | Meaning |
| --- | --- | --- |
| `0` | `SUCCESS` | The operation completed successfully. |
| `1` | `FILENAME_NON_EXISTENT` | The filename you are attempting to open does not exist, or the wallet does not have permission to open it. |
| `2` | `INVALID_WALLET_FILENAME` | We could not open/save to the filename given. Possibly invalid characters, or permission issues. |
| `3` | `NOT_A_WALLET_FILE` | This file is not a wallet file, or is not a wallet file type supported by this wallet version. |
| `4` | `WALLET_FILE_CORRUPTED` | This wallet file appears to have gotten corrupted. |
| `5` | `WRONG_PASSWORD` | The password given for this wallet is incorrect. |
| `6` | `UNSUPPORTED_WALLET_FILE_FORMAT_VERSION` | This wallet file appears to be from a newer or older version of the software, that we do not support. |
| `7` | `INVALID_MNEMONIC` | The mnemonic seed given is invalid. |
| `8` | `WALLET_FILE_ALREADY_EXISTS` | The wallet file you are attempting to create already exists. Please delete it first. |
| `9` | `WILL_OVERFLOW` | This operation will cause integer overflow. Please decrease the amounts you are sending. |
| `10` | `ADDRESS_NOT_IN_WALLET` | The address given does not exist in the wallet container, but is required to exist for this operation. |
| `11` | `NOT_ENOUGH_BALANCE` | Not enough unlocked funds were found to cover this transaction in the subwallets specified (or all wallets, if not specified). (Sum of amounts + fee) |
| `12` | `ADDRESS_WRONG_LENGTH` | The address given is too short or too long. |
| `13` | `ADDRESS_WRONG_PREFIX` | The address does not have the correct prefix corresponding to this coin - it appears to be an address for another cryptocurrency. |
| `14` | `ADDRESS_NOT_BASE58` | The address contains invalid characters, that are not in the base58 set. |
| `15` | `ADDRESS_NOT_VALID` | The address given is not valid. Possibly invalid checksum. Most likely a typo. |
| `16` | `INTEGRATED_ADDRESS_PAYMENT_ID_INVALID` | The payment ID stored in the integrated address supplied is not valid. |
| `17` | `FEE_TOO_SMALL` | The fee given for this transaction is below the minimum allowed network fee. |
| `18` | `NO_DESTINATIONS_GIVEN` | The destinations array (amounts/addresses) is empty. |
| `19` | `AMOUNT_IS_ZERO` | One of the destination parameters has an amount given of zero. |
| `20` | `FAILED_TO_CREATE_RING_SIGNATURE` | Failed to create ring signature - probably a programmer error, or a corrupted wallet. |
| `21` | `MIXIN_TOO_SMALL` | The mixin value given is too low to be accepted by the network (based on the current height known by the wallet) |
| `22` | `MIXIN_TOO_BIG` | The mixin value given is too high to be accepted by the network (based on the current height known by the wallet) |
| `23` | `PAYMENT_ID_WRONG_LENGTH` | The payment ID given is not 16 or 64 characters long. |
| `24` | `PAYMENT_ID_INVALID` | The payment ID given is not a hex string (A-F0-9) |
| `25` | `ADDRESS_IS_INTEGRATED` | The address given is an integrated address, but integrated addresses aren't valid for this parameter, for example, change address. |
| `26` | `CONFLICTING_PAYMENT_IDS` | Conflicting payment IDs were given. This could mean an integrated address + payment ID were given, where they are not the same, or that multiple integrated addresses with different payment IDs were given. |
| `27` | `CANT_GET_FAKE_OUTPUTS` | Failed to get fake outputs from the daemon to obscure our transaction, and mixin is not zero. |
| `28` | `NOT_ENOUGH_FAKE_OUTPUTS` | We could not get enough fake outputs for this transaction to complete. If possible, try lowering the mixin value used, or decrease the amount you are sending. |
| `29` | `INVALID_GENERATED_KEYIMAGE` | The key image we generated is invalid - probably a programmer error, or a corrupted wallet. |
| `30` | `DAEMON_OFFLINE` | We were not able to submit our request to the daemon. Ensure it is online and not frozen. |
| `31` | `DAEMON_ERROR` | An error occured whilst the daemon processed the request. Possibly our software is outdated, the daemon is faulty, or there is a programmer error. Check your daemon logs for more info. (set_log 4) |
| `32` | `TOO_MANY_INPUTS_TO_FIT_IN_BLOCK` | The transaction is too large (in BYTES, not AMOUNT) to fit in a block. Either decrease the amount you are sending, perform fusion transactions, or decrease mixin (if possible). |
| `33` | `MNEMONIC_INVALID_WORD` | The mnemonic seed given has a word that is not present in the english word list. |
| `34` | `MNEMONIC_WRONG_LENGTH` | The mnemonic seed given is the wrong length. |
| `35` | `MNEMONIC_INVALID_CHECKSUM` | The mnemonic seed given has an invalid checksum word. |
| `36` | `FULLY_OPTIMIZED` | Cannot send fusion transaction - wallet is already fully optimized. |
| `37` | `FUSION_MIXIN_TOO_LARGE` | Cannot send fusion transacton - mixin is too large to meet input/output ratio requirements whilst remaining in size constraints. |
| `38` | `SUBWALLET_ALREADY_EXISTS` | A subwallet with the given key already exists. |
| `39` | `ILLEGAL_VIEW_WALLET_OPERATION` | This function cannot be called when using a view wallet. |
| `40` | `ILLEGAL_NON_VIEW_WALLET_OPERATION` | This function can only be used when using a view wallet. |
| `41` | `KEYS_NOT_DETERMINISTIC` | You cannot get a mnemonic seed for this address, as the view key is derived in terms of the spend key. |
| `42` | `CANNOT_DELETE_PRIMARY_ADDRESS` | Each wallet has a primary address when created, this address cannot be removed. |
| `43` | `TX_PRIVATE_KEY_NOT_FOUND` | Couldn't find the private key for this transaction. The transaction must exist, and have been sent by this program. Transaction private keys cannot be found upon rescanning/reimporting. |
| `44` | `AMOUNTS_NOT_PRETTY` | The created transaction isn't comprised of only 'Pretty' amounts. This will cause the outputs to be unmixable. Almost certainly a programmer error. Cancelling transaction. |
| `45` | `UNEXPECTED_FEE` | The fee of the created transaction is not the same as that which was specified (0 for fusion transactions). Almost certainly a programmer error. Cancelling transaction. |
| `46` | `NEGATIVE_VALUE_GIVEN` | The input for this operation must be greater than or equal to zero, but a negative number was given. |
| `47` | `INVALID_KEY_FORMAT` | The public/private key or hash given is not a 64 char hex string. |
| `48` | `HASH_WRONG_LENGTH` | The hash given is not 64 characters long. |
| `49` | `HASH_INVALID` | The hash given is not a hex string (A-Za-z0-9) |
| `50` | `NON_INTEGER_GIVEN` | The number given is a float, not an integer. |
| `51` | `INVALID_PUBLIC_KEY` | The public key given is not a valid ed25519 public key. |
| `52` | `INVALID_PRIVATE_KEY` | The private key given is not a valid ed25519 private key. |
| `53` | `INVALID_EXTRA_DATA` | The extra data given for the transaction could not be decoded. |
| `54` | `UNKNOWN_ERROR` | An unknown error occurred. |
| `55` | `DAEMON_STILL_PROCESSING` | The transaction was sent to the daemon, but the connection timed out before we could determine if the transaction succeeded. Wait a few minutes before retrying the transaction, as it may still succeed. |
| `56` | `OUTPUT_DECOMPOSITION` | The transaction contains more outputs than what is permitted by the number of inputs that have been supplied for the transaction. Please try to send your transaction again. If the problem persists, please reduce the number of destinations that you are trying to send to. |
| `57` | `PREPARED_TRANSACTION_EXPIRED` | The prepared transaction contains inputs that have since been spent or are no longer available, probably due to sending another transaction in between preparing this transaction and sending it. The prepared transaction has been cancelled. |
| `58` | `PREPARED_TRANSACTION_NOT_FOUND` | The prepared transaction hash given does not exist, either because it never existed or because the wallet process was restarted and the previously prepared transactions were lost. Please re-prepare and re-send the transaction, ensuring you specify the correct transaction hash. |
| `59` | `AMOUNT_UGLY` | The amount given does not have only a single significant digit. For example, 20000 or 100000 would be fine, but 20001 or 123456 would not. |
| `60` | `UNLOCK_TIME_TOO_SMALL` | Unlock time is too small |
| `61` | `SHORT_PAYMENT_ID_NEEDS_SINGLE_DESTINATION` | A short payment ID is encrypted so that only the receiver can read it, which means the transaction can only have one destination. Send to a single address, or use a long (64 character) payment ID instead. |
| `62` | `LITE_NODE_CANNOT_RESCAN_THAT_LOW` | The daemon this wallet is connected to is a lite node and holds no block data below its lite height. Rescanning from lower than that would start at the lite height instead, and the transactions this wallet already holds from underneath would be lost with no way to find them again here. Connect a daemon that holds the whole chain, or rescan from the lite height. |

## Codes you should no longer see

| Code | Name | Why |
| --- | --- | --- |
| `36` | `FULLY_OPTIMIZED` | Fusion transactions were removed in 0.4.7. Nothing raises this any more |
| `37` | `FUSION_MIXIN_TOO_LARGE` | Same. The values are kept so old integrations parsing them do not break |

## The ones worth handling explicitly

| Code | Handling |
| --- | --- |
| `11` `NOT_ENOUGH_BALANCE` | Includes the fee. Check unlocked, not total, balance |
| `21` / `22` mixin bounds | The network ring size changes at fork heights - see [Network Parameters](network-parameters.md). The wallet retries at the best achievable ring rather than failing outright since 0.4.8 |
| `28` `NOT_ENOUGH_FAKE_OUTPUTS` | A denomination without enough decoys. The wallet falls back to a smaller ring; a caller pinning `mixin` explicitly can still hit this |
| `30` `DAEMON_OFFLINE` / `31` `DAEMON_ERROR` | Retry with backoff. `31` is worth logging with the daemon's own log |
| `55` `DAEMON_STILL_PROCESSING` | **Do not retry immediately.** The transaction may have succeeded. Wait, then check by hash |
| `57` / `58` prepared transactions | Prepared transactions live in memory and are lost on restart. Re-prepare rather than treating this as fatal |
| `61` `SHORT_PAYMENT_ID_NEEDS_SINGLE_DESTINATION` | Short payment IDs are encrypted to one receiver. Use a long payment ID for a multi-destination transaction - see [Encrypted Payment IDs](encrypted-payment-ids.md) |
| `62` `LITE_NODE_CANNOT_RESCAN_THAT_LOW` | The connected daemon is a [lite node](lite-node.md). Rescan from its lite height, or connect a full node |

