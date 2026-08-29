import 'package:intl/intl.dart';
import '../../core/config/app_config.dart';

/// Why an amount string could not be parsed. Surfaced to the user instead of
/// silently coercing the value — a wallet must never guess what someone meant
/// to send.
enum AmountParseError {
  empty,
  invalidCharacters,
  negative,
  tooManyDecimals,
  tooLarge,
}

/// Outcome of parsing a user-entered amount.
class AmountParseResult {
  final int? atomic;
  final AmountParseError? error;

  const AmountParseResult.ok(int this.atomic) : error = null;
  const AmountParseResult.err(AmountParseError this.error) : atomic = null;

  bool get isValid => atomic != null;
}

/// Largest representable amount, guarding against overflow of the 64-bit
/// atomic-unit space when a user pastes something absurd.
const int _kMaxAtomic = 9007199254740991; // JS-safe integer ceiling

int _pow10(int exp) {
  var result = 1;
  for (var i = 0; i < exp; i++) {
    result *= 10;
  }
  return result;
}

/// Decimal/grouping separators for the app's active locale.
///
/// [Intl.defaultLocale] is set by the app whenever the user changes language,
/// so this tracks the UI rather than assuming en_US. Six of the nine shipped
/// locales (fr, de, es, pt, ru, vi) use "," as the decimal separator, where
/// treating it as a thousands separator turns "1,50" into 150 — a 100x error.
({String decimal, String group}) _separators() {
  try {
    final symbols = NumberFormat.decimalPattern(Intl.defaultLocale).symbols;
    return (decimal: symbols.DECIMAL_SEP, group: symbols.GROUP_SEP);
  } catch (_) {
    return (decimal: '.', group: ',');
  }
}

/// Converts atomic units (int) to a human-readable coin string.
/// WRKZ has 2 decimal places → 100 atomic units = 1.00 WRKZ
///
/// Grouping and the decimal mark follow the app's active locale.
String formatAmount(int atomic, {bool showTicker = false}) {
  final divisor = _pow10(kCoinDecimalPlaces);
  final negative = atomic < 0;
  final magnitude = atomic.abs();
  final whole = magnitude ~/ divisor;
  final frac = magnitude.remainder(divisor);
  final fracStr = frac.toString().padLeft(kCoinDecimalPlaces, '0');

  final sep = _separators();
  String wholeStr;
  try {
    wholeStr = NumberFormat.decimalPattern(Intl.defaultLocale).format(whole);
  } catch (_) {
    wholeStr = NumberFormat('#,##0', 'en_US').format(whole);
  }

  final formatted = '${negative ? '-' : ''}$wholeStr${sep.decimal}$fracStr';
  return showTicker ? '$formatted $kCoinTicker' : formatted;
}

/// Formats without grouping separators — for text fields, where a grouped
/// value the user cannot retype is worse than a plain one.
String formatAmountPlain(int atomic) {
  final divisor = _pow10(kCoinDecimalPlaces);
  final negative = atomic < 0;
  final magnitude = atomic.abs();
  final frac = magnitude.remainder(divisor).toString().padLeft(kCoinDecimalPlaces, '0');
  return '${negative ? '-' : ''}${magnitude ~/ divisor}${_separators().decimal}$frac';
}

/// True when [part] is a valid grouped integer using [sep] — `1`, `12`, `123`,
/// `1,234`, `12,345,678`. Anything else is not grouping.
bool _isGrouped(String part, String sep) {
  if (!part.contains(sep)) return RegExp(r'^\d+$').hasMatch(part);
  final groups = part.split(sep);
  if (groups.length < 2) return false;
  if (!RegExp(r'^\d{1,3}$').hasMatch(groups.first)) return false;
  // A grouped number never leads with a zero group — "0,001" is not something
  // anyone writes, which is exactly what separates it from "0.001".
  if (groups.first.startsWith('0')) return false;
  return groups.skip(1).every((g) => RegExp(r'^\d{3}$').hasMatch(g));
}

