import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:pluton_wallet/app/app.dart';
import 'package:pluton_wallet/core/api/models/transaction.dart';
import 'package:pluton_wallet/core/api/models/wallet_status.dart';
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

  // A lite node answers every scan from its own start height, so how the app
  // reads these two fields decides whether a wallet older than the node shows
  // a balance that is quietly wrong. See LITENODE.md.
  group('Lite node status', () {
    WalletStatus status({
      int liteStart = 0,
      int walletStart = 0,
      int walletStartTimestamp = 0,
      bool stalled = false,
    }) =>
        WalletStatus.fromJson({
          'walletBlockCount': 4200000,
          'localDaemonBlockCount': 4200000,
          'networkBlockCount': 4200000,
          'isDaemonSynced': true,
          'isWalletSynced': true,
          'isOutOfSync': false,
          'peerCount': 8,
          'hashrate': 0,
          'isViewWallet': false,
          'subWalletCount': 1,
          'daemonLiteStartHeight': liteStart,
          'isSyncStalledByLiteNode': stalled,
          'walletSyncStartHeight': walletStart,
          'walletSyncStartTimestamp': walletStartTimestamp,
        });

    test('a daemon holding the whole chain is not a lite node', () {
      final s = status(walletStart: 4000000);
      expect(s.isLiteNode, isFalse);
      expect(s.liteNodeMissesWalletHistory, isFalse);
    });

    test('a node starting above the wallet hides part of its history', () {
      final s = status(liteStart: 4100000, walletStart: 4000000);
      expect(s.isLiteNode, isTrue);
      expect(s.liteNodeMissesWalletHistory, isTrue);
    });

    test('a node starting at or below the wallet covers it', () {
      expect(
          status(liteStart: 4000000, walletStart: 4000000)
              .liteNodeMissesWalletHistory,
          isFalse);
      expect(
          status(liteStart: 3900000, walletStart: 4000000)
              .liteNodeMissesWalletHistory,
          isFalse);
    });

    test('a wallet still carrying a timestamp claims nothing either way', () {
      // The timestamp only resolves to a height once the first sync response
      // arrives. Until then there is no height to compare, and guessing zero
      // would flag every lite node as hiding history.
      final s = status(liteStart: 4100000, walletStartTimestamp: 1690000000);
      expect(s.walletEarliestHeight, isNull);
      expect(s.liteNodeMissesWalletHistory, isFalse);
    });

    test('a wallet scanned from genesis is covered by no lite node', () {
      final s = status(liteStart: 1, walletStart: 0);
      expect(s.walletEarliestHeight, 0);
      expect(s.liteNodeMissesWalletHistory, isTrue);
    });

    test('fields absent from an older wallet_capi read as a full node', () {
      final s = WalletStatus.fromJson({
        'walletBlockCount': 1,
        'localDaemonBlockCount': 1,
        'networkBlockCount': 1,
        'isDaemonSynced': true,
        'isWalletSynced': true,
        'isOutOfSync': false,
        'peerCount': 0,
        'hashrate': 0,
        'isViewWallet': false,
        'subWalletCount': 1,
      });
      expect(s.isLiteNode, isFalse);
      expect(s.isSyncStalledByLiteNode, isFalse);
      expect(s.liteNodeMissesWalletHistory, isFalse);
    });
  });
}
