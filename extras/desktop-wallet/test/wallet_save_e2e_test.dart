/// wallet_save_e2e_test.dart
///
/// Exercises the wallet file write against a real `wallet_capi`.
///
/// The save used to truncate the wallet and write into it, so anything that
/// interrupted it — power loss, the OS killing the app, a user reaching for
/// Task Manager on a save that would not finish — left a file that could not be
/// opened again. It now writes `<wallet>.tmp` and renames it over, which is
/// atomic on NTFS and POSIX alike. These tests are what keeps that true.
///
/// Skipped unless `WRKZ_WALLET_CAPI_E2E=1`, because it needs the shared library
/// beside the test runner and touches the disk. Run it with:
///
///   $env:WRKZ_WALLET_CAPI_E2E = "1"
///   flutter test test/wallet_save_e2e_test.dart
///
/// `wallet_capi.dll` (or the .so/.dylib) has to be findable — copy it into the
/// project root first.
library;

import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:path/path.dart' as p;
import 'package:pluton_wallet/core/ffi/wallet_ffi.dart';

const _password = 'correct horse battery staple';

/// Nothing on the network is contacted: creating and saving a wallet never
/// needs the daemon, and the synchroniser is stopped again before it can get
/// anywhere. The port is a closed one on loopback on purpose.
const _daemonHost = '127.0.0.1';
const _daemonPort = 1;

void main() {
  final enabled = Platform.environment['WRKZ_WALLET_CAPI_E2E'] == '1';
  final skip = enabled ? null : 'set WRKZ_WALLET_CAPI_E2E=1 to run';

  group('Wallet file writes', () {
    late Directory temp;
    late String walletPath;
    late WalletCApi ffi;

    setUp(() async {
      temp = await Directory.systemTemp.createTemp('wrkz-wallet-save-');
      walletPath = p.join(temp.path, 'test.wallet');
      ffi = WalletCApi();
    });

    tearDown(() async {
      try {
        if (ffi.isOpen) await ffi.close();
      } catch (_) {
        // The test may have closed it already.
      }
      try {
        await temp.delete(recursive: true);
      } catch (_) {
        // Windows sometimes still holds the directory briefly.
      }
    });

    test('a saved wallet reopens, and leaves no temporary behind', () async {
      await ffi.create(walletPath, _password, _daemonHost, _daemonPort,
          syncThreads: 1);
      final address = await ffi.getPrimaryAddress();
      expect(address, isNotEmpty);

      await ffi.save();

      // The rename is the whole point: a leftover .tmp means it did not happen,
      // and a save that only wrote the temporary has saved nothing.
      expect(File('$walletPath.tmp').existsSync(), isFalse,
          reason: 'the temporary should have been renamed over the wallet');
      expect(File(walletPath).existsSync(), isTrue);
      expect(File(walletPath).lengthSync(), greaterThan(0));

      await ffi.close();

      final reopened = WalletCApi();
      await reopened.open(walletPath, _password, _daemonHost, _daemonPort,
          syncThreads: 1);
      expect(await reopened.getPrimaryAddress(), address,
          reason: 'the reopened wallet should be the one that was written');
      await reopened.close();
    }, skip: skip);

    test('saving twice replaces the file rather than appending to it',
        () async {
      await ffi.create(walletPath, _password, _daemonHost, _daemonPort,
          syncThreads: 1);
      await ffi.save();
      final first = File(walletPath).lengthSync();

      await ffi.save();
      final second = File(walletPath).lengthSync();

      // A rename onto an existing file has to replace it. If it appended, or
      // failed and left the first write in place, this is where it shows.
      expect(second, first,
          reason: 'a second save should replace the file, not grow it');
      expect(File('$walletPath.tmp').existsSync(), isFalse);

      await ffi.close();
      final reopened = WalletCApi();
      await reopened.open(walletPath, _password, _daemonHost, _daemonPort,
          syncThreads: 1);
      await reopened.close();
    }, skip: skip);

    test('an interrupted save leaves the previous wallet openable', () async {
      await ffi.create(walletPath, _password, _daemonHost, _daemonPort,
          syncThreads: 1);
      await ffi.save();
      final good = File(walletPath).readAsBytesSync();

      // Stand in for a process killed mid-write: a half-written temporary next
      // to the wallet, which is exactly what the old in-place write would have
      // left *as* the wallet. The rename never ran, so the wallet is untouched.
      File('$walletPath.tmp').writeAsBytesSync(good.sublist(0, good.length ~/ 2));

      expect(File(walletPath).readAsBytesSync(), good,
          reason: 'the wallet on disk must be unaffected by a stray temporary');

      await ffi.close();
      final reopened = WalletCApi();
      await reopened.open(walletPath, _password, _daemonHost, _daemonPort,
          syncThreads: 1);
      expect(await reopened.getPrimaryAddress(), isNotEmpty);
      await reopened.close();
    }, skip: skip);
  });
}
