/// local_node.dart
///
/// Everything the app needs to describe a lite node running on this machine:
/// where its binary and data live, what it was configured with, and what it is
/// currently doing.
///
/// The node is a child process, not a library. `Wrkzd`'s `main()` calls
/// `exit()` on a dozen paths, installs a process-global signal handler and
/// blocks in the p2p loop, so hosting it in-process would mean refactoring the
/// daemon; a supervised child costs nothing and can be adopted again after an
/// app crash.
library;

import 'dart:convert';
import 'dart:io';

import 'package:path/path.dart' as p;

/// Where the RPC listener binds. Loopback only — this node exists for this
/// machine's wallet and nothing else should be able to reach it.
///
/// It is unauthenticated: `--rpc-access-token` cannot be used because the
/// wallet backend has no way to send the header (Nigel builds its request
/// headers itself and only ever sets a User-Agent). On POSIX the daemon can
/// also expose an AF_UNIX socket with 0600 permissions, which would be
/// tighter, but Windows compiles that out entirely, so loopback is what both
/// ends can always agree on.
const String kLocalNodeHost = '127.0.0.1';

/// Config filename inside the node data directory.
const String _kConfigFile = 'pluton-node.json';

/// Records the pid across app restarts so a node left running by a crashed or
/// force-quit app can be found and either adopted or stopped, rather than
/// having a second one started next to it on the same data directory.
const String _kPidFile = 'pluton-node.pid';

/// The daemon's own log, which is where a startup failure explains itself.
const String kLocalNodeLogFile = 'wrkzd.log';

/// A configured local node. Written to disk once and then read back; the
/// values here are what the daemon is launched with every time.
class LocalNodeConfig {
  /// The height at and above which the node keeps full block data.
  ///
  /// Permanent for the database. The daemon refuses to start against a
  /// database built at a different height rather than wiping it, so changing
  /// this means deleting the data directory and syncing again from nothing.
  final int liteHeight;

  /// Loopback RPC port. Chosen once from an ephemeral bind so it does not
  /// collide with a daemon the user runs themselves on the standard port.
  final int rpcPort;

  /// P2P listen port, chosen the same way and for the same reason.
  final int p2pPort;

  final DateTime createdAt;

  /// Bring the node up again on the next app launch.
  ///
  /// A first sync runs for hours, so a node the user left running should not
  /// need starting by hand every time. Cleared by an explicit Stop, so a node
  /// deliberately turned off stays off.
  final bool autoStart;

  const LocalNodeConfig({
    required this.liteHeight,
    required this.rpcPort,
    required this.p2pPort,
    required this.createdAt,
    this.autoStart = true,
  });

  Map<String, dynamic> toJson() => {
        'liteHeight': liteHeight,
        'rpcPort': rpcPort,
        'p2pPort': p2pPort,
        'createdAt': createdAt.toIso8601String(),
        'autoStart': autoStart,
      };

  factory LocalNodeConfig.fromJson(Map<String, dynamic> json) =>
      LocalNodeConfig(
        liteHeight: (json['liteHeight'] as num?)?.toInt() ?? 0,
        rpcPort: (json['rpcPort'] as num?)?.toInt() ?? 0,
        p2pPort: (json['p2pPort'] as num?)?.toInt() ?? 0,
        createdAt: DateTime.tryParse(json['createdAt'] as String? ?? '') ??
            DateTime.now(),
        autoStart: json['autoStart'] as bool? ?? true,
      );

  LocalNodeConfig copyWith({bool? autoStart}) => LocalNodeConfig(
        liteHeight: liteHeight,
        rpcPort: rpcPort,
        p2pPort: p2pPort,
        createdAt: createdAt,
        autoStart: autoStart ?? this.autoStart,
      );

