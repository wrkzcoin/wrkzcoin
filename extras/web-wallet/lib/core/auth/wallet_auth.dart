import 'package:flutter_secure_storage/flutter_secure_storage.dart';

const _storage = FlutterSecureStorage();
const _kWalletPasswordKey = 'pluton_wallet_password';

/// Persists the wallet password so the lock screen can verify it without
/// re-opening the wallet. FlutterSecureStorage uses the OS keychain/keystore,
/// so the value is protected at rest.
Future<void> storeWalletPassword(String password) async {
  await _storage.write(key: _kWalletPasswordKey, value: password);
}

/// Clears the stored password. Call on wallet close / logout.
Future<void> clearWalletPassword() async {
  await _storage.delete(key: _kWalletPasswordKey);
}

/// Returns true if the given [password] matches the stored one.
Future<bool> verifyWalletPassword(String password) async {
  final stored = await _storage.read(key: _kWalletPasswordKey);
  return stored != null && stored == password;
}
