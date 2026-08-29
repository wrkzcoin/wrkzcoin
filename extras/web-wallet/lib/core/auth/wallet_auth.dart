/// Lock-screen credential handling.
///
/// **What changed and why.** This used to persist the wallet password itself so
/// the lock screen could compare strings. On web that is not safe: the only
/// storage available to `flutter_secure_storage` is `window.localStorage`, and
/// its web backend keeps the AES key in localStorage *next to* the ciphertext
/// (see flutter_secure_storage_web `_getEncryptionKey`). Anything that can read
/// localStorage — an XSS payload, a browser extension with host permission, or
/// someone with the browser profile on disk — recovers the plaintext password
/// and therefore the wallet.
///
/// The lock screen never needs the password back; it only needs to answer
/// "is this the same password?". So we store a PBKDF2-SHA256 verifier
/// (random salt + iteration count + derived key) and compare digests in
/// constant time. A leak of localStorage now yields a slow-to-attack hash
/// instead of the key to the funds.
library;

import 'dart:convert';
import 'dart:js_interop';
import 'dart:js_interop_unsafe';

import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:web/web.dart' as web;

const _storage = FlutterSecureStorage();

/// Key holding the PBKDF2 verifier document.
const _kWalletVerifierKey = 'pluton_wallet_verifier';

/// Legacy key that held the password in recoverable form. Purged on sight.
const _kLegacyPasswordKey = 'pluton_wallet_password';

/// PBKDF2 work factor. Stored alongside each verifier so it can be raised
/// later without invalidating credentials created under the old value.
const int _kIterations = 310000;
const int _kSaltBytes = 16;
const int _kKeyBits = 256;
const int _kVerifierVersion = 1;

// ─── WebCrypto plumbing ───────────────────────────────────────────────────────

/// Builds `{name: 'PBKDF2', salt, iterations, hash: 'SHA-256'}`.
JSObject _pbkdf2Params(Uint8List salt, int iterations) {
  final params = JSObject();
  params.setProperty('name'.toJS, 'PBKDF2'.toJS);
  params.setProperty('salt'.toJS, salt.toJS);
  params.setProperty('iterations'.toJS, iterations.toJS);
  params.setProperty('hash'.toJS, 'SHA-256'.toJS);
  return params;
}

Uint8List _randomBytes(int length) {
  final bytes = Uint8List(length);
  web.window.crypto.getRandomValues(bytes.toJS);
  return bytes;
}

/// Derives [_kKeyBits] bits from [password] with PBKDF2-SHA256.
Future<Uint8List> _derive(String password, Uint8List salt, int iterations) async {
  final subtle = web.window.crypto.subtle;

  final baseKey = await subtle
      .importKey(
        'raw',
        Uint8List.fromList(utf8.encode(password)).toJS,
        'PBKDF2'.toJS,
        false,
        <JSString>['deriveBits'.toJS].toJS,
      )
      .toDart;

  final bits = await subtle
      .deriveBits(_pbkdf2Params(salt, iterations), baseKey, _kKeyBits)
      .toDart;

  return bits.toDart.asUint8List();
}

/// Length-independent, data-independent comparison. A plain `==` on digests
/// leaks how many leading bytes matched via timing.
bool _constantTimeEquals(Uint8List a, Uint8List b) {
  var diff = a.length ^ b.length;
  final n = a.length < b.length ? a.length : b.length;
  for (var i = 0; i < n; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff == 0;
}

// ─── Public API ───────────────────────────────────────────────────────────────

/// Stores a non-reversible verifier for [password] so the lock screen can check
/// it later. The password itself is never written anywhere.
Future<void> storeWalletPassword(String password) async {
  final salt = _randomBytes(_kSaltBytes);
  final derived = await _derive(password, salt, _kIterations);

  await _storage.write(
    key: _kWalletVerifierKey,
    value: jsonEncode({
      'v': _kVerifierVersion,
      'algorithm': 'PBKDF2-SHA256',
      'iterations': _kIterations,
      'salt': base64Encode(salt),
      'hash': base64Encode(derived),
    }),
  );

  // Remove any password left behind by an older build.
  await _purgeLegacyPassword();
}

/// Clears the stored verifier. Call on wallet close / logout.
Future<void> clearWalletPassword() async {
  await _storage.delete(key: _kWalletVerifierKey);
  await _purgeLegacyPassword();
}

/// Returns true if the given [password] matches the stored verifier.
Future<bool> verifyWalletPassword(String password) async {
  final raw = await _storage.read(key: _kWalletVerifierKey);
  if (raw == null) return false;

  Map<String, dynamic> doc;
  try {
    doc = jsonDecode(raw) as Map<String, dynamic>;
  } catch (_) {
    return false;
  }

  final saltB64 = doc['salt'] as String?;
  final hashB64 = doc['hash'] as String?;
  final iterations = (doc['iterations'] as num?)?.toInt();
  if (saltB64 == null || hashB64 == null || iterations == null || iterations <= 0) {
    return false;
  }

  try {
    final expected = Uint8List.fromList(base64Decode(hashB64));
    final actual = await _derive(password, Uint8List.fromList(base64Decode(saltB64)), iterations);
    return _constantTimeEquals(expected, actual);
  } catch (e) {
    debugPrint('[wallet_auth] verifier check failed: $e');
    return false;
  }
}

/// True when a verifier exists — i.e. the lock screen can authenticate offline.
Future<bool> hasStoredWalletPassword() async {
  return (await _storage.read(key: _kWalletVerifierKey)) != null;
}

/// Deletes any password persisted by a previous version of this app.
///
/// Runs at startup as well as on every store/clear so existing installs stop
/// carrying a recoverable password around the moment they load new code.
Future<void> _purgeLegacyPassword() async {
  try {
    if (await _storage.read(key: _kLegacyPasswordKey) != null) {
      await _storage.delete(key: _kLegacyPasswordKey);
      debugPrint('[wallet_auth] removed legacy plaintext password entry');
    }
  } catch (_) {
    // Storage unavailable (private mode) — nothing to purge.
  }
}

/// Called once during startup.
Future<void> purgeLegacyCredentials() => _purgeLegacyPassword();