  /// The command line this config launches.
  ///
  /// `--no-console` matters more than it looks: with a piped stdin the
  /// daemon's console reader hits EOF and spins, so the console is turned off
  /// and the process is stopped by signal instead.
  ///
  /// `WRKZ_DAEMON_EXTRA_ARGS` is appended last, space separated. It exists for
  /// daemons built without the options this list assumes — a RocksDB linked
  /// without ZSTD needs `--db-enable-compression=false` or it exits at
  /// startup — and for debugging. Nothing in the app sets it.
  List<String> arguments(String dataDir) => [
        '--data-dir', dataDir,
        '--lite',
        '--lite-height', '$liteHeight',
        '--no-console',
        '--rpc-bind-ip', kLocalNodeHost,
        '--rpc-bind-port', '$rpcPort',
        '--p2p-bind-port', '$p2pPort',
        '--log-file', p.join(dataDir, kLocalNodeLogFile),
        '--log-level', '2',
        ...extraDaemonArguments(),
      ];
}

enum LocalNodePhase {
  /// Still reading configuration off disk.
  loading,

  /// No node has been set up on this machine yet.
  unconfigured,

  /// Configured, not running.
  stopped,

  /// Process launched, RPC has not answered yet.
  starting,

  /// Process is up and answering.
  running,

  /// The process exited on its own, or never came up.
  failed,
}

/// A point-in-time view of the local node, refreshed by polling its RPC.
class LocalNodeState {
  final LocalNodePhase phase;
  final LocalNodeConfig? config;

  /// Blocks this node holds, and what it believes the network has. Both zero
  /// until the RPC first answers.
  final int height;
  final int networkHeight;
  final int peers;

  /// The node considers itself level with the network. Read straight from
  /// `/info`; it is the gate on pointing a wallet at it.
  final bool synced;

  /// The start height the daemon itself reports, from `/info`. Zero until the
  /// RPC answers, and it should equal the configured height — a difference
  /// means the running daemon is not the one this config describes.
  final int reportedLiteStartHeight;

  /// Bytes the data directory occupies, refreshed with the poll.
  final int diskBytes;

  /// Why the node is in [LocalNodePhase.failed], or the tail of its log.
  final String? error;

  const LocalNodeState({
    this.phase = LocalNodePhase.loading,
    this.config,
    this.height = 0,
    this.networkHeight = 0,
    this.peers = 0,
    this.synced = false,
    this.reportedLiteStartHeight = 0,
    this.diskBytes = 0,
    this.error,
  });

  bool get isConfigured => config != null;

  bool get isRunning =>
      phase == LocalNodePhase.running || phase == LocalNodePhase.starting;

  /// The node can serve a wallet: it is up, answering, level with the network,
  /// and has actually spoken to someone. A node that is still catching up
  /// would leave the wallet parked — safely, but with nothing on screen to
  /// explain the stall — so switching to it is gated on this.
  ///
  /// The peer count and the lite height are part of the test because the
  /// daemon computes `synced` as `height == network_height`, and both sides of
  /// that are 1 on a brand new node that has not found a peer yet. Taken alone
  /// it would call an empty database synced.
  bool get canServeWallet =>
      phase == LocalNodePhase.running &&
      synced &&
      peers > 0 &&
      config != null &&
      height >= config!.liteHeight;

  /// Sync progress 0.0 → 1.0, or null while the heights are not known.
  double? get progress {
    if (networkHeight <= 0 || height <= 0) return null;
    return (height / networkHeight).clamp(0.0, 1.0);
  }

  LocalNodeState copyWith({
    LocalNodePhase? phase,
    LocalNodeConfig? config,
    bool clearConfig = false,
    int? height,
    int? networkHeight,
    int? peers,
    bool? synced,
    int? reportedLiteStartHeight,
    int? diskBytes,
    String? error,
    bool clearError = false,
  }) =>
      LocalNodeState(
        phase: phase ?? this.phase,
        config: clearConfig ? null : (config ?? this.config),
        height: height ?? this.height,
        networkHeight: networkHeight ?? this.networkHeight,
        peers: peers ?? this.peers,
        synced: synced ?? this.synced,
        reportedLiteStartHeight:
            reportedLiteStartHeight ?? this.reportedLiteStartHeight,
        diskBytes: diskBytes ?? this.diskBytes,
        error: clearError ? null : (error ?? this.error),
      );
}

/// Locating the daemon binary, its data directory and its config file.
class LocalNodePaths {
  /// The data directory, set once at app start.
  static String? _dataDir;

  static void bindDataDirectory(String path) => _dataDir = path;

