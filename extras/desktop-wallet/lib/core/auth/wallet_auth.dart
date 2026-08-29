import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

const _storage = FlutterSecureStorage();
const _kVerifierKey = 'pluton_wallet_pwverifier';

// ── password verifier ────────────────────────────────────────────────────────
//
// The wallet file itself is the authority on whether a password is correct.
// What is stored here is only a verifier, used to re-authenticate the user
// while the wallet is *already* open — the lock screen, revealing the seed —
// where re-opening the file would be wasteful.
//
// It is a PBKDF2-HMAC-SHA256 digest with a random salt, so a reader of the OS
// keychain cannot recover the password itself. The previous scheme stored the
// password verbatim.

const _kPbkdf2Iterations = 100000;
const _kSaltBytes = 16;
const _kKeyBytes = 32;

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

/// Records a verifier for [password] so the lock screen can re-authenticate
/// the user without re-opening the wallet file.
Future<void> storePasswordVerifier(String password) async {
  final rng = Random.secure();
  final salt =
      Uint8List.fromList(List.generate(_kSaltBytes, (_) => rng.nextInt(256)));
  final key =
      _pbkdf2(utf8.encode(password), salt, _kPbkdf2Iterations, _kKeyBytes);
  final record = jsonEncode({
    'v': 1,
    'salt': base64Encode(salt),
    'key': base64Encode(key),
    'iterations': _kPbkdf2Iterations,
  });
  await _storage.write(key: _kVerifierKey, value: record);
}

/// Checks [password] against the stored verifier.
///
/// Returns null when there is no usable verifier, meaning "cannot tell". The
/// caller must then offer to close and re-open the wallet rather than treating
/// the absence as a failed login — otherwise a lost keychain entry would lock
/// the user out of a wallet whose password they still know.
Future<bool?> verifyWalletPassword(String password) async {
  final raw = await _storage.read(key: _kVerifierKey);
  if (raw == null) return null;
  try {
    final json = jsonDecode(raw) as Map<String, dynamic>;
    final salt = base64Decode(json['salt'] as String);
    final expected = base64Decode(json['key'] as String);
    final iterations =
        (json['iterations'] as num?)?.toInt() ?? _kPbkdf2Iterations;
    final actual =
        _pbkdf2(utf8.encode(password), salt, iterations, expected.length);
    return _constantTimeEquals(actual, expected);
  } catch (_) {
    return null; // corrupt record — treat as "cannot tell"
  }
}

/// Clears the stored verifier. Call on wallet close / logout.
Future<void> clearWalletPassword() async {
  await _storage.delete(key: _kVerifierKey);
  // Remove the pre-verifier plaintext entry if this install still carries one.
  await _storage.delete(key: 'pluton_wallet_password');
}
