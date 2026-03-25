import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../auth/wallet_auth.dart';
import '../config/app_config.dart';

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
