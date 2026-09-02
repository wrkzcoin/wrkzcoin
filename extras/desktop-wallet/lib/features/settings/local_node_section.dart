import 'dart:async';
import 'dart:io' show Platform;

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/node/local_node.dart';
import '../../core/node/local_node_controller.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';


/// Settings card for the lite node running on this machine.
///
/// The node's life is independent of the wallet's connection: it keeps syncing
/// whether or not the wallet is pointed at it, so a user can sit on a remote
/// node for the hours the first sync takes and switch over when it is ready —
/// and switch back at any time without stopping anything.
class LocalNodeSection extends ConsumerWidget {
  /// Called after the wallet has been pointed at a different daemon, so the
  /// host/port fields above can be refreshed to match.
  final void Function(String host, int port) onNodeSwitched;

  const LocalNodeSection({super.key, required this.onNodeSwitched});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tr = S.of(context);
    final node = ref.watch(localNodeProvider);

    if (node.phase == LocalNodePhase.loading) {
      return const SizedBox.shrink();
    }

    final nodeInfo = ref.watch(nodeInfoProvider).valueOrNull ?? const {};
    final inUse = node.config != null &&
        nodeInfo['daemonHost'] == kLocalNodeHost &&
        (nodeInfo['daemonPort'] as num?)?.toInt() == node.config!.rpcPort;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              tr?.localNodeDescription ??
                  'Run a node on this computer and sync against it instead of a '
                      'remote server. A lite node stores only what a wallet '
                      'needs, but it still downloads the whole chain once.',
              style: const TextStyle(color: kTextSecondary, fontSize: 13),
            ),
            const SizedBox(height: 16),
            if (node.phase == LocalNodePhase.unconfigured)
              FilledButton.icon(
                onPressed: () => _openSetup(context, ref),
                icon: const Icon(Icons.add_circle_outline, size: 18),
                label: Text(tr?.localNodeSetUp ?? 'Set up local node'),
              )
            else
              ..._configured(context, ref, node, inUse),
          ],
        ),
      ),
    );
  }

  List<Widget> _configured(
    BuildContext context,
    WidgetRef ref,
    LocalNodeState node,
    bool inUse,
  ) {
    final tr = S.of(context);
    final config = node.config!;
    final progress = node.progress;

    return [
      Row(
        children: [
          _PhaseChip(phase: node.phase),
          const SizedBox(width: 12),
          if (node.isRunning && node.networkHeight > 0) ...[
            Text(
              tr?.localNodeProgress(node.height, node.networkHeight) ??
                  'Block ${node.height} of ${node.networkHeight}',
              style: const TextStyle(color: kTextSecondary, fontSize: 12),
            ),
            // A bar creeping across a first sync says almost nothing on its
            // own. "1.9%" is the number people actually want, and it is the
            // difference between "stuck" and "working".
            if (progress != null) ...[
              const SizedBox(width: 8),
              Text(
                '${(progress * 100).toStringAsFixed(progress < 0.1 ? 2 : 1)}%',
                style: const TextStyle(
                    fontSize: 12, fontWeight: FontWeight.w600),
              ),
            ],
          ],
          const Spacer(),
          if (node.diskBytes > 0)
            Text(
              tr?.localNodeDiskUsage(formatBytes(node.diskBytes)) ??
                  '${formatBytes(node.diskBytes)} on disk',
              style: const TextStyle(color: kTextSecondary, fontSize: 12),
            ),
        ],
      ),
      if (node.isRunning && progress != null) ...[
        const SizedBox(height: 10),
        LinearProgressIndicator(
          value: progress,
          backgroundColor: kDivider,
          color: node.synced ? kSuccess : kWarning,
          minHeight: 4,
          borderRadius: BorderRadius.circular(2),
        ),
      ],
      const SizedBox(height: 12),
      Row(
        children: [
          Text(
            // Present tense would be a lie while it is still catching up: a
            // node at block 78,000 configured to keep everything from 4,000,000
            // serves nothing at all yet.
            node.canServeWallet
                ? (tr?.nodeServesFromLabel ?? 'Serves blocks from')
                : (tr?.nodeWillServeFromLabel ?? 'Will serve blocks from'),
            style: const TextStyle(color: kTextSecondary, fontSize: 12),
          ),
          const SizedBox(width: 8),
          SelectableText(
            '${config.liteHeight}',
            style: const TextStyle(fontSize: 12),
          ),
          const SizedBox(width: 16),
          if (node.isRunning)
            Text(
              tr?.localNodePeers(node.peers) ?? '${node.peers} peers',
              style: const TextStyle(color: kTextSecondary, fontSize: 12),
            ),
        ],
      ),
      // Where the several gigabytes went. Selectable because the answer to
      // "why is my disk full" should be copyable into a file manager.
      const SizedBox(height: 6),
      Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            tr?.localNodeDataFolder ?? 'Data folder',
            style: const TextStyle(color: kTextSecondary, fontSize: 12),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: SelectableText(
              LocalNodePaths.dataDir,
              style: const TextStyle(fontSize: 12),
            ),
          ),
        ],
      ),

      // An import runs for twenty to forty minutes with no RPC to poll, so
      // the only thing to show is what the daemon reports about itself. The
      // phase is named rather than folded into one bar: verifying reads the
      // whole file without writing anything and can still refuse it, which is
      // worth distinguishing from the part that is committing to disk.
      if (node.phase == LocalNodePhase.importing) ...[
        const SizedBox(height: 12),
        _Note(
          colour: kWarning,
          icon: Icons.download_for_offline_outlined,
          text: switch (node.importPhase) {
            'verify' =>
              'Checking the snapshot before writing any of it - '
                  '${node.importPercent.toStringAsFixed(0)}%.',
            'blocks' =>
              'Writing block headers - ${node.importPercent.toStringAsFixed(0)}%.',
            'write' =>
              'Loading the chain index - ${node.importPercent.toStringAsFixed(0)}%. '
                  'This is the long part.',
            _ => 'Importing the snapshot.',
          },
        ),
        const SizedBox(height: 8),
        LinearProgressIndicator(
          value: node.importPercent <= 0 ? null : node.importPercent / 100,
        ),
      ],

      // Why the wallet has not moved over yet. Without this the node looks
      // ready — it is running, it has peers — while the wallet sits on a
      // remote server for reasons nothing on screen explains.
      if (node.phase == LocalNodePhase.running && !node.synced) ...[
        const SizedBox(height: 12),
        _Note(
          colour: kWarning,
          icon: Icons.hourglass_bottom,
          text: tr?.localNodeNotReadyYet ??
              'The local node is still catching up and cannot serve the wallet '
                  'yet. It keeps syncing in the background — stay on a remote '
                  'node until it is ready, then switch over.',
        ),
      ],
      if (inUse) ...[
        const SizedBox(height: 12),
        _Note(
          colour: kSuccess,
          icon: Icons.check_circle_outline,
          text: tr?.localNodeInUse ?? 'The wallet is connected to this node.',
        ),
      ],
      if (node.phase == LocalNodePhase.failed) ...[
        const SizedBox(height: 12),
        _Note(
          colour: kError,
          icon: Icons.error_outline,
          text: node.error == 'daemon-not-found'
              ? (tr?.localNodeBinaryMissing(LocalNodePaths.daemonName) ??
                  '${LocalNodePaths.daemonName} was not found. Place the daemon '
                      'binary next to the wallet executable, or in a '
                      "'sidecar' folder beside it, and try again.")
              : (node.error ?? ''),
          selectable: true,
        ),
      ],

      const SizedBox(height: 14),
      const _NodeExitPolicyRow(),

      const SizedBox(height: 16),
      Wrap(
        spacing: 10,
        runSpacing: 10,
        children: [
          if (node.isRunning)
            OutlinedButton.icon(
              onPressed: () async {
                // Move the wallet off first, or stopping the node leaves it
                // pointed at a port nothing answers on.
                if (inUse) await _returnToRemote(context, ref);
                await ref.read(localNodeProvider.notifier).stop();
              },
              icon: const Icon(Icons.stop_circle_outlined, size: 18),
              label: Text(tr?.localNodeStop ?? 'Stop'),
            )
          else
            FilledButton.icon(
              onPressed: () => ref.read(localNodeProvider.notifier).start(),
              icon: const Icon(Icons.play_arrow, size: 18),
              label: Text(tr?.localNodeStart ?? 'Start'),
            ),
          FilledButton.tonalIcon(
            onPressed: node.canServeWallet && !inUse
                ? () => _useLocalNode(context, ref, config)
                : null,
            icon: const Icon(Icons.swap_horiz, size: 18),
            label: Text(tr?.localNodeUse ?? 'Use this node'),
          ),
          TextButton.icon(
            onPressed: () => _confirmDestroy(context, ref, inUse),
            style: TextButton.styleFrom(foregroundColor: kError),
            icon: const Icon(Icons.delete_outline, size: 18),
            label: Text(tr?.localNodeDelete ?? 'Delete node data'),
          ),
        ],
      ),
    ];
  }

  Future<void> _useLocalNode(
    BuildContext context,
    WidgetRef ref,
    LocalNodeConfig config,
  ) async {
    try {
      await ref
          .read(walletCApiProvider)
          .swapNode(kLocalNodeHost, config.rpcPort);
      ref.read(statusProvider.notifier).refresh();
      ref.read(nodeInfoProvider.notifier).refresh();
      onNodeSwitched(kLocalNodeHost, config.rpcPort);
    } on WalletCApiException catch (e) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(e.message), backgroundColor: kError),
      );
    }
  }

  /// The setup wizard. The start height chosen here is permanent for the
  /// database the node builds, so the cost and the one rule that matters —
  /// keep it at or below the wallet's own start — are stated before the field.
  Future<void> _openSetup(BuildContext context, WidgetRef ref) async {
    final status = ref.read(statusProvider).valueOrNull;
    final walletStart = status?.walletEarliestHeight;

    final heightCtrl =
        TextEditingController(text: '${walletStart ?? 0}');
    var acknowledged = false;

    // Where the chain will go. Editable only here: once the node exists, moving
    // a multi-gigabyte RocksDB is a different job from picking a folder.
    var dataDir = LocalNodePaths.dataDir;
    var dataDirFree = true;

    // Snapshot import. Null path means the ordinary sync-from-network route.
    String? snapshotPath;
    Map<String, dynamic>? snapshotInfo;
    String? snapshotError;
    var checkingSnapshot = false;

    final create = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return StatefulBuilder(
          builder: (ctx, setDialogState) {
            final entered = int.tryParse(heightCtrl.text);
            final valid = entered != null && entered > 0;
            // Above the wallet's own start the node can never show this
            // wallet's older transactions. Allowed, but only deliberately.
            final tooHigh = valid &&
                walletStart != null &&
                entered > walletStart;
            return AlertDialog(
              title: Text(dlgTr?.localNodeSetupTitle ??
                  'Set up a local lite node'),
              content: SizedBox(
                width: 460,
                child: SingleChildScrollView(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        snapshotPath == null
                            ? (dlgTr?.localNodeSetupCost ??
                                'Before you start:\n'
                                    '• Around 6 GB of disk space, and the whole chain is downloaded once.\n'
                                    '• The first sync takes hours. It continues in the background and you can keep using a remote node meanwhile.\n'
                                    '• The start height below is permanent. Changing it later means deleting the node and syncing again from nothing.')
                            : 'Before you start:\n'
                                '• Around 6 GB of disk space.\n'
                                '• Importing takes twenty to forty minutes and cannot be resumed. Interrupting it means starting again with an empty folder.\n'
                                '• The start height comes from the snapshot and is permanent. Changing it later means deleting the node and syncing again from nothing.',
                        style: const TextStyle(height: 1.6, fontSize: 13),
                      ),
                      const SizedBox(height: 14),

                      // How the chain gets here. First, because it decides
                      // what the height field below even means: with a
                      // snapshot the height is the file's, not a choice.
                      SegmentedButton<bool>(
                        segments: const [
                          ButtonSegment(
                            value: false,
                            label: Text('Sync from network'),
                            icon: Icon(Icons.cloud_download_outlined, size: 16),
                          ),
                          ButtonSegment(
                            value: true,
                            label: Text('Import a snapshot'),
                            icon: Icon(Icons.folder_zip_outlined, size: 16),
                          ),
                        ],
                        selected: {snapshotPath != null || snapshotError != null},
                        onSelectionChanged: (s) => setDialogState(() {
                          if (s.first) {
                            // Nothing picked yet; the picker below is the
                            // next step. Marked by an empty error so the
                            // segment stays selected.
                            snapshotError = '';
                          } else {
                            snapshotPath = null;
                            snapshotInfo = null;
                            snapshotError = null;
                            heightCtrl.text = '${walletStart ?? 0}';
                          }
                        }),
                      ),

                      if (snapshotPath != null || snapshotError != null) ...[
                        const SizedBox(height: 12),
                        Row(
                          children: [
                            Expanded(
                              child: Text(
                                snapshotPath == null
                                    ? 'No snapshot chosen yet.'
                                    : snapshotPath!.split(Platform.pathSeparator).last,
                                style: const TextStyle(fontSize: 12),
                                overflow: TextOverflow.ellipsis,
                              ),
                            ),
                            const SizedBox(width: 8),
                            OutlinedButton(
                              onPressed: checkingSnapshot
                                  ? null
                                  : () async {
                                      final result =
                                          await FilePicker.platform.pickFiles(
                                        dialogTitle: 'Choose a lite node snapshot',
                                        type: FileType.any,
                                      );
                                      final picked =
                                          result?.files.single.path;
                                      if (picked == null) return;

                                      setDialogState(() {
                                        checkingSnapshot = true;
                                        snapshotError = '';
                                      });

                                      // Ask the daemon what the file is
                                      // rather than trusting its name. It
                                      // also says whether this build will
                                      // accept the digest, which is worth
                                      // knowing now and not after half an
                                      // hour of importing.
                                      final info = await ref
                                          .read(localNodeProvider.notifier)
                                          .describeSnapshot(picked);

                                      setDialogState(() {
                                        checkingSnapshot = false;
                                        if (info == null || info['error'] != null) {
                                          snapshotPath = null;
                                          snapshotInfo = null;
                                          snapshotError = info?['error'] as String? ??
                                              'That file is not a lite node snapshot.';
                                          return;
                                        }
                                        snapshotPath = picked;
                                        snapshotInfo = info;
                                        snapshotError = null;
                                        heightCtrl.text = '${info['liteHeight']}';
                                      });
                                    },
                              child: Text(checkingSnapshot ? 'Reading...' : 'Choose...'),
                            ),
                          ],
                        ),
                        if (snapshotInfo != null) ...[
                          const SizedBox(height: 6),
                          Text(
                            snapshotInfo!['accepted'] == true
                                ? 'Serves blocks from ${snapshotInfo!['liteHeight']}. '
                                    'Recognised by this build.'
                                : 'Serves blocks from ${snapshotInfo!['liteHeight']}, but this '
                                    'build does not recognise its digest and will refuse it. '
                                    'Use a published snapshot.',
                            style: TextStyle(
                              color: snapshotInfo!['accepted'] == true
                                  ? kTextSecondary
                                  : kError,
                              fontSize: 12,
                              height: 1.4,
                            ),
                          ),
                        ],
                        if (snapshotError != null && snapshotError!.isNotEmpty) ...[
                          const SizedBox(height: 6),
                          Text(
                            snapshotError!,
                            style: const TextStyle(
                                color: kError, fontSize: 12, height: 1.4),
                          ),
                        ],
                      ],

                      const SizedBox(height: 18),
                      TextField(
                        controller: heightCtrl,
                        autofocus: snapshotPath == null,
                        // A snapshot only imports at the height it was made
                        // at - the daemon refuses a mismatch - so once one is
                        // chosen this is a fact, not a choice.
                        readOnly: snapshotPath != null,
                        onChanged: (_) => setDialogState(() {}),
                        keyboardType: TextInputType.number,
                        inputFormatters: [
                          FilteringTextInputFormatter.digitsOnly
                        ],
                        decoration: InputDecoration(
                          labelText: dlgTr?.localNodeStartHeightLabel ??
                              'Start height',
                        ),
                      ),
                      if (walletStart != null) ...[
                        const SizedBox(height: 8),
                        Text(
                          dlgTr?.localNodeStartHeightHelp(walletStart) ??
                              'Blocks below this height are downloaded and '
                                  'checked, then only the index later blocks '
                                  'need is kept. Keep it at or below this '
                                  "wallet's own start height ($walletStart).",
                          style: const TextStyle(
                              color: kTextSecondary, fontSize: 12, height: 1.4),
                        ),
                      ],
                      if (!valid) ...[
                        const SizedBox(height: 8),
                        // Without this the button is simply greyed out with
                        // nothing to say why, which is where a wallet whose own
                        // start height is unknown leaves every user.
                        Text(
                          dlgTr?.localNodeStartHeightRequired ??
                              'Enter the height to keep full blocks from. It '
                                  'has to be above zero — a lite node cannot '
                                  'start at the genesis block.',
                          style: const TextStyle(
                              color: kWarning, fontSize: 12, height: 1.4),
                        ),
                      ],
                      const SizedBox(height: 18),
                      Text(
                        dlgTr?.localNodeDataFolder ?? 'Data folder',
                        style: const TextStyle(
                            fontSize: 12, fontWeight: FontWeight.w600),
                      ),
                      const SizedBox(height: 6),
                      Row(
                        children: [
                          Expanded(
                            child: Text(
                              dataDir,
                              style: const TextStyle(
                                  color: kTextSecondary,
                                  fontSize: 12,
                                  height: 1.4),
                            ),
                          ),
                          const SizedBox(width: 8),
                          OutlinedButton(
                            onPressed: () async {
                              final picked = await FilePicker.platform
                                  .getDirectoryPath(
                                      dialogTitle: dlgTr?.localNodeDataFolder ??
                                          'Data folder');
                              if (picked == null) return;
                              final free = await nodeDirIsClaimable(picked);
                              setDialogState(() {
                                dataDir = picked;
                                dataDirFree = free;
                              });
                            },
                            child: Text(dlgTr?.browse ?? 'Browse…'),
                          ),
                        ],
                      ),
                      const SizedBox(height: 6),
                      Text(
                        dlgTr?.localNodeDataFolderHelp ??
                            'Around 6 GB is written here. Pick a drive with '
                                'room for it.',
                        style: const TextStyle(
                            color: kTextSecondary, fontSize: 12, height: 1.4),
                      ),
                      if (!dataDirFree) ...[
                        const SizedBox(height: 12),
                        _Note(
                          colour: kError,
                          icon: Icons.folder_off_outlined,
                          text: dlgTr?.localNodeDataFolderInUse ??
                              'That folder already holds other files. Choose an '
                                  'empty folder, or a new one.',
                        ),
                      ],
                      if (tooHigh) ...[
                        const SizedBox(height: 12),
                        _Note(
                          colour: kWarning,
                          icon: Icons.warning_amber_rounded,
                          text: dlgTr?.localNodeStartHeightTooHigh(
                                  walletStart) ??
                              "Higher than this wallet's start height "
                                  '($walletStart). The node would never be able '
                                  "to show this wallet's older transactions.",
                        ),
                        CheckboxListTile(
                          value: acknowledged,
                          onChanged: (v) => setDialogState(
                              () => acknowledged = v ?? false),
                          dense: true,
                          contentPadding: EdgeInsets.zero,
                          controlAffinity: ListTileControlAffinity.leading,
                          title: Text(
                            dlgTr?.iUnderstandContinue ??
                                'I understand, continue',
                            style: const TextStyle(fontSize: 13),
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(ctx, false),
                  child: Text(dlgTr?.cancel ?? 'Cancel'),
                ),
                FilledButton(
                  onPressed:
                      (!valid || !dataDirFree || (tooHigh && !acknowledged))
                          ? null
                          : () => Navigator.pop(ctx, true),
                  child: Text(dlgTr?.localNodeCreate ?? 'Create node'),
                ),
              ],
            );
          },
        );
      },
    );

    final height = int.tryParse(heightCtrl.text) ?? 0;
    heightCtrl.dispose();
    if (create != true || height <= 0) return;

    // create() writes into LocalNodePaths.dataDir and then remembers it, so the
    // choice has to be bound before it runs. Safe here and nowhere else: the
    // wizard only opens when no node is configured.
    LocalNodePaths.bindDataDirectory(dataDir);

    await ref
        .read(localNodeProvider.notifier)
        .create(liteHeight: height, snapshotPath: snapshotPath);
  }

  /// Points the wallet back at the default remote node.
  ///
  /// Used before stopping or deleting the local node: the wallet keeps
  /// whatever it has already scanned and simply carries on against a daemon
  /// that holds the whole chain.
  Future<void> _returnToRemote(BuildContext context, WidgetRef ref) async {
    try {
      await ref.read(walletCApiProvider).swapNode(
            kDefaultDaemonHost,
            kDefaultDaemonPort,
            ssl: kDefaultDaemonSSL,
          );
      ref.read(statusProvider.notifier).refresh();
      ref.read(nodeInfoProvider.notifier).refresh();
      onNodeSwitched(kDefaultDaemonHost, kDefaultDaemonPort);
    } on WalletCApiException catch (e) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(e.message), backgroundColor: kError),
      );
    }
  }

  Future<void> _confirmDestroy(
    BuildContext context,
    WidgetRef ref,
    bool inUse,
  ) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(
              dlgTr?.localNodeDeleteTitle ?? 'Delete local node data?'),
          content: Text(
            dlgTr?.localNodeDeleteWarning ??
                'This stops the node and permanently deletes its blockchain '
                    'database from disk. Your wallet, its seed and its funds are '
                    'not touched — but a new local node starts syncing again '
                    'from nothing, which takes hours.',
            style: const TextStyle(height: 1.5),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(dlgTr?.cancel ?? 'Cancel'),
            ),
            TextButton(
              onPressed: () => Navigator.pop(ctx, true),
              style: TextButton.styleFrom(foregroundColor: kError),
              child: Text(dlgTr?.iUnderstandContinue ?? 'I understand, continue'),
            ),
          ],
        );
      },
    );
    if (confirmed != true) return;
    if (!context.mounted) return;

    // Same reason as Stop: the wallet must not be left addressing a node that
    // is about to stop existing.
    if (inUse) await _returnToRemote(context, ref);

    await ref.read(localNodeProvider.notifier).destroy();
    if (!context.mounted) return;
    final tr = S.of(context);
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(tr?.localNodeDeleted ?? 'Local node deleted.')),
    );
  }
}

