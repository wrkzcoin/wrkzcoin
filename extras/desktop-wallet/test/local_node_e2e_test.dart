/// End-to-end test for the local lite node supervisor.
///
/// This one starts a real `Wrkzd` against the real network, so it is opt-in:
/// it is skipped unless `WRKZ_LOCAL_NODE_E2E=1` is set. Nothing else in the
/// suite touches the network or the disk outside a temp directory.
///
///   set WRKZ_LOCAL_NODE_E2E=1
///   set WRKZ_DAEMON_PATH=...\build\src\Release\Wrkzd.exe
///   flutter test test/local_node_e2e_test.dart
///
/// A daemon whose RocksDB was linked without ZSTD exits at startup unless it
/// is told not to compress, which is what `WRKZ_DAEMON_EXTRA_ARGS` is for:
///
///   set WRKZ_DAEMON_EXTRA_ARGS=--db-enable-compression=false
library;

import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:pluton_wallet/core/node/local_node.dart';
import 'package:pluton_wallet/core/node/local_node_controller.dart';

/// A height well below any current tip, so the node is a genuine lite node
/// rather than one that refuses to start.
const int _kLiteHeight = 4000000;

/// Polls [check] until it holds, [limit] passes, or [abortIf] goes true.
///
/// The abort matters: a daemon that cannot start exits in under a second, and
/// without it every such failure costs the full timeout before the reason is
/// printed.
Future<bool> waitFor(
  bool Function() check, {
  bool Function()? abortIf,
  Duration limit = const Duration(seconds: 90),
  Duration every = const Duration(milliseconds: 500),
}) async {
  final deadline = DateTime.now().add(limit);
  while (DateTime.now().isBefore(deadline)) {
    if (check()) return true;
    if (abortIf != null && abortIf()) return false;
    await Future<void>.delayed(every);
  }
  return check();
}

void main() {
  final enabled = Platform.environment['WRKZ_LOCAL_NODE_E2E'] == '1';
  final skip = enabled ? null : 'set WRKZ_LOCAL_NODE_E2E=1 to run';

  group('Local node supervisor', () {
    late Directory temp;
    late ProviderContainer container;

    setUp(() {
      temp = Directory.systemTemp.createTempSync('wrkz-node-e2e');
      LocalNodePaths.bindDataDirectory(temp.path);
      container = ProviderContainer();
    });

    tearDown(() async {
      // Never leave a daemon behind, whatever the test did.
      try {
        await container.read(localNodeProvider.notifier).stop();
      } catch (_) {}
      container.dispose();
      if (temp.existsSync()) {
        try {
          temp.deleteSync(recursive: true);
        } catch (_) {}
      }
    });

    test('finds the daemon binary', () {
      final daemon = LocalNodePaths.findDaemon();
      expect(daemon, isNotNull,
          reason: 'set WRKZ_DAEMON_PATH, or put ${LocalNodePaths.daemonName} '
              'next to the test runner');
      expect(daemon!.existsSync(), isTrue);
    }, skip: skip);

    test(
      'starts, answers RPC, reaches the network, stops and deletes',
      () async {
        final node = container.read(localNodeProvider.notifier);

        expect(
            await waitFor(() =>
                container.read(localNodeProvider).phase !=
                LocalNodePhase.loading),
            isTrue);
        expect(container.read(localNodeProvider).phase,
            LocalNodePhase.unconfigured);

        await node.create(liteHeight: _kLiteHeight);

        // Up and answering. The config is what the daemon was launched with,
        // so a mismatch here means the arguments never took.
        expect(
          await waitFor(
            () =>
                container.read(localNodeProvider).phase ==
                LocalNodePhase.running,
            abortIf: () =>
                container.read(localNodeProvider).phase ==
                LocalNodePhase.failed,
          ),
          isTrue,
          reason: 'node never answered its RPC: '
              '${container.read(localNodeProvider).error}',
        );
        expect(container.read(localNodeProvider).config!.liteHeight,
            _kLiteHeight);

        // The daemon agrees it is a lite node at the height it was given.
        // --dump-config cannot show this: it omits `lite` entirely, so this
        // round trip through /info is the only proof the flags took effect.
        expect(
          await waitFor(() =>
              container.read(localNodeProvider).reportedLiteStartHeight > 0),
          isTrue,
          reason: 'daemon did not report itself as a lite node',
        );
        expect(container.read(localNodeProvider).reportedLiteStartHeight,
            _kLiteHeight);

        // Talking to the real network, which is the part that proves the p2p
        // port and the data directory are usable rather than merely accepted.
        expect(
          await waitFor(
            () => container.read(localNodeProvider).peers > 0,
            limit: const Duration(seconds: 120),
          ),
          isTrue,
          reason: 'node never found a peer',
        );

        // A node this far behind must not be offered to the wallet.
        expect(container.read(localNodeProvider).canServeWallet, isFalse);

        await node.stop();
        expect(container.read(localNodeProvider).phase,
            LocalNodePhase.stopped);
        // A stop asked for is not a crash.
        expect(container.read(localNodeProvider).error, isNull);
        // And the port is genuinely free again.
        final probe = await ServerSocket.bind(InternetAddress.loopbackIPv4,
            container.read(localNodeProvider).config!.rpcPort);
        await probe.close();

        await node.destroy();
        expect(container.read(localNodeProvider).phase,
            LocalNodePhase.unconfigured);
        expect(Directory(temp.path).existsSync(), isFalse);
      },
      timeout: const Timeout(Duration(minutes: 6)),
      skip: skip,
    );

    test(
      'a second app instance adopts a running node instead of starting one',
      () async {
        final first = container.read(localNodeProvider.notifier);
        await first.create(liteHeight: _kLiteHeight);
        expect(
          await waitFor(
            () =>
                container.read(localNodeProvider).phase ==
                LocalNodePhase.running,
            abortIf: () =>
                container.read(localNodeProvider).phase ==
                LocalNodePhase.failed,
          ),
          isTrue,
          reason: 'node never started: '
              '${container.read(localNodeProvider).error}',
        );
        final port = container.read(localNodeProvider).config!.rpcPort;

        // A fresh container is what a relaunched app looks like: same data
        // directory, no Process handle, a daemon already on the port.
        final second = ProviderContainer();
        addTearDown(second.dispose);
        final adopted = second.read(localNodeProvider.notifier);

        expect(
          await waitFor(() =>
              second.read(localNodeProvider).phase == LocalNodePhase.running),
          isTrue,
          reason: 'the running node was not adopted',
        );
        expect(second.read(localNodeProvider).config!.rpcPort, port);

        // Only one daemon should be holding the database — a second would have
        // died on RocksDB's lock and reported failed.
        expect(second.read(localNodeProvider).phase, LocalNodePhase.running);

        // The adopted node can be stopped by pid, with no Process to wait on.
        await adopted.stop();
        expect(
            second.read(localNodeProvider).phase, LocalNodePhase.stopped);
        final probe =
            await ServerSocket.bind(InternetAddress.loopbackIPv4, port);
        await probe.close();
      },
      timeout: const Timeout(Duration(minutes: 6)),
      skip: skip,
    );
  });
}