  static String get dataDir {
    final dir = _dataDir;
    if (dir == null) {
      throw StateError('LocalNodePaths.bindDataDirectory has not been called');
    }
    return dir;
  }

  static String get configPath => p.join(dataDir, _kConfigFile);

  static String get pidPath => p.join(dataDir, _kPidFile);

  static String get logPath => p.join(dataDir, kLocalNodeLogFile);

  static String get daemonName => Platform.isWindows ? 'Wrkzd.exe' : 'Wrkzd';

  /// Finds the daemon to run, or null if it is not shipped alongside the app.
  ///
  /// `WRKZ_DAEMON_PATH` wins so a development build can point at a build tree
  /// without copying anything.
  static File? findDaemon() {
    final override = Platform.environment['WRKZ_DAEMON_PATH'];
    if (override != null && override.isNotEmpty) {
      final f = File(override);
      if (f.existsSync()) return f;
    }

    final appDir = p.dirname(Platform.resolvedExecutable);

    final candidates = <String>[
      p.join(appDir, daemonName),
      p.join(appDir, 'sidecar', daemonName),
      // macOS bundle: the runner sits in Contents/MacOS, resources one up.
      p.join(appDir, '..', 'Resources', daemonName),
      p.join(appDir, '..', 'Resources', 'sidecar', daemonName),
      // `flutter run` leaves the working directory at the project root.
      p.join(Directory.current.path, daemonName),
      p.join(Directory.current.path, 'sidecar', daemonName),
    ];

    for (final path in candidates) {
      final f = File(p.normalize(path));
      if (f.existsSync()) return f;
    }
    return null;
  }

  /// Sum of every file under the data directory. Zero when it cannot be
  /// walked, which callers treat as "do not report it" rather than as a size.
  static Future<int> directorySize(String path) async {
    final dir = Directory(path);
    if (!await dir.exists()) return 0;
    var total = 0;
    try {
      await for (final entity in dir.list(recursive: true, followLinks: false)) {
        if (entity is File) {
          try {
            total += await entity.length();
          } on FileSystemException {
            // A compaction can delete an SST between listing and stat.
          }
        }
      }
    } on FileSystemException {
      return 0;
    }
    return total;
  }
}

/// Reads the stored config, or null when no node has been set up.
Future<LocalNodeConfig?> readLocalNodeConfig() async {
  final file = File(LocalNodePaths.configPath);
  if (!await file.exists()) return null;
  try {
    final json = jsonDecode(await file.readAsString()) as Map<String, dynamic>;
    final config = LocalNodeConfig.fromJson(json);
    if (config.liteHeight <= 0 || config.rpcPort <= 1024) return null;
    return config;
  } catch (_) {
    return null;
  }
}

Future<void> writeLocalNodeConfig(LocalNodeConfig config) async {
  final file = File(LocalNodePaths.configPath);
  await file.parent.create(recursive: true);
  await file.writeAsString(jsonEncode(config.toJson()));
}

/// Extra daemon flags from `WRKZ_DAEMON_EXTRA_ARGS`, split on whitespace.
///
/// An escape hatch, not a feature: the app never sets this. It is here so a
/// daemon built without something the standard argument list assumes can still
/// be driven — the usual case being a RocksDB linked without ZSTD, which needs
/// `--db-enable-compression=false` or the daemon exits during startup.
List<String> extraDaemonArguments() {
  final raw = Platform.environment['WRKZ_DAEMON_EXTRA_ARGS'];
  if (raw == null || raw.trim().isEmpty) return const [];
  return raw.trim().split(RegExp(r'\s+'));
}

/// Asks the OS for a free port by binding an ephemeral one and letting go.
///
/// Racy in principle — nothing stops another process taking it in between —
/// but the port is stored and reused, so a collision shows up once as a
/// startup failure rather than silently.
Future<int> pickFreePort() async {
  final socket = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
  final port = socket.port;
  await socket.close();
  return port;
}

/// Human-readable byte size, matching how the daemon reports its own storage.
String formatBytes(int bytes) {
  if (bytes <= 0) return '0 B';
  const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
  var value = bytes.toDouble();
  var unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit++;
  }
  return '${value.toStringAsFixed(unit == 0 ? 0 : 1)} ${units[unit]}';
}
