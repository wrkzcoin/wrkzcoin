import 'package:intl/intl.dart';
import '../../core/config/app_config.dart';

/// Converts atomic units (int) to a human-readable coin string.
/// WRKZ has 2 decimal places → 100 atomic units = 1.00 WRKZ
String formatAmount(int atomic, {bool showTicker = false}) {
  final divisor = _pow10(kCoinDecimalPlaces);
  final whole = atomic ~/ divisor;
  final frac = atomic.remainder(divisor).abs();
  final fracStr = frac.toString().padLeft(kCoinDecimalPlaces, '0');
  final formatted = '${NumberFormat('#,##0', 'en_US').format(whole)}.$fracStr';
  return showTicker ? '$formatted $kCoinTicker' : formatted;
}

/// Parses a human-readable string (e.g. "1.50") into atomic units.
/// Returns null if the input is invalid.
int? parseAmount(String input) {
  final cleaned = input.replaceAll(',', '').trim();
  final parts = cleaned.split('.');
  if (parts.isEmpty || parts.length > 2) return null;
  final whole = int.tryParse(parts[0]);
  if (whole == null) return null;
  final divisor = _pow10(kCoinDecimalPlaces);
  if (parts.length == 1) return whole * divisor;
  final fracRaw = parts[1].substring(
      0, parts[1].length.clamp(0, kCoinDecimalPlaces));
  final frac = int.tryParse(fracRaw.padRight(kCoinDecimalPlaces, '0'));
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
