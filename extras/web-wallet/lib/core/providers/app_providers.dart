import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:intl/intl.dart';
import '../config/app_config.dart';

const _storage = FlutterSecureStorage();
const _kThemeModeKey = 'pluton_theme_mode';
const _kLogLevelKey = 'pluton_log_level';
const _kNotificationsKey = 'pluton_notifications_enabled';
const _kAutosaveKey = 'pluton_autosave_enabled';
const _kScanCoinbaseKey = 'pluton_scan_coinbase';
const _kLocaleKey = 'pluton_locale';
const _kFirstLaunchDoneKey = 'pluton_first_launch_done';
const _kAutoLockKey = 'pluton_auto_lock_minutes';
const _kDefaultNodeHostKey = 'pluton_default_node_host';
const _kDefaultNodePortKey = 'pluton_default_node_port';
const _kDefaultNodeSSLKey = 'pluton_default_node_ssl';

typedef DefaultNode = ({String host, int port, bool ssl});

// ── Preferences preload ───────────────────────────────────────────────────────

/// Snapshot of every persisted preference, read once before the first frame.
///
/// Each notifier used to kick off an un-awaited `_load()` from `build()` and
/// return a default, so the app painted in the wrong theme and language and
/// then snapped to the stored values. On web that flash happens on every page
/// load. Reading them all up front makes the first frame the correct one.
class PreloadedPrefs {
  final ThemeMode themeMode;
  final WalletLogLevel logLevel;
  final bool notificationsEnabled;
  final bool autosaveEnabled;
  final bool scanCoinbase;
  final Locale? locale;
  final bool firstLaunchDone;
  final int autoLockMinutes;
  final DefaultNode defaultNode;

  const PreloadedPrefs({
    required this.themeMode,
    required this.logLevel,
    required this.notificationsEnabled,
    required this.autosaveEnabled,
    required this.scanCoinbase,
    required this.locale,
    required this.firstLaunchDone,
    required this.autoLockMinutes,
    required this.defaultNode,
  });

  static const fallback = PreloadedPrefs(
    themeMode: ThemeMode.system,
    logLevel: WalletLogLevel.info,
    notificationsEnabled: true,
    autosaveEnabled: true,
    scanCoinbase: false,
    locale: null,
    firstLaunchDone: false,
    autoLockMinutes: kDefaultAutoLockMinutes,
    defaultNode: (
      host: kDefaultDaemonHost,
      port: kDefaultDaemonPort,
      ssl: kDefaultDaemonSSL,
    ),
  );
}

Future<String?> _read(String key) async {
  try {
    return await _storage.read(key: key);
  } catch (_) {
    // Storage blocked (private mode, disabled cookies) — fall back to defaults
    // rather than failing startup.
    return null;
  }
}

Future<void> _write(String key, String value) async {
  try {
    await _storage.write(key: key, value: value);
  } catch (_) {
    // Non-fatal: the setting just will not survive a reload.
  }
}

/// Reads every stored preference concurrently. Call before `runApp`.
Future<PreloadedPrefs> loadPreferences() async {
  final values = await Future.wait([
    _read(_kThemeModeKey),        // 0
    _read(_kLogLevelKey),         // 1
    _read(_kNotificationsKey),    // 2
    _read(_kAutosaveKey),         // 3
    _read(_kScanCoinbaseKey),     // 4
    _read(_kLocaleKey),           // 5
    _read(_kFirstLaunchDoneKey),  // 6
    _read(_kAutoLockKey),         // 7
    _read(_kDefaultNodeHostKey),  // 8
    _read(_kDefaultNodePortKey),  // 9
    _read(_kDefaultNodeSSLKey),   // 10
  ]);

  final localeCode = values[5];
  final host = values[8];

  final prefs = PreloadedPrefs(
    themeMode: switch (values[0]) {
      'light' => ThemeMode.light,
      'dark' => ThemeMode.dark,
      _ => ThemeMode.system,
    },
    logLevel: WalletLogLevel.values.firstWhere(
      (l) => l.value == (int.tryParse(values[1] ?? '') ?? 3),
      orElse: () => WalletLogLevel.info,
    ),
    notificationsEnabled: values[2] != 'false',
    autosaveEnabled: values[3] != 'false',
    scanCoinbase: values[4] == 'true',
    locale: (localeCode != null && localeCode.isNotEmpty)
        ? Locale(localeCode)
        : null,
    firstLaunchDone: values[6] == 'true',
    autoLockMinutes: int.tryParse(values[7] ?? '') ?? kDefaultAutoLockMinutes,
    defaultNode: host == null
        ? (host: kDefaultDaemonHost, port: kDefaultDaemonPort, ssl: kDefaultDaemonSSL)
        : (
            host: host,
            port: int.tryParse(values[9] ?? '') ?? kDefaultDaemonPort,
            ssl: values[10] == 'true',
          ),
  );

  // Number and date formatting follow the chosen language from the first frame.
  applyIntlLocale(prefs.locale);
  return prefs;
}

