/// local_node_controller.dart
///
/// Owns the local lite node's child process: starting it, polling its RPC,
/// stopping it, and deleting it. One instance for the app.
library;

import 'dart:async';
import 'dart:collection';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'local_node.dart';

/// How often the node's RPC is polled while it is up.
const Duration _kPollInterval = Duration(seconds: 3);

/// How long a graceful stop is given before the process is killed outright.
///
/// A clean shutdown flushes RocksDB and syncs its WAL. Killing instead is not
/// dangerous — writes already in the WAL are in the OS's hands, so RocksDB
/// replays them on the next open — it just costs recovery time at startup.
const Duration _kStopGrace = Duration(seconds: 45);

/// Lines of daemon output kept for the failure display.
const int _kLogTailLines = 40;

class LocalNodeController extends Notifier<LocalNodeState> {
  Process? _process;

  /// Set when the running node was found on disk rather than started by this
  /// app instance, in which case there is no [Process] to wait on and it can
  /// only be signalled by pid.
  int? _adoptedPid;

  /// A stop this app asked for is in flight, so the process exiting is
  /// expected rather than a crash to report.
  bool _stopping = false;

  Timer? _poll;

  final Queue<String> _logTail = Queue<String>();

  final HttpClient _http = HttpClient()
    ..connectionTimeout = const Duration(seconds: 3);

  @override
  LocalNodeState build() {
    ref.onDispose(() {
      _poll?.cancel();
      _http.close(force: true);
      // The process is deliberately left running: it is supervised across app
      // restarts, and killing a half-synced node on every hot reload would be
      // worse than leaving one to adopt.
    });
    unawaited(_init());
    return const LocalNodeState();
  }

  Future<void> _init() async {
    final config = await readLocalNodeConfig();
    if (config == null) {
      state = const LocalNodeState(phase: LocalNodePhase.unconfigured);
      return;
    }

    // A node left behind by a crashed or force-quit app is still serving on
    // the same port. Adopt it rather than starting a second daemon on the same
    // database, which the first one holds a RocksDB lock on anyway.
    //
    // The port alone does not identify it — a stored port can be taken by
    // anything after a reboot, including a full node the user runs themselves.
    // Only a daemon reporting this config's own lite height is ours; anything
    // else is left alone, and start() then fails honestly on the bind.
    final alive = await _probeOurNode(config);
    if (alive != null) {
      _adoptedPid = await _readPidFile();
      state = LocalNodeState(
        phase: LocalNodePhase.running,
        config: config,
      );
      _applyInfo(alive);
      _startPolling();
      return;
    }

    await _clearPidFile();
    state = LocalNodeState(phase: LocalNodePhase.stopped, config: config);
    unawaited(_refreshDiskUsage());

    // A first sync runs for hours, so a node the user left running comes back
    // up on its own. One that was stopped on purpose stays stopped.
    if (config.autoStart) {
      await start();
    }
  }

  // ── lifecycle ──────────────────────────────────────────────────────────

  /// Creates the node: picks ports, records the start height, and starts it.
  ///
  /// The height cannot be changed afterwards without deleting the database —
  /// the daemon refuses to start against a mismatch — so this is the one
  /// decision the caller has to get right.
  Future<void> create({required int liteHeight}) async {
    if (liteHeight <= 0) {
      throw ArgumentError.value(liteHeight, 'liteHeight', 'must be above zero');
    }
    if (state.isConfigured) {
      throw StateError('a local node is already configured');
    }

    final config = LocalNodeConfig(
      liteHeight: liteHeight,
      rpcPort: await pickFreePort(),
      p2pPort: await pickFreePort(),
      createdAt: DateTime.now(),
    );

    await Directory(LocalNodePaths.dataDir).create(recursive: true);
    await writeLocalNodeConfig(config);
    // Remember where it went. The default is resolved fresh at every launch and
    // can move — a portable copy relocated, a name that was free last time
    // taken since — and an unremembered node is one the next launch offers to
    // sync again from nothing.
    await writeNodeDataDirPref(LocalNodePaths.dataDir);

    state = LocalNodeState(phase: LocalNodePhase.stopped, config: config);
    await start();
  }

