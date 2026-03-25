/// Coin constants matching CryptoNoteConfig.h
const int kCoinDecimalPlaces = 2;
const String kCoinTicker = 'WRKZ';
const String kCoinName = 'WrkzCoin';

/// Default daemon node shown in Create/Open/Import forms.
const String kDefaultDaemonHost = 'nodes.wrkz.work';
const int kDefaultDaemonPort = 17856;
const bool kDefaultDaemonSSL = false;

/// How often to poll the wallet via FFI for live updates.
const Duration kStatusPollInterval = Duration(seconds: 5);
const Duration kBalancePollInterval = Duration(seconds: 10);
const Duration kTransactionPollInterval = Duration(seconds: 15);