/// Points `package:intl` at [locale] so amounts are grouped and punctuated the
/// way the active language expects.
void applyIntlLocale(Locale? locale) {
  Intl.defaultLocale = locale?.toLanguageTag() ?? 'en';
}

/// Overridden in `main()` with the result of [loadPreferences].
final preloadedPrefsProvider = Provider<PreloadedPrefs>(
  (_) => PreloadedPrefs.fallback,
);

// ── Theme mode ────────────────────────────────────────────────────────────────

class ThemeModeNotifier extends Notifier<ThemeMode> {
  @override
  ThemeMode build() => ref.read(preloadedPrefsProvider).themeMode;

  Future<void> set(ThemeMode mode) async {
    state = mode;
    await _write(
      _kThemeModeKey,
      switch (mode) {
        ThemeMode.light => 'light',
        ThemeMode.dark => 'dark',
        _ => 'system',
      },
    );
  }
}

final themeModeProvider =
    NotifierProvider<ThemeModeNotifier, ThemeMode>(ThemeModeNotifier.new);

// ── Log level ─────────────────────────────────────────────────────────────────

/// Maps to C++ Logger::LogLevel: DISABLED=0, FATAL=1, WARNING=2, INFO=3, DEBUG=4, TRACE=5
enum WalletLogLevel {
  disabled(0, 'Disabled'),
  fatal(1, 'Fatal only'),
  warning(2, 'Warnings'),
  info(3, 'Info'),
  debug(4, 'Debug'),
  trace(5, 'Trace');

  final int value;
  final String label;
  const WalletLogLevel(this.value, this.label);
}

class LogLevelNotifier extends Notifier<WalletLogLevel> {
  @override
  WalletLogLevel build() => ref.read(preloadedPrefsProvider).logLevel;

  Future<void> set(WalletLogLevel level) async {
    state = level;
    await _write(_kLogLevelKey, level.value.toString());
  }
}

final logLevelProvider =
    NotifierProvider<LogLevelNotifier, WalletLogLevel>(LogLevelNotifier.new);

// ── Notifications preference ──────────────────────────────────────────────────

class NotificationsEnabledNotifier extends Notifier<bool> {
  @override
  bool build() => ref.read(preloadedPrefsProvider).notificationsEnabled;

  Future<void> set(bool enabled) async {
    state = enabled;
    await _write(_kNotificationsKey, enabled.toString());
  }
}

final notificationsEnabledProvider =
    NotifierProvider<NotificationsEnabledNotifier, bool>(
        NotificationsEnabledNotifier.new);

// ── Autosave preference ──────────────────────────────────────────────────────

class AutosaveEnabledNotifier extends Notifier<bool> {
  @override
  bool build() => ref.read(preloadedPrefsProvider).autosaveEnabled;

  Future<void> set(bool enabled) async {
    state = enabled;
    await _write(_kAutosaveKey, enabled.toString());
  }
}

final autosaveEnabledProvider =
    NotifierProvider<AutosaveEnabledNotifier, bool>(
        AutosaveEnabledNotifier.new);

// ── Scan coinbase ────────────────────────────────────────────────────────────

class ScanCoinbaseNotifier extends Notifier<bool> {
  @override
  bool build() => ref.read(preloadedPrefsProvider).scanCoinbase;

