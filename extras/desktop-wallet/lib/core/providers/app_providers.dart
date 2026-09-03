import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import '../config/app_config.dart';
import '../ffi/wallet_ffi.dart';
import 'providers.dart';

const _storage = FlutterSecureStorage();
const _kThemeModeKey = 'pluton_theme_mode';
const _kLogLevelKey = 'pluton_log_level';
const _kNotificationsKey = 'pluton_notifications_enabled';

// ── Theme mode ────────────────────────────────────────────────────────────────

class ThemeModeNotifier extends Notifier<ThemeMode> {
  @override
  ThemeMode build() {
    _load();
    return ThemeMode.system;
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kThemeModeKey);
    state = switch (v) {
      'light' => ThemeMode.light,
      'dark' => ThemeMode.dark,
      _ => ThemeMode.system,
    };
  }

  Future<void> set(ThemeMode mode) async {
    state = mode;
    await _storage.write(
      key: _kThemeModeKey,
      value: switch (mode) {
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
  WalletLogLevel build() {
    _load();
    return WalletLogLevel.info;
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kLogLevelKey);
    final n = int.tryParse(v ?? '') ?? 3;
    state = WalletLogLevel.values.firstWhere(
      (l) => l.value == n,
      orElse: () => WalletLogLevel.info,
    );
    _applyToNative(state);
  }

  Future<void> set(WalletLogLevel level) async {
    state = level;
    _applyToNative(level);
    await _storage.write(key: _kLogLevelKey, value: level.value.toString());
  }

  /// The native logger defaults to DISABLED on every process start, so the
  /// stored preference has to be pushed across on load as well as on change —
  /// otherwise the log viewer stays empty until the dropdown is touched.
  void _applyToNative(WalletLogLevel level) {
    try {
      ref.read(walletCApiProvider).setLogLevel(level.name);
    } catch (_) {
      // Library not loadable yet/at all — logging is best effort.
    }
  }
}

final logLevelProvider =
    NotifierProvider<LogLevelNotifier, WalletLogLevel>(LogLevelNotifier.new);

// ── Notifications preference ──────────────────────────────────────────────────

class NotificationsEnabledNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return true; // default: On
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kNotificationsKey);
    state = v != 'false';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await _storage.write(key: _kNotificationsKey, value: enabled.toString());
  }
}

final notificationsEnabledProvider =
    NotifierProvider<NotificationsEnabledNotifier, bool>(
        NotificationsEnabledNotifier.new);

// ── Autosave preference ──────────────────────────────────────────────────────

const _kAutosaveKey = 'pluton_autosave_enabled';

class AutosaveEnabledNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return true; // default: On
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kAutosaveKey);
    state = v != 'false';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await _storage.write(key: _kAutosaveKey, value: enabled.toString());
  }
}

final autosaveEnabledProvider =
    NotifierProvider<AutosaveEnabledNotifier, bool>(
        AutosaveEnabledNotifier.new);

// ── Scan coinbase ────────────────────────────────────────────────────────────

const _kScanCoinbaseKey = 'pluton_scan_coinbase';

class ScanCoinbaseNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return false; // default: Off
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kScanCoinbaseKey);
    if (v != null) state = v == 'true';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await _storage.write(key: _kScanCoinbaseKey, value: enabled.toString());
  }
}

final scanCoinbaseProvider =
    NotifierProvider<ScanCoinbaseNotifier, bool>(ScanCoinbaseNotifier.new);

// ── External Tx PoW server ───────────────────────────────────────────────────

const _kTxPowServerKey = 'pluton_tx_pow_server';

/// Where the wallet sends its transaction proof of work. When [active], the
/// native wallet asks the server first and falls back to this device's CPU
/// if the server does not answer. Off by default: the fields start out
/// pointing at the project's public server so enabling it is one switch.
class TxPowServerSettings {
  const TxPowServerSettings({
    this.enabled = false,
    this.host = kDefaultTxPowServerHost,
    this.port = kDefaultTxPowServerPort,
    this.ssl = kDefaultTxPowServerSSL,
    this.loaded = false,
  });

