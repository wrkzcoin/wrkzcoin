import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:local_auth/local_auth.dart';

import '../config/app_config.dart';

const _storage = FlutterSecureStorage();
final _localAuth = LocalAuthentication();

// ── password storage ─────────────────────────────────────────────────────────

Future<void> storeWalletPassword(String filename, String password) async {
  await _storage.write(
    key: AppConfig.walletPasswordKey(filename),
    value: password,
  );
}

Future<bool> verifyWalletPassword(String filename, String password) async {
  final stored = await _storage.read(
    key: AppConfig.walletPasswordKey(filename),
  );
  return stored == password;
}

Future<void> clearWalletPassword(String filename) async {
  await _storage.delete(key: AppConfig.walletPasswordKey(filename));
}

// ── biometric ────────────────────────────────────────────────────────────────

Future<bool> isBiometricAvailable() async {
  try {
    final canCheck = await _localAuth.canCheckBiometrics;
    final isSupported = await _localAuth.isDeviceSupported();
    return canCheck && isSupported;
  } catch (_) {
    return false;
  }
}

Future<bool> authenticateWithBiometric() async {
  try {
    return await _localAuth.authenticate(
      localizedReason: 'Unlock your WRKZ wallet',
      options: const AuthenticationOptions(
        stickyAuth: true,
        biometricOnly: true,
      ),
    );
  } catch (_) {
    return false;
  }
}

// ── general preferences ──────────────────────────────────────────────────────

Future<void> storePref(String key, String value) async {
  await _storage.write(key: key, value: value);
}

Future<String?> readPref(String key) async {
  return _storage.read(key: key);
}

Future<void> deletePref(String key) async {
  await _storage.delete(key: key);
}

// ── seed backup flag (per-wallet) ────────────────────────────────────────────

String _seedBackupKey(String filename) =>
    '${AppConfig.skSeedBackupConfirmed}_$filename';

Future<void> markSeedBackupConfirmed(String filename) async {
  await _storage.write(key: _seedBackupKey(filename), value: 'true');
}

Future<bool> isSeedBackupConfirmed(String filename) async {
  final v = await _storage.read(key: _seedBackupKey(filename));
  return v == 'true';
}

Future<void> clearSeedBackupFlag(String filename) async {
  await _storage.delete(key: _seedBackupKey(filename));
}
