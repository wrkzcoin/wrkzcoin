import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:local_auth/local_auth.dart';

import '../config/app_config.dart';

const _storage = FlutterSecureStorage();
final _localAuth = LocalAuthentication();

// ── password verifier ────────────────────────────────────────────────────────
//
// The wallet file itself is the authority on whether a password is correct —
// see `WalletCApi.open`. What is stored here is only a verifier, used to
// re-authenticate the user while the wallet is *already* open (auto-lock,
// "reveal seed", "change password"), where re-opening the file would be
// wasteful.
//
// The verifier is a PBKDF2-HMAC-SHA256 digest with a per-wallet random salt,
// so a reader of the keychain cannot recover the password itself.

const _kPbkdf2Iterations = 100000;
const _kSaltBytes = 16;
const _kKeyBytes = 32;

String _verifierKey(String filename) => 'wallet_pwverifier_$filename';

Uint8List _pbkdf2(List<int> password, List<int> salt, int iterations, int len) {
  final hmac = Hmac(sha256, password);
  final out = BytesBuilder();
  var block = 1;
  while (out.length < len) {
    // U1 = HMAC(password, salt || INT32BE(block))
    final blockIndex = Uint8List(4)..buffer.asByteData().setUint32(0, block);
    var u = Uint8List.fromList(hmac.convert([...salt, ...blockIndex]).bytes);
    final t = Uint8List.fromList(u);
    for (var i = 1; i < iterations; i++) {
      u = Uint8List.fromList(hmac.convert(u).bytes);
      for (var j = 0; j < t.length; j++) {
        t[j] ^= u[j];
      }
    }
    out.add(t);
    block++;
  }
  return Uint8List.fromList(out.toBytes().sublist(0, len));
}

/// Constant-time comparison — a plain `==` on the digest would leak, via
/// timing, how many leading bytes of a guess were right.
bool _constantTimeEquals(List<int> a, List<int> b) {
  if (a.length != b.length) return false;
  var diff = 0;
  for (var i = 0; i < a.length; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff == 0;
}

/// Records a verifier for [password] so the app can re-authenticate the user
/// without re-opening the wallet file.
Future<void> storePasswordVerifier(String filename, String password) async {
  final rng = Random.secure();
  final salt =
      Uint8List.fromList(List.generate(_kSaltBytes, (_) => rng.nextInt(256)));
  final key = _pbkdf2(utf8.encode(password), salt, _kPbkdf2Iterations, _kKeyBytes);
  final record = jsonEncode({
    'v': 1,
    'salt': base64Encode(salt),
    'key': base64Encode(key),
    'iterations': _kPbkdf2Iterations,
  });
  await _storage.write(key: _verifierKey(filename), value: record);
}

/// Whether a verifier exists for this wallet.
///
/// When it does not — a wallet created before this scheme, or a keychain that
/// lost its entries — the caller must fall back to opening the wallet file,
/// never to rejecting the user.
Future<bool> hasPasswordVerifier(String filename) async {
  return (await _storage.read(key: _verifierKey(filename))) != null;
}

/// Checks [password] against the stored verifier.
///
/// Returns null when there is no usable verifier, meaning "cannot tell" — the
/// caller must then verify against the wallet file itself rather than treating
/// the absence as a failed login.
Future<bool?> verifyPasswordAgainstVerifier(
    String filename, String password) async {
  final raw = await _storage.read(key: _verifierKey(filename));
  if (raw == null) return null;
  try {
    final json = jsonDecode(raw) as Map<String, dynamic>;
    final salt = base64Decode(json['salt'] as String);
    final expected = base64Decode(json['key'] as String);
    final iterations = (json['iterations'] as num?)?.toInt() ?? _kPbkdf2Iterations;
    final actual =
        _pbkdf2(utf8.encode(password), salt, iterations, expected.length);
    return _constantTimeEquals(actual, expected);
  } catch (_) {
    return null; // corrupt record — fall back to the wallet file
  }
}

Future<void> clearPasswordVerifier(String filename) async {
  await _storage.delete(key: _verifierKey(filename));
}

// ── password escrow (biometric unlock only) ──────────────────────────────────
//
// Unlocking with a fingerprint requires the app to supply the real password to
// the native layer, so it has to be recoverable. It is therefore stored ONLY
// while the user has biometric unlock switched on, and wiped as soon as they
// switch it off.

Future<void> storeWalletPassword(String filename, String password) async {
  await _storage.write(
    key: AppConfig.walletPasswordKey(filename),
    value: password,
  );
}

Future<String?> readWalletPassword(String filename) async {
  return _storage.read(key: AppConfig.walletPasswordKey(filename));
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