  Future<void> set(bool enabled) async {
    state = enabled;
    await _write(_kScanCoinbaseKey, enabled.toString());
  }
}

final scanCoinbaseProvider =
    NotifierProvider<ScanCoinbaseNotifier, bool>(ScanCoinbaseNotifier.new);

// ── Auto-lock ────────────────────────────────────────────────────────────────

/// Minutes of inactivity before the wallet locks itself. 0 disables it.
///
/// A browser tab left open on a shared machine previously stayed unlocked
/// forever with the seed one menu click away.
class AutoLockNotifier extends Notifier<int> {
  @override
  int build() => ref.read(preloadedPrefsProvider).autoLockMinutes;

  Future<void> set(int minutes) async {
    state = minutes;
    await _write(_kAutoLockKey, minutes.toString());
  }
}

final autoLockMinutesProvider =
    NotifierProvider<AutoLockNotifier, int>(AutoLockNotifier.new);

/// Selectable idle timeouts, in minutes (0 = never).
const List<int> kAutoLockChoices = [0, 1, 5, 15, 30, 60];

// ── Wallet lock ───────────────────────────────────────────────────────────────

/// When true, the wallet is open but the UI is locked — a login screen is shown.
/// Set to true on lock, false after successful re-auth.
final walletLockedProvider = StateProvider<bool>((_) => false);

// ── Locale ───────────────────────────────────────────────────────────────────

const supportedLocales = [
  Locale('en'),
  Locale('fr'),
  Locale('de'),
  Locale('zh'),
  Locale('vi'),
  Locale('ja'),
  Locale('es'),
  Locale('pt'),
  Locale('ru'),
];

class LocaleNotifier extends Notifier<Locale?> {
  @override
  Locale? build() => ref.read(preloadedPrefsProvider).locale;

  Future<void> set(Locale? locale) async {
    state = locale;
    // Keep number/date formatting in step with the UI language — otherwise a
    // German user sees "1.234,56"-style prompts but amounts parsed as en_US.
    applyIntlLocale(locale);
    await _write(_kLocaleKey, locale?.languageCode ?? '');
  }
}

final localeProvider =
    NotifierProvider<LocaleNotifier, Locale?>(LocaleNotifier.new);

// ── First launch flag ────────────────────────────────────────────────────────

/// Synchronous now that preferences are preloaded — the router no longer has to
/// guess an initial route from an unresolved future and then correct itself.
class FirstLaunchNotifier extends Notifier<bool> {
  @override
  bool build() => ref.read(preloadedPrefsProvider).firstLaunchDone;

  Future<void> markDone() async {
    state = true;
    await _write(_kFirstLaunchDoneKey, 'true');
  }
}

final firstLaunchDoneProvider =
    NotifierProvider<FirstLaunchNotifier, bool>(FirstLaunchNotifier.new);

// ── Default node preference ───────────────────────────────────────────────────

class DefaultNodeNotifier extends Notifier<DefaultNode> {
  @override
  DefaultNode build() => ref.read(preloadedPrefsProvider).defaultNode;

  Future<void> set({required String host, required int port, required bool ssl}) async {
    state = (host: host, port: port, ssl: ssl);
    await Future.wait([
      _write(_kDefaultNodeHostKey, host),
      _write(_kDefaultNodePortKey, port.toString()),
      _write(_kDefaultNodeSSLKey, ssl.toString()),
    ]);
  }
}

final defaultNodeProvider =
    NotifierProvider<DefaultNodeNotifier, DefaultNode>(DefaultNodeNotifier.new);

// ── Last opened wallet ───────────────────────────────────────────────────────

const _kLastWalletKey = 'pluton_last_wallet_name';

/// Name of the most recently opened wallet.
///
/// This is what "Delete Wallet Data" deletes. It was previously never written,
/// so that flow read null, skipped the delete entirely, and still reported
/// success — leaving the wallet sitting in IndexedDB.
Future<String?> readLastWalletName() => _read(_kLastWalletKey);

Future<void> saveLastWalletName(String name) => _write(_kLastWalletKey, name);

Future<void> clearLastWalletName() async {
  try {
    await _storage.delete(key: _kLastWalletKey);
  } catch (_) {
    // Nothing to clear.
  }
}