  final bool enabled;
  final String host;
  final int port;
  final bool ssl;

  /// True once the stored value has been read, so forms know when to prefill.
  final bool loaded;

  bool get active => enabled && host.isNotEmpty && port > 0;

  TxPowServerSettings copyWith({
    bool? enabled,
    String? host,
    int? port,
    bool? ssl,
    bool? loaded,
  }) =>
      TxPowServerSettings(
        enabled: enabled ?? this.enabled,
        host: host ?? this.host,
        port: port ?? this.port,
        ssl: ssl ?? this.ssl,
        loaded: loaded ?? this.loaded,
      );

  Map<String, dynamic> toJson() =>
      {'enabled': enabled, 'host': host, 'port': port, 'ssl': ssl};

  factory TxPowServerSettings.fromJson(Map<String, dynamic> j) {
    final host = (j['host'] as String? ?? '').trim();
    return TxPowServerSettings(
      enabled: j['enabled'] as bool? ?? false,
      host: host.isEmpty ? kDefaultTxPowServerHost : host,
      port: (j['port'] as num?)?.toInt() ?? kDefaultTxPowServerPort,
      ssl: j['ssl'] as bool? ?? kDefaultTxPowServerSSL,
      loaded: true,
    );
  }

  /// Pushes this setting into the native wallet. Call after every wallet
  /// open and whenever the setting changes.
  void applyTo(WalletCApi ffi) =>
      ffi.setTxPowServer(active ? host : '', port, ssl: ssl);
}

class TxPowServerNotifier extends Notifier<TxPowServerSettings> {
  @override
  TxPowServerSettings build() {
    _load();
    return const TxPowServerSettings();
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kTxPowServerKey);
    if (v == null || v.isEmpty) {
      state = state.copyWith(loaded: true);
      return;
    }
    try {
      state = TxPowServerSettings.fromJson(
          jsonDecode(v) as Map<String, dynamic>);
    } catch (_) {
      state = state.copyWith(loaded: true);
    }
  }

  Future<void> set(TxPowServerSettings settings) async {
    state = settings.copyWith(loaded: true);
    await _storage.write(
        key: _kTxPowServerKey, value: jsonEncode(settings.toJson()));
  }
}

final txPowServerProvider =
    NotifierProvider<TxPowServerNotifier, TxPowServerSettings>(
        TxPowServerNotifier.new);

// ── Wallet lock ───────────────────────────────────────────────────────────────

/// When true, the wallet is open but the UI is locked — a login screen is shown.
/// Set to true on lock, false after successful re-auth.
final walletLockedProvider = StateProvider<bool>((_) => false);

// ── Locale ───────────────────────────────────────────────────────────────────

const _kLocaleKey = 'pluton_locale';
const _kFirstLaunchDoneKey = 'pluton_first_launch_done';

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
  Locale? build() {
    _load();
    return null; // null = system default
  }

  Future<void> _load() async {
    final v = await _storage.read(key: _kLocaleKey);
    if (v != null && v.isNotEmpty) {
      state = Locale(v);
    }
  }

  Future<void> set(Locale? locale) async {
    state = locale;
    await _storage.write(key: _kLocaleKey, value: locale?.languageCode ?? '');
  }
}

final localeProvider =
    NotifierProvider<LocaleNotifier, Locale?>(LocaleNotifier.new);

// ── First launch flag ────────────────────────────────────────────────────────

final firstLaunchDoneProvider = FutureProvider<bool>((ref) async {
  final v = await _storage.read(key: _kFirstLaunchDoneKey);
  return v == 'true';
});

Future<void> markFirstLaunchDone() async {
  await _storage.write(key: _kFirstLaunchDoneKey, value: 'true');
}
