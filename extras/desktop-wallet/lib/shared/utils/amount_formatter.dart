import 'package:intl/intl.dart';
import '../../core/config/app_config.dart';

final _numberFormat = NumberFormat('#,##0', 'en_US');

/// Converts atomic units (int) to a human-readable coin string.
/// WRKZ has 2 decimal places → 100 atomic units = 1.00 WRKZ
String formatAmount(int atomic, {bool showTicker = false}) {
  final divisor = _pow10(kCoinDecimalPlaces);
  final whole = atomic ~/ divisor;
  final frac = atomic.remainder(divisor).abs();
  final fracStr = frac.toString().padLeft(kCoinDecimalPlaces, '0');
  // `~/` truncates toward zero, so -50 atomic gives whole == 0 and the sign
  // would be lost. Carry it explicitly.
  final sign = (atomic < 0 && whole == 0) ? '-' : '';
  final formatted = '$sign${_numberFormat.format(whole)}.$fracStr';
  return showTicker ? '$formatted $kCoinTicker' : formatted;
}

/// Parses a human-readable string (e.g. "1.50") into atomic units.
/// Returns null if the input is invalid — including negative amounts and more
/// precision than the coin supports, both of which must never be silently
/// coerced into a send request.
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

  final divisor = _pow10(kCoinDecimalPlaces);
  if (parts.length == 1) return whole * divisor;

  var fracStr = parts[1];
  if (fracStr.length > kCoinDecimalPlaces) return null;
  fracStr = fracStr.padRight(kCoinDecimalPlaces, '0');
  final frac = int.tryParse(fracStr);
  if (frac == null) return null;

  return whole * divisor + frac;
}

int _pow10(int exp) {
  var result = 1;
  for (var i = 0; i < exp; i++) {
    result *= 10;
  }
  return result;
}
