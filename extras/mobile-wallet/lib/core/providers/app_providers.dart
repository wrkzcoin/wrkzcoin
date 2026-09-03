import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../auth/wallet_auth.dart';
import '../config/app_config.dart';
import '../ffi/wallet_ffi.dart';

// ── theme mode ───────────────────────────────────────────────────────────────

class ThemeModeNotifier extends Notifier<ThemeMode> {
  @override
  ThemeMode build() {
    _load();
    return ThemeMode.system;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skThemeMode);
    if (v != null) {
      final mode = ThemeMode.values.firstWhere(
        (m) => m.name == v,
        orElse: () => ThemeMode.system,
      );
      state = mode;
    }
  }

  Future<void> set(ThemeMode mode) async {
    state = mode;
    await storePref(AppConfig.skThemeMode, mode.name);
  }
}

final themeModeProvider =
    NotifierProvider<ThemeModeNotifier, ThemeMode>(ThemeModeNotifier.new);

// ── notifications ────────────────────────────────────────────────────────────

class NotificationsEnabledNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return true;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skNotificationsEnabled);
    if (v != null) state = v == 'true';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await storePref(AppConfig.skNotificationsEnabled, enabled.toString());
  }
}

final notificationsEnabledProvider =
    NotifierProvider<NotificationsEnabledNotifier, bool>(
        NotificationsEnabledNotifier.new);

// ── autosave ─────────────────────────────────────────────────────────────────

class AutosaveEnabledNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return true;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skAutosaveEnabled);
    if (v != null) state = v == 'true';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await storePref(AppConfig.skAutosaveEnabled, enabled.toString());
  }
}

final autosaveEnabledProvider =
    NotifierProvider<AutosaveEnabledNotifier, bool>(
        AutosaveEnabledNotifier.new);

// ── auto-lock timeout ────────────────────────────────────────────────────────

class AutoLockIndexNotifier extends Notifier<int> {
  @override
  int build() {
    _load();
    return AppConfig.defaultAutoLockIndex;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skAutoLockIndex);
    if (v != null) {
      final idx = int.tryParse(v);
      if (idx != null && idx >= 0 && idx < AppConfig.autoLockOptions.length) {
        state = idx;
      }
    }
  }

  AutoLockOption get current => AppConfig.autoLockOptions[state];

  Future<void> set(int index) async {
    state = index;
    await storePref(AppConfig.skAutoLockIndex, index.toString());
  }
}

final autoLockIndexProvider =
    NotifierProvider<AutoLockIndexNotifier, int>(AutoLockIndexNotifier.new);

// ── biometric enabled ────────────────────────────────────────────────────────

class BiometricEnabledNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return false;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skBiometricEnabled);
    if (v != null) state = v == 'true';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await storePref(AppConfig.skBiometricEnabled, enabled.toString());
  }
}

final biometricEnabledProvider =
    NotifierProvider<BiometricEnabledNotifier, bool>(
        BiometricEnabledNotifier.new);

// ── log level ────────────────────────────────────────────────────────────────

enum WalletLogLevel {
  disabled('disabled'),
  fatal('fatal'),
  warning('warning'),
  info('info'),
  debug('debug'),
  trace('trace');

  final String value;
  const WalletLogLevel(this.value);
}

class LogLevelNotifier extends Notifier<WalletLogLevel> {
  @override
  WalletLogLevel build() {
    _load();
    return WalletLogLevel.disabled;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skLogLevel);
    if (v != null) {
      state = WalletLogLevel.values.firstWhere(
        (l) => l.value == v,
        orElse: () => WalletLogLevel.disabled,
      );
    }
  }

  Future<void> set(WalletLogLevel level) async {
    state = level;
    await storePref(AppConfig.skLogLevel, level.value);
  }
}

final logLevelProvider =
    NotifierProvider<LogLevelNotifier, WalletLogLevel>(LogLevelNotifier.new);

// ── scan coinbase ───────────────────────────────────────────────────────────

class ScanCoinbaseNotifier extends Notifier<bool> {
  @override
  bool build() {
    _load();
    return false;
  }

  Future<void> _load() async {
    final v = await readPref(AppConfig.skScanCoinbase);
    if (v != null) state = v == 'true';
  }

  Future<void> set(bool enabled) async {
    state = enabled;
    await storePref(AppConfig.skScanCoinbase, enabled.toString());
  }
}

final scanCoinbaseProvider =
    NotifierProvider<ScanCoinbaseNotifier, bool>(ScanCoinbaseNotifier.new);

// ── external tx PoW server ──────────────────────────────────────────────────

const _kTxPowServerKey = 'pref_tx_pow_server';

/// Default port of wrkz-txpow-server.
const int kDefaultTxPowServerPort = 17870;

/// Where the wallet sends its transaction proof of work. When [active], the
/// native wallet asks the server first and falls back to this phone's CPU if
/// the server does not answer.
class TxPowServerSettings {
  const TxPowServerSettings({
    this.enabled = false,
    this.host = '',
    this.port = kDefaultTxPowServerPort,
    this.ssl = false,
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

  factory TxPowServerSettings.fromJson(Map<String, dynamic> j) =>
      TxPowServerSettings(
        enabled: j['enabled'] as bool? ?? false,
        host: (j['host'] as String? ?? '').trim(),
        port: (j['port'] as num?)?.toInt() ?? kDefaultTxPowServerPort,
        ssl: j['ssl'] as bool? ?? false,
        loaded: true,
      );

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
    final v = await readPref(_kTxPowServerKey);
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
    await storePref(_kTxPowServerKey, jsonEncode(settings.toJson()));
  }
}

final txPowServerProvider =
    NotifierProvider<TxPowServerNotifier, TxPowServerSettings>(
        TxPowServerNotifier.new);

// ── locale ──────────────────────────────────────────────────────────────────

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
    final v = await readPref(AppConfig.skLocale);
    if (v != null && v.isNotEmpty) {
      state = Locale(v);
    }
  }

  Future<void> set(Locale? locale) async {
    state = locale;
    await storePref(AppConfig.skLocale, locale?.languageCode ?? '');
  }
}

final localeProvider =
    NotifierProvider<LocaleNotifier, Locale?>(LocaleNotifier.new);

// ── first launch flag ───────────────────────────────────────────────────────

final firstLaunchDoneProvider = FutureProvider<bool>((ref) async {
  final v = await readPref(AppConfig.skFirstLaunchDone);
  return v == 'true';
});

Future<void> markFirstLaunchDone() async {
  await storePref(AppConfig.skFirstLaunchDone, 'true');
}