class _PhaseChip extends StatelessWidget {
  final LocalNodePhase phase;
  const _PhaseChip({required this.phase});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final (String label, Color colour) = switch (phase) {
      LocalNodePhase.importing => ('Importing', kWarning),
      LocalNodePhase.starting =>
        (tr?.localNodeStateStarting ?? 'Starting', kWarning),
      LocalNodePhase.running => (tr?.localNodeStateSyncing ?? 'Syncing', kWarning),
      LocalNodePhase.failed => (tr?.localNodeStateFailed ?? 'Failed', kError),
      _ => (tr?.localNodeStateStopped ?? 'Stopped', kTextSecondary),
    };

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: colour.withAlpha(30),
        borderRadius: BorderRadius.circular(20),
      ),
      child: Text(label,
          style: TextStyle(
              color: colour, fontSize: 12, fontWeight: FontWeight.w600)),
    );
  }
}

/// "When the wallet closes: …" — the standing answer to the question the
/// shutdown overlay asks, so a remembered choice can be taken back.
class _NodeExitPolicyRow extends StatefulWidget {
  const _NodeExitPolicyRow();

  @override
  State<_NodeExitPolicyRow> createState() => _NodeExitPolicyRowState();
}

class _NodeExitPolicyRowState extends State<_NodeExitPolicyRow> {
  NodeExitPolicy? _policy;

