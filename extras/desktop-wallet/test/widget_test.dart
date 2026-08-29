import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:pluton_wallet/app/app.dart';
import 'package:pluton_wallet/core/api/models/transaction.dart';
import 'package:pluton_wallet/shared/utils/address_validator.dart';
import 'package:pluton_wallet/shared/utils/amount_formatter.dart';

void main() {
  testWidgets('App smoke test — builds without crashing', (tester) async {
    await tester.pumpWidget(const ProviderScope(child: PlutonApp()));
    // App starts on setup screen (no wallet open yet)
    expect(find.text('PLUTON'), findsWidgets);
  });

  group('Amount formatting', () {
    test('formats whole and fractional amounts', () {
      expect(formatAmount(0), '0.00');
      expect(formatAmount(100), '1.00');
      expect(formatAmount(150), '1.50');
      expect(formatAmount(100000), '1,000.00');
    });

    test('keeps the sign on sub-unit negatives', () {
      // `atomic ~/ divisor` truncates toward zero, so the whole part is 0 here
      // and the minus sign has to be carried explicitly.
      expect(formatAmount(-250), '-2.50');
      expect(formatAmount(-50), '-0.50');
      expect(formatAmount(-1), '-0.01');
    });
  });

  group('Amount parsing', () {
    test('parses valid input', () {
      expect(parseAmount('1.00'), 100);
      expect(parseAmount('1,000'), 100000);
    });

    test('rejects negatives rather than wrapping them into a uint64', () {
      expect(parseAmount('-5'), null);
      expect(parseAmount('-0.01'), null);
    });

    test('rejects excess precision rather than truncating it', () {
      expect(parseAmount('1.999'), null);
    });

    test('rejects empty and non-numeric input', () {
      expect(parseAmount(''), null);
      expect(parseAmount('   '), null);
      expect(parseAmount('abc'), null);
    });
  });

  group('Address validation', () {
    final validAddress = 'Wrkz${'a' * 94}';

    test('accepts a well-formed address', () {
      expect(isValidWrkzAddress(validAddress), isTrue);
    });

    test('rejects wrong prefix, length and alphabet', () {
      expect(isValidWrkzAddress('Xrkz${'a' * 94}'), isFalse);
      expect(isValidWrkzAddress('Wrkz${'a' * 93}'), isFalse);
      expect(isValidWrkzAddress('Wrkz${'a' * 93}0'), isFalse);
    });
  });

  group('Payment ID validation', () {
    test('accepts empty, 16 and 64 hex characters', () {
      expect(isValidPaymentId(''), isTrue);
      expect(isValidPaymentId('0' * 16), isTrue);
      expect(isValidPaymentId('aF' * 32), isTrue);
    });

    test('rejects other lengths and non-hex', () {
      expect(isValidPaymentId('0' * 15), isFalse);
      expect(isValidPaymentId('g' * 16), isFalse);
    });
  });

  group('Transaction direction', () {
    Transaction tx(int totalAmount, List<int> transferAmounts) =>
        Transaction.fromJson({
          'hash': 'h',
          'timestamp': 0,
          'blockHeight': 1,
          'totalAmount': totalAmount,
          'transfers': [
            for (final a in transferAmounts) {'amount': a, 'type': a >= 0 ? 1 : 0},
          ],
        });

    test('a plain receive is incoming', () {
      expect(tx(500, [500]).isIncoming, isTrue);
    });

    test('a plain send is outgoing', () {
      expect(tx(-500, [-500]).isIncoming, isFalse);
    });

    test('a send with change to another subwallet is still outgoing', () {
      // The positive transfer is this wallet's own change. Judging direction
      // from the individual transfers would call this incoming and fire a
      // bogus "WRKZ received" notification.
      expect(tx(-500, [-1500, 1000]).isIncoming, isFalse);
    });

    test('tolerates missing fields instead of throwing', () {
      final t = Transaction.fromJson({'totalAmount': 10});
      expect(t.hash, '');
      expect(t.transfers, isEmpty);
      expect(t.isIncoming, isTrue);
    });
  });
}
