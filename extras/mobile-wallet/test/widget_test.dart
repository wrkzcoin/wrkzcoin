import 'package:flutter_test/flutter_test.dart';
import 'package:pluton_mobile/shared/utils/amount_formatter.dart';

void main() {
  group('Amount formatting', () {
    test('formats zero', () {
      expect(formatAmount(0), '0.00');
    });

    test('formats whole amounts', () {
      expect(formatAmount(100), '1.00');
      expect(formatAmount(100000), '1,000.00');
    });

    test('formats fractional amounts', () {
      expect(formatAmount(150), '1.50');
      expect(formatAmount(1), '0.01');
      expect(formatAmount(99), '0.99');
    });

    test('formats with ticker', () {
      expect(formatAmount(100, showTicker: true), '1.00 WRKZ');
    });

    test('formats negative (absolute)', () {
      expect(formatAmount(-250), '-2.50');
    });
  });

  group('Amount parsing', () {
    test('parses simple amounts', () {
      expect(parseAmount('1.00'), 100);
      expect(parseAmount('0.50'), 50);
      expect(parseAmount('1000'), 100000);
    });

    test('parses with commas', () {
      expect(parseAmount('1,000.00'), 100000);
    });

    test('returns null for invalid input', () {
      expect(parseAmount(''), null);
      expect(parseAmount('abc'), null);
      expect(parseAmount('1.234'), null);
    });
  });
}
