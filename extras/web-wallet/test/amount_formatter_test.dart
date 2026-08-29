import 'package:flutter_test/flutter_test.dart';
import 'package:intl/date_symbol_data_local.dart';
import 'package:intl/intl.dart';
import 'package:pluton_web_wallet/core/config/app_config.dart';
import 'package:pluton_web_wallet/shared/utils/amount_formatter.dart';

void main() {
  setUpAll(() async {
    await initializeDateFormatting();
  });

  tearDown(() {
    Intl.defaultLocale = null;
  });

  group('parseAmountChecked — plain values', () {
    test('whole numbers', () {
      expect(parseAmount('1'), 100);
      expect(parseAmount('0'), 0);
      expect(parseAmount('12345'), 1234500);
    });

    test('decimals', () {
      expect(parseAmount('1.5'), 150);
      expect(parseAmount('1.50'), 150);
      expect(parseAmount('0.01'), 1);
      expect(parseAmount('1.05'), 105);
    });

    test('surrounding whitespace is ignored', () {
      expect(parseAmount('  2.50  '), 250);
    });
  });

  group('locale decimal separators', () {
    // Six of the nine shipped locales use "," as the decimal mark. Treating it
    // as a thousands separator turned "1,50" into 150 — a 100x overpayment.
    test('comma decimal parses as a fraction, not thousands', () {
      expect(parseAmount('1,50'), 150);
      expect(parseAmount('0,01'), 1);
    });

    test('comma grouping still parses as grouping', () {
      // Three trailing digits cannot be a fraction for a 2-decimal coin.
      expect(parseAmount('1,234'), 123400);
      expect(parseAmount('1,234,567'), 123456700);
    });

    test('mixed separators: the last one is the decimal mark', () {
      expect(parseAmount('1.234,56'), 123456); // de/es/pt style
      expect(parseAmount('1,234.56'), 123456); // en style
    });

    test('non-breaking and thin spaces used for grouping', () {
      expect(parseAmount('1 234,56'), 123456); // fr/ru style
      expect(parseAmount('1 234.56'), 123456);
    });
  });

  group('rejections', () {
    test('empty', () {
      expect(parseAmountChecked('').error, AmountParseError.empty);
      expect(parseAmountChecked('   ').error, AmountParseError.empty);
    });

    test('negative values are rejected, not silently flipped', () {
      // "-0.50" used to parse to +50 because int.tryParse('-0') is 0.
      expect(parseAmountChecked('-0.50').error, AmountParseError.negative);
      expect(parseAmountChecked('-1.50').error, AmountParseError.negative);
    });

    test('too many decimals is an error, not a silent truncation', () {
      // "1.999" previously became 1.99 with no indication to the user.
      expect(parseAmountChecked('1.999').error, AmountParseError.tooManyDecimals);
      expect(parseAmountChecked('0.001').error, AmountParseError.tooManyDecimals);
    });

    test('letters and symbols', () {
      expect(parseAmountChecked('abc').error, AmountParseError.invalidCharacters);
      expect(parseAmountChecked('1e3').error, AmountParseError.invalidCharacters);
      expect(parseAmountChecked('1.2.3').error, AmountParseError.invalidCharacters);
    });

    test('absurdly large values do not overflow', () {
      expect(parseAmountChecked('999999999999999999999').error,
          AmountParseError.tooLarge);
    });
  });

  group('formatAmount', () {
    test('round-trips through parseAmount', () {
      for (final atomic in [0, 1, 99, 100, 12345, 100000000]) {
        expect(parseAmount(formatAmount(atomic)), atomic,
            reason: 'failed for $atomic');
      }
    });

    test('always shows the full fractional part', () {
      expect(formatAmount(100), '1.00');
      expect(formatAmount(105), '1.05');
      expect(formatAmount(1), '0.01');
    });

    test('negative amounts keep their sign', () {
      expect(formatAmount(-150), '-1.50');
    });

    test('ticker suffix', () {
      expect(formatAmount(100, showTicker: true), '1.00 $kCoinTicker');
    });

    test('follows the active locale', () {
      Intl.defaultLocale = 'de';
      expect(formatAmount(123456), '1.234,56');
      Intl.defaultLocale = 'en';
      expect(formatAmount(123456), '1,234.56');
    });

    test('formatAmountPlain omits grouping so the value can be retyped', () {
      Intl.defaultLocale = 'en';
      expect(formatAmountPlain(123456789), '1234567.89');
    });
  });
}
