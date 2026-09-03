import 'dart:async';
import 'dart:io';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../core/node/local_node.dart';
import '../../core/node/local_node_controller.dart';
import '../../shared/theme/app_theme.dart';
import '../../l10n/generated/app_localizations.dart';
import 'local_node_section.dart';

const _storage = FlutterSecureStorage();
const _kLastWalletKey = 'pluton_last_wallet_path';

// -- Last-opened wallet persistence -------------------------------------------

final lastWalletPathProvider = FutureProvider<String?>((ref) async {
  return _storage.read(key: _kLastWalletKey);
});

Future<void> saveLastWalletPath(String path) async {
  await _storage.write(key: _kLastWalletKey, value: path);
}

Future<void> clearLastWalletPath() async {
  await _storage.delete(key: _kLastWalletKey);
}

// -- Screen -------------------------------------------------------------------

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  bool _savingNode = false;
  String? _nodeError;
  String? _nodeSuccess;

  // Node form
  final _nodeHostCtrl = TextEditingController();
  final _nodePortCtrl = TextEditingController();
  bool _nodeSSL = false;

  // Tx PoW server form
  final _powHostCtrl = TextEditingController();
  final _powPortCtrl = TextEditingController();
  bool _powEnabled = false;
  bool _powSSL = false;
  bool _powFormLoaded = false;
  String? _powError;
  String? _powSuccess;

  @override
  void initState() {
    super.initState();
    // Pre-fill from current node info via FFI
    ref.read(walletCApiProvider).getNodeInfoJson().then((info) {
      if (mounted) {
        _nodeHostCtrl.text = info['daemonHost'] as String? ?? '';
        _nodePortCtrl.text = (info['daemonPort'] as num?)?.toString() ?? '';
        setState(() => _nodeSSL = info['daemonSSL'] as bool? ?? false);
      }
    }).catchError((_) {});
  }

  @override
  void dispose() {
    _nodeHostCtrl.dispose();
    _nodePortCtrl.dispose();
    _powHostCtrl.dispose();
    _powPortCtrl.dispose();
    super.dispose();
  }

  /// Prefills the PoW server form once the stored setting has been read, and
  /// leaves it alone afterwards so typing is not overwritten by rebuilds.
  void _syncPowForm(TxPowServerSettings s) {
    if (_powFormLoaded || !s.loaded) return;
    _powFormLoaded = true;
    _powEnabled = s.enabled;
    _powSSL = s.ssl;
    _powHostCtrl.text = s.host;
    _powPortCtrl.text = '${s.port}';
  }

  Future<void> _savePowServer() async {
    final tr = S.of(context);
    final host = _powHostCtrl.text.trim();
    final port = int.tryParse(_powPortCtrl.text.trim()) ?? 0;
    if (_powEnabled && (host.isEmpty || port <= 0 || port > 65535)) {
      setState(() {
        _powError = tr?.txPowServerInvalid ?? 'Enter a valid host and port';
        _powSuccess = null;
      });
      return;
    }
    final settings = TxPowServerSettings(
      enabled: _powEnabled,
      host: host,
      port: port > 0 ? port : kDefaultTxPowServerPort,
      ssl: _powSSL,
    );
    await ref.read(txPowServerProvider.notifier).set(settings);
    settings.applyTo(ref.read(walletCApiProvider));
    if (!mounted) return;
    setState(() {
      _powError = null;
      _powSuccess = tr?.txPowServerSaved ?? 'PoW server settings saved';
    });
  }

  /// True when Apply may go ahead.
  ///
  /// Only asks when the target is this machine's own lite node and that node
  /// cannot serve the wallet yet — it is still catching up, has no peers, or
  /// has not reached its own start height. Any other daemon is the user's
  /// business and is applied without comment.
  Future<bool> _confirmUnreadyLocalNode() async {
    final node = ref.read(localNodeProvider);
    final config = node.config;
    if (config == null) return true;

    final targetingLocal = _nodeHostCtrl.text.trim() == kLocalNodeHost &&
        int.tryParse(_nodePortCtrl.text) == config.rpcPort;
    if (!targetingLocal || node.canServeWallet) return true;

    final tr = S.of(context);
    final behind = node.networkHeight > node.height
        ? node.networkHeight - node.height
        : 0;

    final proceed = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.localNodeNotReadyTitle ?? 'This node is not ready'),
          content: SizedBox(
            width: 420,
            child: Text(
              dlgTr?.localNodeNotReadyBody(behind) ??
                  'The local node is still $behind blocks behind the network. '
                      'Point the wallet at it now and the sync will park until '
                      'it catches up, showing a balance that is missing '
                      'anything it has not reached — with nothing on screen to '
                      'say why. Staying on the remote node until it is ready '
                      'costs nothing; the node keeps syncing either way.',
              style: const TextStyle(height: 1.5, fontSize: 13),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(dlgTr?.cancel ?? 'Cancel'),
            ),
            TextButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(dlgTr?.switchAnyway ?? 'Switch anyway'),
            ),
          ],
        );
      },
    );

    if (proceed != true) {
      if (mounted) {
        setState(() =>
            _nodeError = tr?.localNodeNotReadyTitle ?? 'This node is not ready');
      }
      return false;
    }
    return true;
  }

  Future<void> _saveNode() async {
    final tr = S.of(context);
    if (_nodeHostCtrl.text.trim().isEmpty) {
      setState(() => _nodeError = tr?.hostIpAddress ?? 'Host / IP address');
      return;
    }

    // "Use this node" in the card below is gated on the node being able to
    // serve the wallet. This field and Apply reached the same daemon with no
    // gate at all, which made that check two clicks from useless - and pointing
    // a wallet at a lite node that has not reached its own start height is
    // exactly what parks the sync with an incomplete balance.
    if (!await _confirmUnreadyLocalNode()) return;

    setState(() { _savingNode = true; _nodeError = null; _nodeSuccess = null; });
    try {
      await ref.read(walletCApiProvider).swapNode(
        _nodeHostCtrl.text.trim(),
        int.tryParse(_nodePortCtrl.text) ?? kDefaultDaemonPort,
        ssl: _nodeSSL,
      );
      ref.read(statusProvider.notifier).refresh();
      if (!mounted) return;
      setState(() => _nodeSuccess = tr?.nodeUpdatedSuccess ?? 'Node updated successfully');
    } on WalletCApiException catch (e) {
      if (!mounted) return;
      setState(() => _nodeError = e.message);
    } catch (e) {
      if (!mounted) return;
      setState(() => _nodeError = e.toString());
    } finally {
      if (mounted) setState(() => _savingNode = false);
    }
  }

  Future<void> _saveWallet() async {
    final tr = S.of(context);
    try {
      await ref.read(walletCApiProvider).save();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr?.walletSaved ?? 'Wallet saved'), backgroundColor: kSuccess),
        );
      }
    } on WalletCApiException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(e.message), backgroundColor: kError),
        );
      }
    }
  }

  /// Writes the full wallet as unencrypted JSON.
  ///
  /// The payload carries the private view key and every subwallet's private
  /// spend key in the clear — it is seed-equivalent — so this is gated behind
  /// an explicit warning and a password re-authentication.
  Future<void> _exportJson() async {
    final tr = S.of(context);

    final acknowledged = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.exportJsonWarningTitle ??
              'Export unencrypted wallet?'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Icon(Icons.warning_amber_rounded, color: kError, size: 40),
              const SizedBox(height: 12),
              Text(
                dlgTr?.exportJsonWarningBody ??
                    'The exported file contains your private view key and '
                        'private spend keys in plain text. Anyone who reads it '
                        'can spend your funds.\n\n'
                        'Save it only to storage you control, and delete it as '
                        'soon as you are done.',
                style: const TextStyle(height: 1.5),
              ),
            ],
          ),
          actions: [
            TextButton(
                onPressed: () => Navigator.pop(ctx, false),
                child: Text(dlgTr?.cancel ?? 'Cancel')),
            TextButton(
              onPressed: () => Navigator.pop(ctx, true),
              style: TextButton.styleFrom(foregroundColor: kError),
              child: Text(
                  dlgTr?.iUnderstandContinue ?? 'I understand, continue'),
            ),
          ],
        );
      },
    );
    if (acknowledged != true || !mounted) return;
    if (!await _confirmPassword()) return;
    if (!mounted) return;

    final path = await FilePicker.platform.saveFile(
      dialogTitle: tr?.exportJsonTitle ?? 'Export wallet JSON',
      fileName: 'wallet_export.json',
    );
    if (path == null) return;
    try {
      final jsonStr = await ref.read(walletCApiProvider).exportJson();
      await File(path).writeAsString(jsonStr);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr?.exportedTo(path) ?? 'Exported to $path'), backgroundColor: kSuccess),
        );
      }
    } on WalletCApiException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(e.message), backgroundColor: kError),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr?.exportFailed(e.toString()) ?? 'Export failed: $e'), backgroundColor: kError),
        );
      }
    }
  }

  /// Re-authenticates the user against the stored verifier.
  ///
  /// A null verdict means "cannot tell" — no verifier recorded on this
  /// install. The wallet is already open and unlocked at this point, so treat
  /// that as a pass rather than blocking the user out of their own data.
  Future<bool> _confirmPassword() async {
    final passCtrl = TextEditingController();
    try {
      final ok = await showDialog<bool>(
        context: context,
        builder: (ctx) {
          final dlgTr = S.of(ctx);
          return AlertDialog(
            title: Text(dlgTr?.walletPassword ?? 'Wallet password'),
            content: TextField(
              controller: passCtrl,
              obscureText: true,
              autofocus: true,
              decoration:
                  InputDecoration(labelText: dlgTr?.password ?? 'Password'),
              onSubmitted: (_) => Navigator.pop(ctx, true),
            ),
            actions: [
              TextButton(
                  onPressed: () => Navigator.pop(ctx, false),
                  child: Text(dlgTr?.cancel ?? 'Cancel')),
              FilledButton(
                  onPressed: () => Navigator.pop(ctx, true),
                  child: Text(dlgTr?.unlock ?? 'Unlock')),
            ],
          );
        },
      );
      if (ok != true) return false;
      final verified = await verifyWalletPassword(passCtrl.text);
      if (verified == false) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
                content: Text(
                    S.of(context)?.incorrectPassword ?? 'Incorrect password'),
                backgroundColor: kError),
          );
        }
        return false;
      }
      return true;
    } finally {
      passCtrl.dispose();
    }
  }

  /// Rescan from a chosen height.
  ///
  /// Against a lite node the field is floored at that node's start height: it
  /// holds nothing below there and would answer a lower request from its own
  /// start anyway, which would move the wallet's recorded position over blocks
  /// nobody ever looked at. The native side refuses it as well — the daemon
  /// can be swapped between this check and the call — and that refusal gets
  /// its own dialog rather than a bare error code. See LITENODE.md.
  Future<void> _resetScanHeight() async {
    final liteStart =
        ref.read(statusProvider).valueOrNull?.daemonLiteStartHeight ?? 0;

    final heightCtrl =
        TextEditingController(text: liteStart > 0 ? '$liteStart' : '0');
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return StatefulBuilder(
          builder: (ctx, setDialogState) {
            final entered = int.tryParse(heightCtrl.text);
            final tooLow =
                liteStart > 0 && entered != null && entered < liteStart;
            return AlertDialog(
              title: Text(dlgTr?.resetScanHeight ?? 'Reset Scan Height'),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(dlgTr?.resetScanHeightDescription ?? 'Enter a block height to rescan from. Use 0 for a full rescan.'),
                  if (liteStart > 0) ...[
                    const SizedBox(height: 8),
                    Text(
                      dlgTr?.liteNodeRescanHint(liteStart) ??
                          'The connected node can only rescan from block $liteStart or above.',
                      style: const TextStyle(color: kWarning, fontSize: 12, height: 1.35),
                    ),
                  ],
                  const SizedBox(height: 12),
                  TextField(
                    controller: heightCtrl,
                    onChanged: (_) => setDialogState(() {}),
                    decoration: InputDecoration(
                      labelText: dlgTr?.scanHeight ?? 'Scan height',
                      errorText: tooLow
                          ? (dlgTr?.liteNodeRescanHint(liteStart) ??
                              'The connected node can only rescan from block $liteStart or above.')
                          : null,
                    ),
                    keyboardType: TextInputType.number,
                  ),
                ],
              ),
              actions: [
                TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(dlgTr?.cancel ?? 'Cancel')),
                FilledButton(
                  onPressed: tooLow ? null : () => Navigator.pop(ctx, true),
                  child: Text(dlgTr?.reset ?? 'Reset'),
                ),
              ],
            );
          },
        );
      },
    );
    final height = int.tryParse(heightCtrl.text) ?? 0;
    heightCtrl.dispose();
    if (confirmed != true) return;
    await _runReset(height);
  }

  Future<void> _runReset(int height) async {
    try {
      await ref.read(walletCApiProvider).reset(scanHeight: height);
      ref.read(statusProvider.notifier).refresh();
    } on WalletCApiException catch (e) {
      if (!mounted) return;
      if (e.errorCode == kErrLiteNodeCannotRescanThatLow) {
        await _showLiteRescanRefused();
        return;
      }
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(e.message), backgroundColor: kError),
      );
    }
  }

  /// The wallet refused the rescan because the connected lite node holds no
  /// data that far back and the wallet already has transactions from below
  /// there — rescanning would drop them with no way to find them again here.
  /// Nothing has been changed, so the only choices are to rescan from a height
  /// this node can serve, or to connect a node holding the whole chain.
  Future<void> _showLiteRescanRefused() async {
    await ref.read(statusProvider.notifier).refresh();
    if (!mounted) return;
    final liteStart =
        ref.read(statusProvider).valueOrNull?.daemonLiteStartHeight ?? 0;

    final rescanFrom = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.liteNodeRescanRefusedTitle ?? 'This node cannot rescan that far back'),
          content: Text(
            dlgTr?.liteNodeRescanRefused(liteStart) ??
                'The connected node is a lite node holding no block data below '
                    '$liteStart. Rescanning from lower than that would drop '
                    'transactions this wallet has already found, with no way to '
                    'find them again here. Nothing has been changed.',
            style: const TextStyle(height: 1.5),
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(dlgTr?.cancel ?? 'Cancel')),
            if (liteStart > 0)
              FilledButton(
                onPressed: () => Navigator.pop(ctx, true),
                child: Text(dlgTr?.liteNodeRescanFromInstead(liteStart) ?? 'Rescan from $liteStart instead'),
              ),
          ],
        );
      },
    );

    if (rescanFrom == true && liteStart > 0) {
      await _runReset(liteStart);
    }
  }

  Future<void> _deleteWalletData() async {
    // Step 1: first confirmation
    final step1 = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.deleteWalletData ?? 'Delete Wallet Data'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Icon(Icons.warning_amber_rounded, color: kError, size: 40),
              const SizedBox(height: 12),
              Text(
                dlgTr?.deleteWalletWarning ??
                    'This will permanently delete your wallet file from disk.\n\n'
                    'Make sure you have backed up your seed phrase and private keys before proceeding. '
                    'This action cannot be undone.',
                style: const TextStyle(height: 1.5),
              ),
            ],
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(dlgTr?.cancel ?? 'Cancel')),
            TextButton(
              onPressed: () => Navigator.pop(ctx, true),
              style: TextButton.styleFrom(foregroundColor: kError),
              child: Text(dlgTr?.iUnderstandContinue ?? 'I understand, continue'),
            ),
          ],
        );
      },
    );
    if (step1 != true) return;
    if (!mounted) return;

    // Step 2: type "DELETE" to confirm
    final typeCtrl = TextEditingController();
    final step2 = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.finalConfirmation ?? 'Final Confirmation'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(dlgTr?.typeDeleteToConfirm ?? 'Type DELETE to confirm:'),
              const SizedBox(height: 10),
              TextField(
                controller: typeCtrl,
                autofocus: true,
                decoration: InputDecoration(hintText: dlgTr?.deleteHint ?? 'DELETE'),
              ),
            ],
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(dlgTr?.cancel ?? 'Cancel')),
            FilledButton(
              style: FilledButton.styleFrom(backgroundColor: kError),
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(dlgTr?.deletePermanently ?? 'Delete permanently'),
            ),
          ],
        );
      },
    );
    final typed = typeCtrl.text.trim();
    typeCtrl.dispose();
    if (step2 != true) return;
    // Tell the user why nothing happened instead of returning silently.
    if (typed != 'DELETE') {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
              content: Text(S.of(context)?.deleteConfirmMismatch ??
                  'Type DELETE exactly to confirm.'),
              backgroundColor: kError),
        );
      }
      return;
    }

    try {
      final ffi = ref.read(walletCApiProvider);
      // Save before closing so the file is in a clean state before we delete it
      try { await ffi.save(); } catch (_) {}
      await ffi.close();

      // Delete the wallet file from disk
      final walletPath = await _storage.read(key: _kLastWalletKey);
      if (walletPath != null) {
        final file = File(walletPath);
        if (await file.exists()) await file.delete();
        final keysFile = File('$walletPath.keys');
        if (await keysFile.exists()) await keysFile.delete();
      }

      await clearLastWalletPath();
      await clearWalletPassword();
      ref.read(walletOpenProvider.notifier).state = false;
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error: $e'), backgroundColor: kError),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final nodeAsync = ref.watch(statusProvider);
    final themeMode = ref.watch(themeModeProvider);
    final logLevel = ref.watch(logLevelProvider);
    final notificationsEnabled = ref.watch(notificationsEnabledProvider);
    final scanCoinbase = ref.watch(scanCoinbaseProvider);
    final autosaveEnabled = ref.watch(autosaveEnabledProvider);
    _syncPowForm(ref.watch(txPowServerProvider));

    return SingleChildScrollView(
      padding: const EdgeInsets.all(28),
      child: Center(
        child: SizedBox(
          width: 620,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(tr?.settings ?? 'Settings', style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 24),

              // -- Node section ------------------------------------------------
              _SectionHeader(title: tr?.sectionDaemonNode ?? 'Daemon Node', icon: Icons.cloud_outlined),
              const SizedBox(height: 12),

              // Node error banner (shown when node is unreachable)
              nodeAsync.whenOrNull(
                error: (e, _) => _NodeWarningBanner(
                  message: tr?.nodeUnreachable ?? 'Cannot reach the current node. Enter a new node address below and tap Apply.',
                  onSwitch: () => _scrollToNodeForm(),
                ),
              ) ?? const SizedBox.shrink(),

              Card(
                child: Padding(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        tr?.nodeDescription ?? 'Connect to a local or remote daemon node. Changes take effect immediately.',
                        style: const TextStyle(color: kTextSecondary, fontSize: 13),
                      ),
                      const SizedBox(height: 14),

                      // Quick targets, and the only thing on screen that says
                      // which node the wallet is actually on: the host field
                      // shows what will be applied, not what is connected, and
                      // those differ the moment anyone types in it.
                      //
                      // Selection is decided by the live connection, not by the
                      // fields.
                      Builder(builder: (context) {
                        final localConfig = ref.watch(localNodeProvider).config;
                        final info = ref.watch(nodeInfoProvider).valueOrNull;
                        final liveHost = info?['daemonHost'] as String?;
                        final livePort = (info?['daemonPort'] as num?)?.toInt();

                        final onLocal = localConfig != null &&
                            liveHost == kLocalNodeHost &&
                            livePort == localConfig.rpcPort;
                        final onRemote = liveHost != null && !onLocal;

                        return Wrap(
                          spacing: 8,
                          runSpacing: 8,
                          children: [
                            ChoiceChip(
                              avatar: Icon(Icons.cloud_outlined,
                                  size: 16,
                                  color: onRemote ? kPrimary : null),
                              label: Text(tr?.nodePresetRemote ?? 'Remote node'),
                              selected: onRemote,
                              selectedColor: kPrimary.withAlpha(40),
                              onSelected: (_) => setState(() {
                                _nodeHostCtrl.text = kDefaultDaemonHost;
                                _nodePortCtrl.text = '$kDefaultDaemonPort';
                                _nodeSSL = kDefaultDaemonSSL;
                              }),
                            ),
                            if (localConfig != null)
                              ChoiceChip(
                                avatar: Icon(Icons.dns_outlined,
                                    size: 16, color: onLocal ? kPrimary : null),
                                label:
                                    Text(tr?.nodePresetLocal ?? 'Local lite node'),
                                selected: onLocal,
                                selectedColor: kPrimary.withAlpha(40),
                                onSelected: (_) => setState(() {
                                  _nodeHostCtrl.text = kLocalNodeHost;
                                  _nodePortCtrl.text = '${localConfig.rpcPort}';
                                  _nodeSSL = false;
                                }),
                              ),
                          ],
                        );
                      }),
                      const SizedBox(height: 14),
                      Row(
                        children: [
                          Expanded(
                            flex: 3,
                            child: TextField(
                              controller: _nodeHostCtrl,
                              decoration: InputDecoration(labelText: tr?.hostIpAddress ?? 'Host / IP address'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: TextField(
                              controller: _nodePortCtrl,
                              decoration: InputDecoration(labelText: tr?.port ?? 'Port'),
                              keyboardType: TextInputType.number,
                            ),
                          ),
                          const SizedBox(width: 10),
                          Column(
                            children: [
                              Text(tr?.ssl ?? 'SSL', style: const TextStyle(color: kTextSecondary, fontSize: 12)),
                              Switch(value: _nodeSSL, onChanged: (v) => setState(() => _nodeSSL = v)),
                            ],
                          ),
                        ],
                      ),
                      if (_nodeError != null) ...[
                        const SizedBox(height: 10),
                        _InlineError(message: _nodeError!),
                      ],
                      if (_nodeSuccess != null) ...[
                        const SizedBox(height: 10),
                        Container(
                          padding: const EdgeInsets.all(8),
                          decoration: BoxDecoration(
                            color: kSuccess.withAlpha(25),
                            borderRadius: BorderRadius.circular(6),
                          ),
                          child: Row(children: [
                            const Icon(Icons.check_circle_outline, color: kSuccess, size: 14),
                            const SizedBox(width: 6),
                            Text(_nodeSuccess!, style: const TextStyle(color: kSuccess, fontSize: 12)),
                          ]),
                        ),
                      ],
                      const SizedBox(height: 14),
                      FilledButton(
                        onPressed: _savingNode ? null : _saveNode,
                        child: _savingNode
                            ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
                            : Text(tr?.apply ?? 'Apply'),
                      ),

                      // How far back the connected node goes. A lite node
                      // answers every scan from its own start height, so this
                      // is not a detail: it decides whether this wallet's
                      // older transactions can be seen at all. See LITENODE.md.
                      if (nodeAsync.valueOrNull case final status?) ...[
                        const Divider(height: 28),
                        Row(
                          children: [
                            Text(
                              tr?.nodeServesFromLabel ?? 'Serves blocks from',
                              style: const TextStyle(color: kTextSecondary, fontSize: 12),
                            ),
                            const SizedBox(width: 8),
                            Text(
                              status.isLiteNode
                                  ? '${status.daemonLiteStartHeight}'
                                  : (tr?.nodeFullChain ?? 'Full chain'),
                              style: TextStyle(
                                fontSize: 12,
                                color: status.isLiteNode ? kWarning : kSuccess,
                                fontWeight: FontWeight.w600,
                              ),
                            ),
                          ],
                        ),
                        // The shell already shows this banner across the top
                        // of every screen, this one included. Two copies of the
                        // same red paragraph on one page is not twice the
                        // warning, and the second one pushed the node controls
                        // off the fold. The shell's is the one that stays.
                        if (status.hasReportableSyncGap) ...[
                          const SizedBox(height: 10),
                          OutlinedButton.icon(
                            onPressed: _savingNode
                                ? null
                                : () {
                                    setState(() {
                                      _nodeHostCtrl.text = kDefaultDaemonHost;
                                      _nodePortCtrl.text =
                                          '$kDefaultDaemonPort';
                                      _nodeSSL = kDefaultDaemonSSL;
                                    });
                                    unawaited(_saveNode());
                                  },
                            icon: const Icon(Icons.cloud_outlined, size: 16),
                            label: Text(tr?.switchToRemoteNode ??
                                'Switch to the remote node'),
                          ),
                        ],
                      ],
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // -- Local lite node section -------------------------------------
              _SectionHeader(
                  title: tr?.sectionLocalNode ?? 'Local Lite Node',
                  icon: Icons.dns_outlined),
              const SizedBox(height: 12),
              LocalNodeSection(
                onNodeSwitched: (host, port) {
                  setState(() {
                    _nodeHostCtrl.text = host;
                    _nodePortCtrl.text = '$port';
                    _nodeSSL = false;
                    _nodeError = null;
                    _nodeSuccess = null;
                  });
                },
              ),
              const SizedBox(height: 24),

              // -- Transaction PoW server section ------------------------------
              _SectionHeader(title: tr?.txPowServerSection ?? 'Transaction PoW Server', icon: Icons.memory_outlined),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(tr?.txPowServerUse ?? 'Use an external PoW server', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                                const SizedBox(height: 4),
                                Text(
                                  tr?.txPowServerSubtitle ?? "Send the transaction proof of work to a server instead of computing it on this device. If the server does not respond, this device's CPU is used.",
                                  style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(width: 12),
                          Switch(
                            value: _powEnabled,
                            onChanged: (v) => setState(() => _powEnabled = v),
                          ),
                        ],
                      ),
                      if (_powEnabled) ...[
                        const SizedBox(height: 12),
                        Row(
                          children: [
                            Expanded(
                              flex: 3,
                              child: TextField(
                                controller: _powHostCtrl,
                                decoration: InputDecoration(labelText: tr?.hostIpAddress ?? 'Host / IP address'),
                              ),
                            ),
                            const SizedBox(width: 12),
                            Expanded(
                              child: TextField(
                                controller: _powPortCtrl,
                                keyboardType: TextInputType.number,
                                decoration: InputDecoration(labelText: tr?.port ?? 'Port'),
                              ),
                            ),
                            const SizedBox(width: 12),
                            Column(
                              children: [
                                Text(tr?.ssl ?? 'SSL', style: const TextStyle(color: kTextSecondary, fontSize: 12)),
                                Switch(value: _powSSL, onChanged: (v) => setState(() => _powSSL = v)),
                              ],
                            ),
                          ],
                        ),
                      ],
                      if (_powError != null) ...[
                        const SizedBox(height: 8),
                        Text(_powError!, style: const TextStyle(color: kError, fontSize: 13)),
                      ],
                      if (_powSuccess != null) ...[
                        const SizedBox(height: 8),
                        Text(_powSuccess!, style: const TextStyle(color: kSuccess, fontSize: 13)),
                      ],
                      const SizedBox(height: 12),
                      Align(
                        alignment: Alignment.centerRight,
                        child: FilledButton(
                          onPressed: _savePowServer,
                          child: Text(tr?.apply ?? 'Apply'),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // -- Wallet section ----------------------------------------------
              _SectionHeader(title: tr?.sectionWallet ?? 'Wallet', icon: Icons.account_balance_wallet_outlined),
              const SizedBox(height: 12),
              Card(
                child: Column(
                  children: [
                    _SettingsTile(
                      icon: Icons.save_outlined,
                      title: tr?.saveWallet ?? 'Save Wallet',
                      subtitle: tr?.saveWalletSubtitle ?? 'Flush current state to disk',
                      onTap: _saveWallet,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.file_download_outlined,
                      title: tr?.exportToJson ?? 'Export to JSON',
                      subtitle: tr?.exportToJsonSubtitle ?? 'Save wallet data as a JSON file',
                      onTap: _exportJson,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.sync_outlined,
                      title: tr?.resetScanHeight ?? 'Reset Scan Height',
                      subtitle: tr?.resetScanHeightSubtitle ?? 'Rescan blockchain from a specific height',
                      onTap: _resetScanHeight,
                    ),
                    const Divider(height: 1, indent: 56),
                    Padding(
                      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                      child: Row(
                        children: [
                          Icon(Icons.autorenew_outlined, size: 20, color: Theme.of(context).colorScheme.onSurfaceVariant),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(tr?.autosave ?? 'Autosave', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                                Text(tr?.autosaveSubtitle ?? 'Save wallet to disk after sync and every 5 minutes', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                              ],
                            ),
                          ),
                          Switch(
                            value: autosaveEnabled,
                            onChanged: (v) => ref.read(autosaveEnabledProvider.notifier).set(v),
                          ),
                        ],
                      ),
                    ),
                    const Divider(height: 1, indent: 56),
                    Padding(
                      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                      child: Row(
                        children: [
                          Icon(Icons.construction_outlined, size: 20, color: Theme.of(context).colorScheme.onSurfaceVariant),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(tr?.scanCoinbaseTx ?? 'Scan Coinbase Transactions', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                                Text(tr?.scanCoinbaseSubtitle ?? 'Include miner rewards when syncing (off by default)', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                              ],
                            ),
                          ),
                          Switch(
                            value: scanCoinbase,
                            onChanged: (v) {
                              ref.read(scanCoinbaseProvider.notifier).set(v);
                              ref.read(walletCApiProvider).setScanCoinbase(v);
                            },
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 24),

              // -- Appearance section ------------------------------------------
              _SectionHeader(title: tr?.sectionAppearance ?? 'Appearance', icon: Icons.palette_outlined),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                  child: Row(
                    children: [
                      const Icon(Icons.brightness_6_outlined, size: 20, color: kTextSecondary),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(tr?.theme ?? 'Theme', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                            Text(tr?.themeSubtitle ?? 'Choose app colour scheme', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                          ],
                        ),
                      ),
                      SegmentedButton<ThemeMode>(
                        segments: [
                          ButtonSegment(value: ThemeMode.system, icon: const Icon(Icons.brightness_auto, size: 16), label: Text(tr?.themeSystem ?? 'System')),
                          ButtonSegment(value: ThemeMode.light,  icon: const Icon(Icons.light_mode, size: 16),      label: Text(tr?.themeLight ?? 'Light')),
                          ButtonSegment(value: ThemeMode.dark,   icon: const Icon(Icons.dark_mode, size: 16),       label: Text(tr?.themeDark ?? 'Dark')),
                        ],
                        selected: {themeMode},
                        onSelectionChanged: (s) => ref.read(themeModeProvider.notifier).set(s.first),
                        style: ButtonStyle(visualDensity: VisualDensity.compact),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // -- Notifications section ---------------------------------------
              _SectionHeader(title: tr?.sectionNotifications ?? 'Notifications', icon: Icons.notifications_outlined),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                  child: Row(
                    children: [
                      const Icon(Icons.notifications_active_outlined, size: 20, color: kTextSecondary),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(tr?.incomingTxAlerts ?? 'Incoming Transaction Alerts', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                            Text(tr?.incomingTxAlertsSubtitle ?? 'Show a desktop notification when WRKZ is received', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                          ],
                        ),
                      ),
                      Switch(
                        value: notificationsEnabled,
                        onChanged: (v) => ref.read(notificationsEnabledProvider.notifier).set(v),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // -- Debug / Logs section ----------------------------------------
              _SectionHeader(title: tr?.sectionDebugLogs ?? 'Debug & Logs', icon: Icons.bug_report_outlined),
              const SizedBox(height: 12),
              Card(
                child: Column(
                  children: [
                    Padding(
                      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                      child: Row(
                        children: [
                          const Icon(Icons.tune_outlined, size: 20, color: kTextSecondary),
                          const SizedBox(width: 12),
                          Expanded(
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text(tr?.logLevel ?? 'Log Level', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                                Text(tr?.logLevelSubtitle ?? 'Controls wallet library verbosity', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                              ],
                            ),
                          ),
                          DropdownButton<WalletLogLevel>(
                            value: logLevel,
                            underline: const SizedBox.shrink(),
                            items: WalletLogLevel.values
                                .map((l) => DropdownMenuItem(value: l, child: Text(l.label, style: const TextStyle(fontSize: 13))))
                                .toList(),
                            onChanged: (l) {
                              if (l != null) {
                                ref.read(logLevelProvider.notifier).set(l);
                              }
                            },
                          ),
                        ],
                      ),
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.article_outlined,
                      title: tr?.viewLogs ?? 'View Logs',
                      subtitle: tr?.viewLogsSubtitle ?? 'Live wallet library log output',
                      onTap: () => _showLogViewer(context),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 24),

              // -- Danger zone -------------------------------------------------
              _SectionHeader(title: tr?.sectionDangerZone ?? 'Danger Zone', icon: Icons.warning_amber_outlined, color: kError),
              const SizedBox(height: 12),
              Card(
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                  side: const BorderSide(color: kError, width: 1),
                ),
                child: _SettingsTile(
                  icon: Icons.delete_forever_outlined,
                  title: tr?.deleteWalletData ?? 'Delete Wallet Data',
                  subtitle: tr?.deleteWalletDataSubtitle ?? 'Permanently remove wallet file from disk',
                  iconColor: kError,
                  titleColor: kError,
                  onTap: _deleteWalletData,
                ),
              ),
              const SizedBox(height: 32),
            ],
          ),
        ),
      ),
    );
  }

  void _showLogViewer(BuildContext context) {
    showDialog(
      context: context,
      builder: (_) => _LogViewerDialog(ffi: ref.read(walletCApiProvider)),
    );
  }

  void _scrollToNodeForm() {
    _nodeHostCtrl.selection = TextSelection(
        baseOffset: 0, extentOffset: _nodeHostCtrl.text.length);
  }
}

// -- Local helper widgets -----------------------------------------------------

class _SectionHeader extends StatelessWidget {
  final String title;
  final IconData icon;
  final Color? color;

  const _SectionHeader({
    required this.title,
    required this.icon,
    this.color,
  });

  @override
  Widget build(BuildContext context) {
    final c = color ?? Theme.of(context).colorScheme.onSurface;
    return Row(
      children: [
        Icon(icon, size: 16, color: c),
        const SizedBox(width: 8),
        Text(title, style: TextStyle(color: c, fontSize: 14, fontWeight: FontWeight.w600)),
      ],
    );
  }
}

class _SettingsTile extends StatelessWidget {
  final IconData icon;
  final String title;
  final String subtitle;
  final VoidCallback onTap;
  final Color? iconColor;
  final Color? titleColor;

  const _SettingsTile({
    required this.icon,
    required this.title,
    required this.subtitle,
    required this.onTap,
    this.iconColor,
    this.titleColor,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    return ListTile(
      leading: Icon(icon, size: 20, color: iconColor ?? cs.onSurfaceVariant),
      title: Text(title, style: TextStyle(color: titleColor ?? cs.onSurface, fontSize: 14)),
      subtitle: Text(subtitle, style: TextStyle(color: cs.onSurfaceVariant, fontSize: 12)),
      trailing: Icon(Icons.chevron_right, size: 16, color: cs.onSurfaceVariant),
      onTap: onTap,
    );
  }
}

class _NodeWarningBanner extends StatelessWidget {
  final String message;
  final VoidCallback onSwitch;
  const _NodeWarningBanner({required this.message, required this.onSwitch});

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: kError.withAlpha(20),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: kError.withAlpha(80)),
      ),
      child: Row(
        children: [
          const Icon(Icons.cloud_off_outlined, color: kError, size: 18),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              message,
              style: const TextStyle(color: kError, fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }
}

class _InlineError extends StatelessWidget {
  final String message;
  const _InlineError({required this.message});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(8),
      decoration: BoxDecoration(
        color: kError.withAlpha(25),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: kError, size: 14),
          const SizedBox(width: 6),
          Expanded(child: Text(message, style: const TextStyle(color: kError, fontSize: 12))),
        ],
      ),
    );
  }
}

// -- Log viewer dialog --------------------------------------------------------

class _LogEntry {
  final String pretty;
  final String level;
  final int ts;
  const _LogEntry({required this.pretty, required this.level, required this.ts});

  factory _LogEntry.fromJson(Map<String, dynamic> j) => _LogEntry(
        pretty: j['pretty'] as String? ?? '',
        level: j['level'] as String? ?? 'info',
        ts: (j['ts'] as num? ?? 0).toInt(),
      );
}

class _LogViewerDialog extends StatefulWidget {
  final WalletCApi ffi;
  const _LogViewerDialog({required this.ffi});

  @override
  State<_LogViewerDialog> createState() => _LogViewerDialogState();
}

class _LogViewerDialogState extends State<_LogViewerDialog> {
  final List<_LogEntry> _entries = [];
  final ScrollController _scroll = ScrollController();
  Timer? _timer;
  bool _autoScroll = true;

  static const _levelColors = {
    'fatal': kError,
    'error': kError,
    'warning': kWarning,
    'info': kTextPrimary,
    'debug': kTextSecondary,
    'trace': kTextDisabled,
  };

  @override
  void initState() {
    super.initState();
    _poll();
    _timer = Timer.periodic(const Duration(seconds: 2), (_) => _poll());
  }

  @override
  void dispose() {
    _timer?.cancel();
    _scroll.dispose();
    super.dispose();
  }

  static const _kMaxLogEntries = 2000;

  void _poll() {
    try {
      final data = widget.ffi.takeLogs();
      final list = (data['entries'] as List<dynamic>? ?? [])
          .map((e) => _LogEntry.fromJson(e as Map<String, dynamic>))
          .toList();
      if (list.isEmpty) return;
      setState(() {
        _entries.addAll(list);
        // The native side keeps 2000 entries; match it so a trace-level
        // session cannot grow this list without bound.
        if (_entries.length > _kMaxLogEntries) {
          _entries.removeRange(0, _entries.length - _kMaxLogEntries);
        }
      });
      if (_autoScroll) {
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (_scroll.hasClients) {
            _scroll.jumpTo(_scroll.position.maxScrollExtent);
          }
        });
      }
    } catch (_) {}
  }

  void _clear() => setState(() => _entries.clear());

  void _copyAll(BuildContext context) {
    final tr = S.of(context);
    final text = _entries.map((e) => e.pretty).join('\n');
    Clipboard.setData(ClipboardData(text: text));
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(tr?.logsCopied ?? 'Logs copied to clipboard'), duration: const Duration(seconds: 2)),
    );
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final count = _entries.length;
    return Dialog(
      child: SizedBox(
        width: 800,
        height: 560,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Title bar
            Padding(
              padding: const EdgeInsets.fromLTRB(20, 16, 12, 0),
              child: Row(
                children: [
                  const Icon(Icons.article_outlined, size: 18, color: kTextSecondary),
                  const SizedBox(width: 8),
                  Text(tr?.walletLogs ?? 'Wallet Logs', style: Theme.of(context).textTheme.titleMedium),
                  const Spacer(),
                  Text(tr?.logEntries(count) ?? '$count entries',
                      style: const TextStyle(fontSize: 12, color: kTextDisabled)),
                  const SizedBox(width: 12),
                  // Auto-scroll toggle
                  Row(
                    children: [
                      Text(tr?.autoScroll ?? 'Auto-scroll', style: const TextStyle(fontSize: 12, color: kTextSecondary)),
                      const SizedBox(width: 4),
                      Switch(
                        value: _autoScroll,
                        onChanged: (v) => setState(() => _autoScroll = v),
                        materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                      ),
                    ],
                  ),
                  const SizedBox(width: 8),
                  IconButton(
                    icon: const Icon(Icons.copy_outlined, size: 18),
                    tooltip: tr?.copyAll ?? 'Copy all',
                    onPressed: _entries.isEmpty ? null : () => _copyAll(context),
                  ),
                  IconButton(
                    icon: const Icon(Icons.delete_sweep_outlined, size: 18),
                    tooltip: tr?.clear ?? 'Clear',
                    onPressed: _entries.isEmpty ? null : _clear,
                  ),
                  IconButton(
                    icon: const Icon(Icons.close, size: 18),
                    tooltip: tr?.close ?? 'Close',
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
            ),
            const Divider(),
            // Log list
            Expanded(
              child: _entries.isEmpty
                  ? Center(
                      child: Text(tr?.noLogsYet ?? 'No logs yet. Set a log level above Disabled to see output.',
                          style: const TextStyle(color: kTextDisabled, fontSize: 13)),
                    )
                  : ListView.builder(
                      controller: _scroll,
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                      itemCount: _entries.length,
                      itemBuilder: (_, i) {
                        final e = _entries[i];
                        final color = _levelColors[e.level] ?? kTextPrimary;
                        return SelectableText(
                          e.pretty,
                          style: TextStyle(
                            fontSize: 11,
                            fontFamily: 'monospace',
                            color: color,
                            height: 1.5,
                          ),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