  Future<void> start() async {
    final config = state.config;
    if (config == null) throw StateError('no local node is configured');
    if (state.isRunning) return;

    await _setAutoStart(true);

    final daemon = LocalNodePaths.findDaemon();
    if (daemon == null) {
      state = state.copyWith(
        phase: LocalNodePhase.failed,
        error: 'daemon-not-found',
      );
      return;
    }

    _logTail.clear();
    state = state.copyWith(phase: LocalNodePhase.starting, clearError: true);

    try {
      final process = await Process.start(
        daemon.path,
        config.arguments(LocalNodePaths.dataDir),
        workingDirectory: LocalNodePaths.dataDir,
      );
      _process = process;
      _adoptedPid = null;
      await _writePidFile(process.pid);

      // Both streams have to be drained or the daemon blocks on a full pipe
      // once it has written a few kilobytes of startup output.
      _drain(process.stdout);
      _drain(process.stderr);

      unawaited(process.exitCode.then(_onExit));
    } catch (e) {
      state = state.copyWith(
        phase: LocalNodePhase.failed,
        error: e.toString(),
      );
      return;
    }

    _startPolling();
  }

  Future<void> stop() async {
    await _setAutoStart(false);

    // The exitCode handler fires before this method gets to set the phase, so
    // it has to be told this exit was asked for or it reports it as a crash.
    _stopping = true;
    _poll?.cancel();
    _poll = null;

    final config = state.config;
    final process = _process;
    final pid = process?.pid ?? _adoptedPid;

    if (pid == null || config == null) {
      _stopping = false;
      state = state.copyWith(
          phase: LocalNodePhase.stopped, synced: false, clearError: true);
      return;
    }

    // SIGINT is what the daemon installs a handler for, and it runs the same
    // shutdown the console 'exit' command does. Windows has no real SIGINT for
    // a child of a GUI process — dart:io maps it onto TerminateProcess — so
    // there it is simply a hard stop, which the WAL makes safe.
    try {
      if (process != null) {
        process.kill(ProcessSignal.sigint);
      } else {
        Process.killPid(pid, ProcessSignal.sigint);
      }
    } catch (_) {
      // Already gone.
    }

    if (process != null) {
      await process.exitCode.timeout(_kStopGrace, onTimeout: () {
        process.kill(ProcessSignal.sigkill);
        return -1;
      });
    } else {
      // An adopted node has no Process to wait on, so the port is the only
      // signal that it is going down. It stops answering early in the shutdown,
      // while RocksDB is still flushing — killing on that would interrupt
      // exactly the clean close the SIGINT asked for. Worse, once the process
      // has fully exited the pid is free to be reused, and the kill would land
      // on a stranger. So only the timeout kills.
      final closed = await _waitForPortToClose(config.rpcPort, _kStopGrace);
      if (!closed) {
        try {
          Process.killPid(pid, ProcessSignal.sigkill);
        } catch (_) {
          // It exited between the last probe and here.
        }
      }
    }

    _process = null;
    _adoptedPid = null;
    _stopping = false;
    await _clearPidFile();
    state = state.copyWith(
        phase: LocalNodePhase.stopped, synced: false, clearError: true);
    unawaited(_refreshDiskUsage());
  }

  /// Stops the node and deletes its blockchain database and configuration.
  ///
  /// The wallet is untouched. What is lost is the sync — a new node starts
  /// again from nothing, which is hours.
  Future<void> destroy() async {
    if (state.isRunning) await stop();

    final dir = Directory(LocalNodePaths.dataDir);
    if (await dir.exists()) {
      await dir.delete(recursive: true);
    }

    state = const LocalNodeState(phase: LocalNodePhase.unconfigured);
  }

  // ── polling ────────────────────────────────────────────────────────────

  void _startPolling() {
    _poll?.cancel();
    _poll = Timer.periodic(_kPollInterval, (_) => unawaited(refresh()));
    unawaited(refresh());
  }

  Future<void> refresh() async {
    final config = state.config;
    if (config == null) return;

    final info = await _probe(config.rpcPort);
    if (info == null) {
      // Not answering yet is normal while it opens the database, which on a
      // large one takes a while. Only an exited process is a failure, and
      // that arrives through _onExit.
      return;
    }

    if (state.phase == LocalNodePhase.starting) {
      state = state.copyWith(phase: LocalNodePhase.running, clearError: true);
    }
    _applyInfo(info);
    unawaited(_refreshDiskUsage());
  }

  void _applyInfo(Map<String, dynamic> info) {
    final height = (info['height'] as num?)?.toInt() ?? 0;
    final network = (info['network_height'] as num?)?.toInt() ?? 0;
    final incoming = (info['incoming_connections_count'] as num?)?.toInt() ?? 0;
    final outgoing = (info['outgoing_connections_count'] as num?)?.toInt() ?? 0;

    state = state.copyWith(
      height: height,
      networkHeight: network,
      peers: incoming + outgoing,
      // Trust the daemon's own answer rather than comparing heights here: it
      // is the same comparison, made where the numbers come from.
      synced: info['synced'] as bool? ?? (height > 0 && height == network),
      // Absent from /height, so only overwrite when /info actually answered.
      reportedLiteStartHeight: (info['lite_start_height'] as num?)?.toInt(),
    );
  }