  @override
  void initState() {
    super.initState();
    unawaited(_load());
  }

  Future<void> _load() async {
    final policy = await readNodeExitPolicy();
    if (mounted) setState(() => _policy = policy);
  }

  Future<void> _set(NodeExitPolicy policy) async {
    setState(() => _policy = policy);
    await writeNodeExitPolicy(policy);
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final policy = _policy;
    if (policy == null) return const SizedBox.shrink();

    String label(NodeExitPolicy p) => switch (p) {
          NodeExitPolicy.ask => tr?.nodeExitPolicyAsk ?? 'Ask me',
          NodeExitPolicy.keep =>
            tr?.nodeExitPolicyKeep ?? 'Leave the node running',
          NodeExitPolicy.stop => tr?.nodeExitPolicyStop ?? 'Stop the node',
        };

    return Row(
      children: [
        Text(
          tr?.nodeExitPolicyLabel ?? 'When the wallet closes',
          style: const TextStyle(color: kTextSecondary, fontSize: 12),
        ),
        const SizedBox(width: 10),
        DropdownButton<NodeExitPolicy>(
          value: policy,
          isDense: true,
          underline: const SizedBox.shrink(),
          style: const TextStyle(fontSize: 12),
          items: [
            for (final p in NodeExitPolicy.values)
              DropdownMenuItem(
                value: p,
                child: Text(label(p), style: const TextStyle(fontSize: 12)),
              ),
          ],
          onChanged: (p) => p == null ? null : unawaited(_set(p)),
        ),
      ],
    );
  }
}

class _Note extends StatelessWidget {
  final Color colour;
  final IconData icon;
  final String text;
  final bool selectable;

  const _Note({
    required this.colour,
    required this.icon,
    required this.text,
    this.selectable = false,
  });

  @override
  Widget build(BuildContext context) {
    final style = TextStyle(color: colour, fontSize: 12, height: 1.4);
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: colour.withAlpha(25),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: colour, size: 15),
          const SizedBox(width: 8),
          Expanded(
            child: selectable
                ? SelectableText(text, style: style)
                : Text(text, style: style),
          ),
        ],
      ),
    );
  }
}
