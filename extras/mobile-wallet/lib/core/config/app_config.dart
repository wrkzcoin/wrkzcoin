/// App-wide constants, coin configuration, node presets, and general settings.
class AppConfig {
  AppConfig._();

  // ── coin ───────────────────────────────────────────────────────────────────
  static const String ticker = 'WRKZ';
  static const String coinName = 'WrkzCoin';
  static const int decimalPlaces = 2;
  static const int atomicDivisor = 100; // 10^decimalPlaces
  static const String addressPrefix = 'Wrkz';

  /// 98 = standard, 120 = integrated (short payment ID),
  /// 186 = integrated (full payment ID).
  static const Set<int> validAddressLengths = {98, 120, 186};

  /// Minimum wallet password length enforced on create / change.
  static const int minPasswordLength = 8;

  // ── node presets ───────────────────────────────────────────────────────────
  static const List<NodePreset> nodePresets = [
    NodePreset(
      label: 'WRKZ Primary',
      host: 'nodes.wrkz.work',
      port: 17856,
      ssl: false,
    ),
    NodePreset(
      label: 'WRKZ Finland',
      host: 'node-fin.wrkz.work',
      port: 17856,
      ssl: false,
    ),
  ];

  static const String defaultDaemonHost = 'nodes.wrkz.work';
  static const int defaultDaemonPort = 17856;
  static const bool defaultDaemonSsl = false;

  // ── polling intervals ──────────────────────────────────────────────────────
  static const Duration statusPollInterval = Duration(seconds: 5);
  static const Duration balancePollInterval = Duration(seconds: 10);
  static const Duration transactionsPollInterval = Duration(seconds: 15);

  // ── autosave ───────────────────────────────────────────────────────────────
  static const Duration autosaveInterval = Duration(minutes: 5);

  // ── auto-lock timeouts ─────────────────────────────────────────────────────
  static const List<AutoLockOption> autoLockOptions = [
    AutoLockOption(label: 'Immediately', duration: Duration.zero),
    AutoLockOption(label: '1 minute', duration: Duration(minutes: 1)),
    AutoLockOption(label: '5 minutes', duration: Duration(minutes: 5)),
    AutoLockOption(label: 'Never', duration: null),
  ];
  static const int defaultAutoLockIndex = 0; // Immediately

  // ── wallet file storage ────────────────────────────────────────────────────
  static const String walletsSubdir = 'wallets';
  static const String registryFilename = 'wallets.json';
  static const String walletFileExtension = '.wallet';

  // ── UI ─────────────────────────────────────────────────────────────────────
  static const int recentTxCount = 5;
  static const int historyPageSize = 25;

  // ── secure storage keys ────────────────────────────────────────────────────
  static const String skThemeMode = 'pref_theme_mode';
  static const String skNotificationsEnabled = 'pref_notifications';
  static const String skAutosaveEnabled = 'pref_autosave';
  static const String skAutoLockIndex = 'pref_auto_lock_index';
  static const String skBiometricEnabled = 'pref_biometric';
  static const String skSeedBackupConfirmed = 'pref_seed_backup_confirmed';
  static const String skLogLevel = 'pref_log_level';
  static const String skScanCoinbase = 'pref_scan_coinbase';
  static const String skLocale = 'pref_locale';
  static const String skFirstLaunchDone = 'pref_first_launch_done';
  // Per-wallet password key: "wallet_pw_<filename>"
  static String walletPasswordKey(String filename) => 'wallet_pw_$filename';
}

class NodePreset {
  final String label;
  final String host;
  final int port;
  final bool ssl;

  const NodePreset({
    required this.label,
    required this.host,
    required this.port,
    required this.ssl,
  });
}

class AutoLockOption {
  final String label;

  /// null means "Never" (auto-lock disabled).
  final Duration? duration;

  const AutoLockOption({required this.label, required this.duration});
}
