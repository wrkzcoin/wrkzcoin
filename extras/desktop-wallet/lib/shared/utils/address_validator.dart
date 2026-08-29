import '../../core/config/app_config.dart';

/// Base58 alphabet used by CryptoNote addresses (no 0, O, I or l).
final _base58 = RegExp(
    r'^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$');

/// Cheap client-side sanity check on a WRKZ address.
///
/// This is a shape check, not a checksum check — the native layer still
/// validates properly. It exists so an obvious typo is caught before a
/// transaction is built.
bool isValidWrkzAddress(String address) {
  if (!kValidAddressLengths.contains(address.length)) return false;
  if (!address.startsWith(kAddressPrefix)) return false;
  return _base58.hasMatch(address);
}

/// Validates an optional payment ID. Empty is allowed (means "none").
/// A payment ID is 64 hex characters; 16 is accepted for short/integrated use.
bool isValidPaymentId(String paymentId) {
  if (paymentId.isEmpty) return true;
  if (paymentId.length != 16 && paymentId.length != 64) return false;
  return RegExp(r'^[0-9a-fA-F]+$').hasMatch(paymentId);
}
