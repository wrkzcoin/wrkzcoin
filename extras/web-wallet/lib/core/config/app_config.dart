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

/// Public transaction PoW server prefilled in Settings. Off by default; until
/// the user enables it the web wallet pays the PoW bypass fee instead.
/// Must be HTTPS for the same mixed-content reason as the node.
const String kDefaultTxPowServerHost = 'txpow.wrkz.work';
const int kDefaultTxPowServerPort = 443;
const bool kDefaultTxPowServerSSL = true;

/// How often to poll the wallet via FFI for live updates.
const Duration kStatusPollInterval = Duration(seconds: 5);
const Duration kBalancePollInterval = Duration(seconds: 10);
const Duration kTransactionPollInterval = Duration(seconds: 15);
