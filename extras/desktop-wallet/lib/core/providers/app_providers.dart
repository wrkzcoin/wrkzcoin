import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

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
  }

  Future<void> set(WalletLogLevel level) async {
    state = level;
    await _storage.write(key: _kLogLevelKey, value: level.value.toString());
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

// ── Wallet lock ───────────────────────────────────────────────────────────────

/// When true, the wallet is open but the UI is locked — a login screen is shown.
/// Set to true on lock, false after successful re-auth.
final walletLockedProvider = StateProvider<bool>((_) => false);