/// Parses a human-readable string (e.g. "1.50" or "1,50") into atomic units,
/// reporting *why* it failed rather than returning a silently wrong number.
///
/// Both "." and "," are accepted as the decimal mark, so a value formatted for
/// another locale still parses. Where that is genuinely ambiguous — a single
/// separator followed by exactly three digits, as in "1,234", which reads as
/// 1234 in English and 1.234 in German — the active locale breaks the tie, and
/// a value needing more precision than the coin has is rejected outright.
/// Guessing wrong here is a 1000x error in a send box.
AmountParseResult parseAmountChecked(String input) {
  var s = input.trim();
  if (s.isEmpty) return const AmountParseResult.err(AmountParseError.empty);

  // Strip spaces used as group separators (fr/ru use NBSP and thin space).
  s = s.replaceAll(RegExp(r'[\s   ]'), '');

  if (s.startsWith('-')) {
    return const AmountParseResult.err(AmountParseError.negative);
  }
  s = s.startsWith('+') ? s.substring(1) : s;
  if (s.isEmpty) return const AmountParseResult.err(AmountParseError.empty);

  if (!RegExp(r'^[0-9.,]+$').hasMatch(s)) {
    return const AmountParseResult.err(AmountParseError.invalidCharacters);
  }

  final dots = '.'.allMatches(s).length;
  final commas = ','.allMatches(s).length;

  String wholePart;
  String fracPart;

  if (dots == 0 && commas == 0) {
    wholePart = s;
    fracPart = '';
  } else if (dots > 0 && commas > 0) {
    // Mixed separators: the one appearing last is the decimal mark and the
    // other must form valid grouping ("1.234,56" / "1,234.56").
    final decimalChar = s.lastIndexOf('.') > s.lastIndexOf(',') ? '.' : ',';
    final groupChar = decimalChar == '.' ? ',' : '.';
    final at = s.lastIndexOf(decimalChar);
    if (s.substring(at + 1).contains(groupChar) ||
        decimalChar.allMatches(s).length > 1) {
      return const AmountParseResult.err(AmountParseError.invalidCharacters);
    }
    final head = s.substring(0, at);
    if (!_isGrouped(head, groupChar)) {
      return const AmountParseResult.err(AmountParseError.invalidCharacters);
    }
    wholePart = head.replaceAll(groupChar, '');
    fracPart = s.substring(at + 1);
  } else {
    final sep = dots > 0 ? '.' : ',';
    final count = dots > 0 ? dots : commas;
    final at = s.lastIndexOf(sep);
    final tail = s.substring(at + 1);

    if (count > 1) {
      // Repeated separators can only be grouping — "1,234,567".
      if (!_isGrouped(s, sep)) {
        return const AmountParseResult.err(AmountParseError.invalidCharacters);
      }
      wholePart = s.replaceAll(sep, '');
      fracPart = '';
    } else if (tail.length == 3 && _isGrouped(s, sep)) {
      // The ambiguous case. If this is the locale's grouping mark it means
      // thousands; if it is the decimal mark it means three decimal places,
      // which this coin cannot represent.
      if (sep == _separators().decimal) {
        return const AmountParseResult.err(AmountParseError.tooManyDecimals);
      }
      wholePart = s.replaceAll(sep, '');
      fracPart = '';
    } else {
      wholePart = at == 0 ? '0' : s.substring(0, at);
      fracPart = tail;
    }
  }

  if (!RegExp(r'^\d+$').hasMatch(wholePart)) {
    return const AmountParseResult.err(AmountParseError.invalidCharacters);
  }
  if (fracPart.isNotEmpty && !RegExp(r'^\d+$').hasMatch(fracPart)) {
    return const AmountParseResult.err(AmountParseError.invalidCharacters);
  }
  // Reject rather than truncate: quietly dropping digits from "1.999" would
  // send a different amount than the one shown on screen.
  if (fracPart.length > kCoinDecimalPlaces) {
    return const AmountParseResult.err(AmountParseError.tooManyDecimals);
  }

  final whole = int.tryParse(wholePart);
  if (whole == null) {
    // All digits, yet unparseable, means it overflowed the 64-bit range.
    return const AmountParseResult.err(AmountParseError.tooLarge);
  }
  final frac = fracPart.isEmpty
      ? 0
      : int.parse(fracPart.padRight(kCoinDecimalPlaces, '0'));

  final divisor = _pow10(kCoinDecimalPlaces);
  if (whole > (_kMaxAtomic - frac) ~/ divisor) {
    return const AmountParseResult.err(AmountParseError.tooLarge);
  }

  return AmountParseResult.ok(whole * divisor + frac);
}

/// Parses a human-readable string into atomic units.
/// Returns null if the input is invalid.
int? parseAmount(String input) => parseAmountChecked(input).atomic;
