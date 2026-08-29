import 'package:intl/intl.dart';

import '../../core/config/app_config.dart';

final _numberFormat = NumberFormat('#,##0', 'en_US');

/// Converts atomic units to a human-readable string.
/// 100 atomic = 1.00 WRKZ (2 decimal places).
String formatAmount(int atomic, {bool showTicker = false}) {
  final whole = atomic ~/ AppConfig.atomicDivisor;
  final frac = atomic.remainder(AppConfig.atomicDivisor).abs();
  final fracStr = frac.toString().padLeft(AppConfig.decimalPlaces, '0');
  // `~/` truncates toward zero, so -50 atomic gives whole == 0 and the sign
  // would be lost. Carry it explicitly.
  final sign = (atomic < 0 && whole == 0) ? '-' : '';
  final formatted = '$sign${_numberFormat.format(whole)}.$fracStr';
  return showTicker ? '$formatted ${AppConfig.ticker}' : formatted;
}

/// Parses a human-readable amount string to atomic units.
/// Returns null if the string is invalid.
int? parseAmount(String input) {
  if (input.trim().isEmpty) return null;
  final cleaned = input.replaceAll(',', '').trim();
  // Reject any sign outright. Checking `whole < 0` after parsing is not
  // enough: "-0.01" splits into a whole part of "-0", which parses to 0 and
  // slips through as a positive amount.
  if (cleaned.contains('-') || cleaned.contains('+')) return null;
  final parts = cleaned.split('.');
  if (parts.length > 2) return null;

  final whole = int.tryParse(parts[0]);
  if (whole == null || whole < 0) return null;

  int frac = 0;
  if (parts.length == 2) {
    var fracStr = parts[1];
    if (fracStr.length > AppConfig.decimalPlaces) return null;
    fracStr = fracStr.padRight(AppConfig.decimalPlaces, '0');
    frac = int.tryParse(fracStr) ?? 0;
  }

  return whole * AppConfig.atomicDivisor + frac;
}

/// Base58 alphabet used by CryptoNote addresses (no 0, O, I or l).
final _base58 = RegExp(
    r'^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$');

/// Cheap client-side sanity check on a WRKZ address.
///
/// This is a shape check, not a checksum check — the native layer still
/// validates properly. It exists so an obvious typo is caught before a
/// transaction is built.
bool isValidWrkzAddress(String address) {
  if (!AppConfig.validAddressLengths.contains(address.length)) return false;
  if (!address.startsWith(AppConfig.addressPrefix)) return false;
  return _base58.hasMatch(address);
}

/// Validates an optional payment ID. Empty is allowed (means "none").
/// A payment ID is 64 hex characters; 16 is accepted for short/integrated use.
bool isValidPaymentId(String paymentId) {
  if (paymentId.isEmpty) return true;
  if (paymentId.length != 16 && paymentId.length != 64) return false;
  return RegExp(r'^[0-9a-fA-F]+$').hasMatch(paymentId);
}

/// Extracts a bare address from a scanned QR payload.
///
/// Accepts a plain address, or a `wrkz:<address>?amount=…&paymentId=…` URI.
/// Returns the address and any amount / payment ID carried alongside it.
({String address, String? amount, String? paymentId}) parseAddressPayload(
    String raw) {
  final value = raw.trim();
  if (!value.toLowerCase().startsWith('wrkz:')) {
    return (address: value, amount: null, paymentId: null);
  }
  // Uri.parse treats everything after "wrkz:" as an opaque path, so split by
  // hand rather than relying on host/path parsing.
  final body = value.substring('wrkz:'.length);
  final qIndex = body.indexOf('?');
  if (qIndex < 0) {
    return (address: body, amount: null, paymentId: null);
  }
  final address = body.substring(0, qIndex);
  final params = Uri.splitQueryString(body.substring(qIndex + 1));
  return (
    address: address,
    amount: params['amount'],
    paymentId: params['paymentId'] ?? params['payment_id'],
  );
}
