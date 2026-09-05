/// Coin constants matching CryptoNoteConfig.h
const int kCoinDecimalPlaces = 2;
const String kCoinTicker = 'WRKZ';
const String kCoinName = 'WrkzCoin';

/// Address shape. 98 = standard, 120 = integrated (short payment ID),
/// 186 = integrated (full payment ID).
const String kAddressPrefix = 'Wrkz';
const Set<int> kValidAddressLengths = {98, 120, 186};

/// Minimum wallet password length enforced on create / change.
const int kMinPasswordLength = 8;

/// Default daemon node shown in Create/Open/Import forms.
const String kDefaultDaemonHost = 'nodes.wrkz.work';
const int kDefaultDaemonPort = 17856;
const bool kDefaultDaemonSSL = false;

/// Public transaction PoW server prefilled in Settings. Off by default; the
/// wallet computes the proof of work on this machine until the user enables
/// it. See TXPOWSERVER.md.
const String kDefaultTxPowServerHost = 'txpow.wrkz.work';
const int kDefaultTxPowServerPort = 443;
const bool kDefaultTxPowServerSSL = true;

/// How often to poll the wallet via FFI for live updates.
const Duration kStatusPollInterval = Duration(seconds: 5);
const Duration kBalancePollInterval = Duration(seconds: 10);
const Duration kTransactionPollInterval = Duration(seconds: 15);
