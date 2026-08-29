/// Coin constants matching CryptoNoteConfig.h
const int kCoinDecimalPlaces = 2;
const String kCoinTicker = 'WRKZ';
const String kCoinName = 'WrkzCoin';

/// Default daemon node shown in Create/Open/Import forms.
/// Must use HTTPS — browsers block mixed content (HTTP from HTTPS pages).
/// Alternative: use same-origin nginx proxy (e.g. 'web-wallet.wrkz.work/daemon')
/// with the proxy forwarding to the real node.
const String kDefaultDaemonHost = 'node-fin.wrkz.work';
const int kDefaultDaemonPort = 443;
const bool kDefaultDaemonSSL = true;

/// Fallback poll intervals.
///
/// The wallet pushes `synced` / `transaction` events through the WASM bridge,
/// and the notifiers refresh on those. These timers exist only as a safety net
/// for anything the event stream misses, so they are deliberately slack — the
/// old 5/10/15-second cadence refetched the entire transaction history four
/// times a minute whether or not anything had changed.
const Duration kStatusPollInterval = Duration(seconds: 10);
const Duration kBalancePollInterval = Duration(seconds: 30);
const Duration kTransactionPollInterval = Duration(seconds: 60);

/// Poll interval used while the wallet is still syncing, when block height and
/// balance genuinely do change continuously.
const Duration kSyncingStatusPollInterval = Duration(seconds: 3);

/// Idle minutes before the wallet locks itself. 0 disables auto-lock.
const int kDefaultAutoLockMinutes = 15;

/// How often the wallet is flushed to browser storage once it is open.
const Duration kAutosaveInterval = Duration(minutes: 5);

/// Flush interval while the initial sync is still running, so closing the tab
/// mid-sync does not throw away hours of scanning.
const Duration kSyncingAutosaveInterval = Duration(minutes: 1);

/// Block-explorer transaction URL template; `{hash}` is substituted.
const String kExplorerTxUrl = 'https://explorer.wrkz.work/?hash={hash}#blockchain_transaction';

// ── Fees ─────────────────────────────────────────────────────────────────────
//
// Mirrors CryptoNoteConfig.h. Paying at least TRANSACTION_POW_PASS_WITH_FEE
// exempts a transaction from the per-transaction proof of work. That matters a
// lot here: in a single-threaded WASM build the PoW runs on the worker thread
// and can take minutes, so the wallet defaults to the exempt fee. It is now a
// visible, explained choice rather than a hardcoded constant in the send path.

/// Fee (atomic units) at or above which the transaction PoW is skipped.
/// 10000 atomic = 100.00 WRKZ.
const int kPowExemptFee = 10000;

/// How the fee is chosen for a transfer.
enum FeeMode {
  /// Pay [kPowExemptFee] so the transaction is exempt from proof of work.
  /// Costs more, completes in seconds.
  fast,

  /// Omit an explicit fee and let the wallet library charge the network
  /// minimum. Cheaper, but the browser must compute the transaction PoW, which
  /// can take several minutes in a single-threaded WASM build.
  economy,
}

// -- Passwords ---------------------------------------------------------------

/// Minimum wallet password length.
///
/// The create form previously accepted anything the two fields agreed on,
/// including an empty string — which leaves the wallet file in browser storage
/// effectively unencrypted.
const int kMinPasswordLength = 8;

/// Words in a CryptoNote mnemonic seed (24 words plus a checksum word).
const int kMnemonicWordCount = 25;
