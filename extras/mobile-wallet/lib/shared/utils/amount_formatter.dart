import 'package:intl/intl.dart';

import '../../core/config/app_config.dart';

final _numberFormat = NumberFormat('#,##0', 'en_US');

/// Converts atomic units to a human-readable string.
/// 100 atomic = 1.00 WRKZ (2 decimal places).
String formatAmount(int atomic, {bool showTicker = false}) {
  final whole = atomic ~/ AppConfig.atomicDivisor;
  final frac = atomic.remainder(AppConfig.atomicDivisor).abs();
  final fracStr = frac.toString().padLeft(AppConfig.decimalPlaces, '0');
  final formatted = '${_numberFormat.format(whole)}.$fracStr';
  return showTicker ? '$formatted ${AppConfig.ticker}' : formatted;
}

/// Parses a human-readable amount string to atomic units.
/// Returns null if the string is invalid.
int? parseAmount(String input) {
  if (input.trim().isEmpty) return null;
  final cleaned = input.replaceAll(',', '').trim();
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

/// Validates a WRKZ address.
bool isValidWrkzAddress(String address) {
  if (!AppConfig.validAddressLengths.contains(address.length)) return false;
  if (!address.startsWith(AppConfig.addressPrefix)) return false;
  return RegExp(
          r'^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$')
      .hasMatch(address);
}