  Future<void> _refreshDiskUsage() async {
    final bytes = await LocalNodePaths.directorySize(LocalNodePaths.dataDir);
    if (bytes != state.diskBytes) {
      state = state.copyWith(diskBytes: bytes);
    }
  }

  /// One `/info` call, falling back to `/height`.
  ///
  /// `/info` reads more of the core than `/height` does and can answer BUSY
  /// while the chain is locked, which during a long initial sync would leave
  /// the progress display frozen. `/height` reads two counters and always
  /// answers, so it keeps the numbers moving.
  Future<Map<String, dynamic>?> _probe(int port) async {
    final info = await _get(port, '/info');
    if (info != null && info['status'] == 'OK') return info;
    return _get(port, '/height');
  }

  /// `/info` from the daemon on this config's port, but only if it is the one
  /// this config describes. Returns null for an empty port and for a stranger
  /// alike, so the caller cannot confuse the two with a running node of ours.
  Future<Map<String, dynamic>?> _probeOurNode(LocalNodeConfig config) async {
    final info = await _get(config.rpcPort, '/info');
    if (info == null || info['status'] != 'OK') return null;
    final reported = (info['lite_start_height'] as num?)?.toInt();
    return reported == config.liteHeight ? info : null;
  }

  Future<Map<String, dynamic>?> _get(int port, String path) async {
    try {
      final request = await _http.get(kLocalNodeHost, port, path);
      final response = await request.close().timeout(const Duration(seconds: 5));
      if (response.statusCode != 200) {
        await response.drain<void>();
        return null;
      }
      final body = await response.transform(utf8.decoder).join();
      final decoded = jsonDecode(body);
      return decoded is Map<String, dynamic> ? decoded : null;
    } catch (_) {
      return null;
    }
  }

  /// Whether the port went quiet within [limit]. False means it timed out and
  /// the daemon is still answering, which is the only case that earns a kill.
  Future<bool> _waitForPortToClose(int port, Duration limit) async {
    final deadline = DateTime.now().add(limit);
    while (DateTime.now().isBefore(deadline)) {
      if (await _get(port, '/height') == null) return true;
      await Future<void>.delayed(const Duration(seconds: 1));
    }
    return false;
  }

  // ── process plumbing ───────────────────────────────────────────────────

  void _drain(Stream<List<int>> stream) {
    stream
        .transform(utf8.decoder)
        .transform(const LineSplitter())
        .listen((line) {
      _logTail.addLast(line);
      while (_logTail.length > _kLogTailLines) {
        _logTail.removeFirst();
      }
    }, onError: (_) {});
  }

  void _onExit(int code) {
    _poll?.cancel();
    _poll = null;
    _process = null;
    unawaited(_clearPidFile());

    // A stop() we asked for is not a failure. Anything else is the node
    // falling over, and the log tail is the only explanation available.
    if (_stopping ||
        state.phase == LocalNodePhase.stopped ||
        state.phase == LocalNodePhase.unconfigured) {
      return;
    }

    state = state.copyWith(
      phase: LocalNodePhase.failed,
      synced: false,
      error: _logTail.isEmpty
          ? 'Node exited with code $code.'
          : 'Node exited with code $code.\n\n${_logTail.join('\n')}',
    );
  }

  /// Records whether the node should come back up on the next launch.
  ///
  /// Written through to disk so it survives the app, and mirrored into the
  /// in-memory config so a later save does not undo it.
  Future<void> _setAutoStart(bool autoStart) async {
    final config = state.config;
    if (config == null || config.autoStart == autoStart) return;
    final updated = config.copyWith(autoStart: autoStart);
    state = state.copyWith(config: updated);
    try {
      await writeLocalNodeConfig(updated);
    } catch (_) {
      // Losing the preference costs one click on the next launch.
    }
  }

  Future<void> _writePidFile(int pid) async {
    try {
      await File(LocalNodePaths.pidPath).writeAsString('$pid');
    } catch (_) {
      // Adoption is a convenience; failing to record the pid is not fatal.
    }
  }

  Future<int?> _readPidFile() async {
    try {
      final file = File(LocalNodePaths.pidPath);
      if (!await file.exists()) return null;
      return int.tryParse((await file.readAsString()).trim());
    } catch (_) {
      return null;
    }
  }

  Future<void> _clearPidFile() async {
    try {
      final file = File(LocalNodePaths.pidPath);
      if (await file.exists()) await file.delete();
    } catch (_) {
      // Nothing depends on it being gone.
    }
  }

  /// The last lines the daemon printed, for the failure panel.
  String get logTail => _logTail.join('\n');
}

final localNodeProvider =
    NotifierProvider<LocalNodeController, LocalNodeState>(
        LocalNodeController.new);
