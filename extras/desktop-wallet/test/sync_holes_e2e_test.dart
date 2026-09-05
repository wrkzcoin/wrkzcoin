/// sync_holes_e2e_test.dart
///
/// Syncs a throwaway wallet across a stretch of chain the daemon answers with
/// holes in, and checks it gets to the other side.
///
/// A block whose only transaction is its coinbase carries nothing for a wallet
/// that asked for coinbases to be left out, so daemons omit the whole block.
/// The wallet used to predict that from an advertised feature flag rather than
/// from what it had asked for, and got it wrong for every daemon older than the
/// flag — which is most of the public network. The first omitted block landing
/// where the wallet expected its next one was read as "this daemon cannot serve
/// me", and sync stopped for good with the balance declared incomplete.
///
/// Measured against nodes.wrkz.work (v0.4.6, advertising no sync_features) at
/// the time this was written: a request from 4202339 with
/// skipCoinbaseTransactions comes back 4202340, 4202342, 4202344, 4202346,
/// 4202352 — while the same request without it comes back contiguous.
///
/// Skipped unless `WRKZ_SYNC_HOLES_E2E=1`, because it talks to a real daemon
/// over the network. `wallet_capi.dll` must be beside the test runner.
library;

import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:path/path.dart' as p;
import 'package:pluton_wallet/core/ffi/wallet_ffi.dart';

const _password = 'sync hole test';

/// A public full node. Overridable so this can be pointed at a daemon whose
/// behaviour is being investigated.
String get _host =>
    Platform.environment['WRKZ_E2E_DAEMON_HOST'] ?? 'nodes.wrkz.work';

int get _port =>
    int.tryParse(Platform.environment['WRKZ_E2E_DAEMON_PORT'] ?? '') ?? 17856;

/// How far back to start. Far enough to contain coinbase-only blocks, close
/// enough to the tip that the scan finishes inside the timeout.
const _blocksBack = 400;

/// A wallet that has to cross the holes has to be given time to. The scan is a
/// few hundred blocks, but the daemon is a public one and pauses.
const _syncBudget = Duration(minutes: 4);

void main() {
  final enabled = Platform.environment['WRKZ_SYNC_HOLES_E2E'] == '1';
  final skip = enabled ? null : 'set WRKZ_SYNC_HOLES_E2E=1 to run';

  test('a wallet syncs through blocks the daemon leaves out', () async {
    final temp = await Directory.systemTemp.createTemp('wrkz-sync-holes-');
    final seedWallet = p.join(temp.path, 'seed.wallet');
    final scanWallet = p.join(temp.path, 'scan.wallet');

    // A throwaway wallet, only so there is a seed to restore from at a height
    // of our choosing. Nothing is ever sent to it.
    final maker = WalletCApi();
    await maker.create(seedWallet, _password, _host, _port, syncThreads: 1);
    final seed = await maker.getMnemonicSeed();
    final tip =
        (await maker.getStatusJson())['networkBlockCount'] as int? ?? 0;
    await maker.close();

    expect(tip, greaterThan(0),
        reason: 'the daemon should have reported a network height');

    final from = tip - _blocksBack;

    final wallet = WalletCApi();
    await wallet.restoreFromSeed(seed, scanWallet, _password, _host, _port,
        scanHeight: from, syncThreads: 1);

    var height = 0;
    var stalled = false;
    final deadline = DateTime.now().add(_syncBudget);

    while (DateTime.now().isBefore(deadline)) {
      final status = await wallet.getStatusJson();
      height = status['walletBlockCount'] as int? ?? 0;
      stalled = status['isSyncStalledByLiteNode'] as bool? ?? false;

      // Sync is done once the wallet is level with the daemon. Getting there
      // at all is the whole point: every one of those few hundred blocks that
      // held only a coinbase was a chance to stop.
      if (stalled || height >= (status['networkBlockCount'] as int? ?? 0)) {
        break;
      }
      await Future<void>.delayed(const Duration(seconds: 2));
    }

    await wallet.close();
    try {
      await temp.delete(recursive: true);
    } catch (_) {
      // Windows sometimes still holds the directory briefly.
    }

    expect(stalled, isFalse,
        reason: 'sync stopped over holes the wallet itself asked for');
    expect(height, greaterThan(from),
        reason: 'the wallet should have advanced past its start height');
  }, timeout: const Timeout(Duration(minutes: 6)), skip: skip);
}
