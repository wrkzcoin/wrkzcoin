import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../core/storage/wallet_registry.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/copy_button.dart';
import '../../shared/widgets/language_selector.dart';
import '../../shared/widgets/lite_node_banner.dart';

class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  // Node config
  int _nodePresetIndex = 0;
  bool _customNode = false;
  final _customHostCtrl = TextEditingController();
  final _customPortCtrl = TextEditingController(text: '17856');
  bool _customSsl = false;
  bool _nodeLoading = false;
  String? _nodeMsg;
  bool _nodeMsgIsError = false;

  // Tx PoW server
  final _powHostCtrl = TextEditingController();
  final _powPortCtrl = TextEditingController(text: '17870');
  bool _powEnabled = false;
  bool _powSsl = false;
  bool _powFormLoaded = false;
  String? _powMsg;
  bool _powMsgIsError = false;

  // Reset scan
  final _resetHeightCtrl = TextEditingController(text: '0');

  @override
  void initState() {
    super.initState();
    _loadCurrentNode();
  }

  @override
  void dispose() {
    _customHostCtrl.dispose();
    _customPortCtrl.dispose();
    _powHostCtrl.dispose();
    _powPortCtrl.dispose();
    _resetHeightCtrl.dispose();
    super.dispose();
  }

  /// Prefills the PoW server form once the stored setting has been read, and
  /// leaves it alone afterwards so typing is not overwritten by rebuilds.
  void _syncPowForm(TxPowServerSettings s) {
    if (_powFormLoaded || !s.loaded) return;
    _powFormLoaded = true;
    _powEnabled = s.enabled;
    _powSsl = s.ssl;
    _powHostCtrl.text = s.host;
    _powPortCtrl.text = '${s.port}';
  }

  Future<void> _applyPowServer() async {
    final tr = S.of(context)!;
    final host = _powHostCtrl.text.trim();
    final port = int.tryParse(_powPortCtrl.text.trim()) ?? 0;
    if (_powEnabled && (host.isEmpty || port <= 0 || port > 65535)) {
      setState(() {
        _powMsg = tr.txPowServerInvalid;
        _powMsgIsError = true;
      });
      return;
    }
    final settings = TxPowServerSettings(
      enabled: _powEnabled,
      host: host,
      port: port > 0 ? port : kDefaultTxPowServerPort,
      ssl: _powSsl,
    );
    await ref.read(txPowServerProvider.notifier).set(settings);
    settings.applyTo(ref.read(walletCApiProvider));
    hapticMedium();
    if (!mounted) return;
    setState(() {
      _powMsg = tr.txPowServerSaved;
      _powMsgIsError = false;
    });
  }

  void _loadCurrentNode() {
    final nodeInfo = ref.read(nodeInfoProvider).valueOrNull;
    if (nodeInfo != null) {
      final host = nodeInfo['daemonHost'] as String? ?? '';
      final port = nodeInfo['daemonPort'] as int? ?? 17856;
      // Check if it matches a preset
      for (var i = 0; i < AppConfig.nodePresets.length; i++) {
        final p = AppConfig.nodePresets[i];
        if (p.host == host && p.port == port) {
          _nodePresetIndex = i;
          _customNode = false;
          return;
        }
      }
      _customNode = true;
      _customHostCtrl.text = host;
      _customPortCtrl.text = port.toString();
    }
  }

  Future<void> _applyNode() async {
    final tr = S.of(context)!;
    String host;
    int port;
    bool ssl;

    if (_customNode) {
      host = _customHostCtrl.text.trim();
      port = int.tryParse(_customPortCtrl.text) ?? 17856;
      ssl = _customSsl;
      if (host.isEmpty) {
        setState(() {
          _nodeMsg = tr.hostRequired;
          _nodeMsgIsError = true;
        });
        return;
      }
    } else {
      final preset = AppConfig.nodePresets[_nodePresetIndex];
      host = preset.host;
      port = preset.port;
      ssl = preset.ssl;
    }

    setState(() {
      _nodeLoading = true;
      _nodeMsg = null;
    });

    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.swapNode(host, port, ssl: ssl);
      ref.read(nodeInfoProvider.notifier).refresh();
      ref.read(statusProvider.notifier).refresh();
      hapticMedium();
      setState(() {
        _nodeLoading = false;
        _nodeMsg = tr.nodeUpdated(host, port);
        _nodeMsgIsError = false;
      });
    } catch (e) {
      setState(() {
        _nodeLoading = false;
        _nodeMsg = e.toString();
        _nodeMsgIsError = true;
      });
    }
  }

  Future<void> _saveWallet() async {
    final tr = S.of(context)!;
    try {
      await ref.read(walletCApiProvider).save();
      hapticLight();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.walletSaved)),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.saveFailed(e.toString()))),
        );
      }
    }
  }

  /// Rescan from the height in the field.
  ///
  /// Against a lite node anything below that node's start height is refused
  /// here rather than sent: the node holds nothing down there and would answer
  /// from its own start whatever it was asked for, which would move the
  /// wallet's recorded position over blocks nobody ever looked at. The native
  /// side refuses it too (error 62) — the daemon can be swapped between this
  /// check and the call. See LITENODE.md.
  Future<void> _resetScan() async {
    final tr = S.of(context)!;
    final height = int.tryParse(_resetHeightCtrl.text) ?? 0;
    final liteStart =
        ref.read(statusProvider).valueOrNull?.daemonLiteStartHeight ?? 0;

    if (liteStart > 0 && height < liteStart) {
      await _showLiteRescanRefused(liteStart);
      return;
    }

    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr.resetScanHeight),
        content: Text(tr.resetScanConfirm(height)),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(tr.cancel),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(tr.reset),
          ),
        ],
      ),
    );
    if (confirmed != true) return;

    await _runReset(height);
  }

  Future<void> _runReset(int height) async {
    final tr = S.of(context)!;
    try {
      await ref.read(walletCApiProvider).reset(scanHeight: height);
      hapticMedium();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.scanResetTo(height))),
        );
      }
    } on WalletCApiException catch (e) {
      if (!mounted) return;
      if (e.errorCode == kErrLiteNodeCannotRescanThatLow) {
        await ref.read(statusProvider.notifier).refresh();
        if (!mounted) return;
        await _showLiteRescanRefused(
            ref.read(statusProvider).valueOrNull?.daemonLiteStartHeight ?? 0);
        return;
      }
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(tr.resetFailed(e.message))),
      );
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.resetFailed(e.toString()))),
        );
      }
    }
  }

  /// The connected node is a lite node that cannot serve the requested range.
  /// Nothing has been changed, so the only choices are to rescan from a height
  /// it can serve, or to connect a node holding the whole chain.
  Future<void> _showLiteRescanRefused(int liteStart) async {
    final tr = S.of(context)!;
    final rescanFrom = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr.liteNodeRescanRefusedTitle),
        content: Text(tr.liteNodeRescanRefused(liteStart),
            style: const TextStyle(height: 1.5)),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(tr.cancel),
          ),
          if (liteStart > 0)
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(tr.liteNodeRescanFromInstead(liteStart)),
            ),
        ],
      ),
    );

    if (rescanFrom == true && liteStart > 0) {
      _resetHeightCtrl.text = '$liteStart';
      await _runReset(liteStart);
    }
  }

  Future<void> _backupSeed() async {
    final tr = S.of(context)!;
    // Re-authenticate first
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    final passCtrl = TextEditingController();
    try {
      final ok = await showDialog<bool>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(tr.enterPasswordTitle),
          content: TextField(
            controller: passCtrl,
            obscureText: true,
            decoration: InputDecoration(hintText: tr.password),
            autofocus: true,
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(tr.cancel),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(tr.confirm),
            ),
          ],
        ),
      );
      if (ok != true) return;

      // null means "cannot tell" (no verifier recorded) — the wallet is
      // already open and unlocked, so don't refuse on a missing keychain
      // entry; only a definite mismatch is a rejection.
      final verified =
          await verifyPasswordAgainstVerifier(filename, passCtrl.text);
      if (verified == false) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(tr.incorrectPassword)),
          );
        }
        return;
      }
    } finally {
      passCtrl.dispose();
    }

    try {
      final ffi = ref.read(walletCApiProvider);
      final seed = await ffi.getMnemonicSeed();
      final viewKey = await ffi.getPrivateViewKey();

      if (!mounted) return;
      await showDialog(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(tr.seedBackup),
          content: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(tr.seedPhraseColon,
                    style: const TextStyle(fontWeight: FontWeight.bold)),
                const SizedBox(height: 4),
                Row(
                  children: [
                    Expanded(
                      child: SelectableText(seed,
                          style: const TextStyle(
                              fontFamily: 'monospace', fontSize: 12)),
                    ),
                    CopyButton(text: seed, size: 18),
                  ],
                ),
                const SizedBox(height: 16),
                Text(tr.privateViewKeyColon,
                    style: const TextStyle(fontWeight: FontWeight.bold)),
                const SizedBox(height: 4),
                Row(
                  children: [
                    Expanded(
                      child: SelectableText(viewKey,
                          style: const TextStyle(
                              fontFamily: 'monospace', fontSize: 11)),
                    ),
                    CopyButton(text: viewKey, size: 18),
                  ],
                ),
              ],
            ),
          ),
          actions: [
            FilledButton(
              onPressed: () async {
                await markSeedBackupConfirmed(filename);
                if (ctx.mounted) Navigator.pop(ctx);
              },
              child: Text(tr.iveBackedUp),
            ),
          ],
        ),
      );
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.errorPrefix(e.toString()))),
        );
      }
    }
  }

  Future<void> _changePassword() async {
    final tr = S.of(context)!;
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    final currentCtrl = TextEditingController();
    final newCtrl = TextEditingController();
    final confirmCtrl = TextEditingController();
    try {
      await _changePasswordFlow(tr, filename, currentCtrl, newCtrl, confirmCtrl);
    } finally {
      currentCtrl.dispose();
      newCtrl.dispose();
      confirmCtrl.dispose();
    }
  }

  Future<void> _changePasswordFlow(
    S tr,
    String filename,
    TextEditingController currentCtrl,
    TextEditingController newCtrl,
    TextEditingController confirmCtrl,
  ) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr.changePassword),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: currentCtrl,
              obscureText: true,
              decoration: InputDecoration(labelText: tr.currentPasswordLabel),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: newCtrl,
              obscureText: true,
              decoration: InputDecoration(labelText: tr.newPasswordLabel),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: confirmCtrl,
              obscureText: true,
              decoration:
                  InputDecoration(labelText: tr.confirmNewPasswordLabel),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(tr.cancel),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(tr.change),
          ),
        ],
      ),
    );
    if (ok != true) return;

    final verified =
        await verifyPasswordAgainstVerifier(filename, currentCtrl.text);
    if (verified == false) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.currentPasswordIncorrect)),
        );
      }
      return;
    }
    if (newCtrl.text != confirmCtrl.text) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.newPasswordsDoNotMatch)),
        );
      }
      return;
    }
    if (newCtrl.text.length < AppConfig.minPasswordLength) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
              content: Text(tr.passwordTooShort(AppConfig.minPasswordLength))),
        );
      }
      return;
    }

    try {
      await ref.read(walletCApiProvider).changePassword(newCtrl.text);
      // Only re-record after the native call succeeded, or the stored material
      // would describe a password the wallet file no longer accepts.
      await storePasswordVerifier(filename, newCtrl.text);
      if (ref.read(biometricEnabledProvider)) {
        await storeWalletPassword(filename, newCtrl.text);
      }
      hapticMedium();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.passwordChanged)),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.errorPrefix(e.toString()))),
        );
      }
    }
  }

  Future<void> _switchWallet() async {
    final ffi = ref.read(walletCApiProvider);
    try {
      await ffi.save();
    } catch (_) {
      // Fall through: the wallet still has to be closed even if the save
      // failed, otherwise the handle leaks and its synchronizer keeps running.
    } finally {
      await ffi.close();
    }

    ref.read(walletOpenProvider.notifier).state = false;
    ref.read(walletLockedProvider.notifier).state = false;
    ref.read(activeWalletFilenameProvider.notifier).state = null;
    if (mounted) context.go('/picker');
  }

  Future<void> _manageWallets() async {
    final registry = ref.read(walletRegistryProvider);
    final currentFilename = ref.read(activeWalletFilenameProvider);

    await showDialog(
      context: context,
      builder: (ctx) => _ManageWalletsDialog(
        registry: registry,
        currentFilename: currentFilename,
      ),
    );
  }

  Future<void> _deleteCurrentWallet() async {
    final tr = S.of(context)!;
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    // Deleting the file is unrecoverable without the seed, so say so loudly
    // when the user never confirmed a backup.
    final backedUp = await isSeedBackupConfirmed(filename);
    if (!mounted) return;

    final deleteCtrl = TextEditingController();
    final bool confirmed;
    try {
      confirmed = await showDialog<bool>(
            context: context,
            builder: (ctx) => AlertDialog(
              title: Text(tr.deleteWallet),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  if (!backedUp) ...[
                    Container(
                      padding: const EdgeInsets.all(12),
                      decoration: BoxDecoration(
                        color: kError.withAlpha(25),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Row(
                        children: [
                          const Icon(Icons.warning_amber,
                              color: kError, size: 18),
                          const SizedBox(width: 10),
                          Expanded(
                            child: Text(tr.seedNotBackedUpWarning,
                                style: const TextStyle(
                                    color: kError, fontSize: 13)),
                          ),
                        ],
                      ),
                    ),
                    const SizedBox(height: 12),
                  ],
                  Text(tr.deleteWalletTypeCaps),
                  const SizedBox(height: 12),
                  TextField(
                    controller: deleteCtrl,
                    decoration: InputDecoration(hintText: tr.deleteHint),
                    autofocus: true,
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(ctx, false),
                  child: Text(tr.cancel),
                ),
                FilledButton(
                  style: FilledButton.styleFrom(backgroundColor: kError),
                  onPressed: () => Navigator.pop(ctx, true),
                  child: Text(tr.delete),
                ),
              ],
            ),
          ) ??
          false;
      if (!confirmed) return;
      // Say why nothing happened rather than silently returning.
      if (deleteCtrl.text.trim() != 'DELETE') {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(tr.deleteConfirmMismatch)),
          );
        }
        return;
      }
    } finally {
      deleteCtrl.dispose();
    }

    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.close();
      final registry = ref.read(walletRegistryProvider);
      await registry.deleteWallet(filename);
      await clearWalletPassword(filename);
      await clearPasswordVerifier(filename);
      await clearSeedBackupFlag(filename);
      hapticHeavy();

      ref.read(walletOpenProvider.notifier).state = false;
      ref.read(walletLockedProvider.notifier).state = false;
      ref.read(activeWalletFilenameProvider.notifier).state = null;
      if (mounted) context.go('/picker');
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.errorPrefix(e.toString()))),
        );
      }
    }
  }

  /// Turning biometric unlock on escrows the wallet password, because
  /// unlocking with a fingerprint has to hand the real password to the native
  /// layer. Turning it off wipes that copy again — it is the only reason the
  /// password is kept at all.
  Future<void> _setBiometric(bool enabled) async {
    final tr = S.of(context)!;
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    if (!enabled) {
      await clearWalletPassword(filename);
      ref.read(biometricEnabledProvider.notifier).set(false);
      return;
    }

    if (!await isBiometricAvailable()) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr.biometricNotAvailable)),
        );
      }
      return;
    }
    if (!mounted) return;

    final passCtrl = TextEditingController();
    try {
      final ok = await showDialog<bool>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(tr.enterPasswordTitle),
          content: TextField(
            controller: passCtrl,
            obscureText: true,
            autofocus: true,
            decoration: InputDecoration(hintText: tr.password),
          ),
          actions: [
            TextButton(
                onPressed: () => Navigator.pop(ctx, false),
                child: Text(tr.cancel)),
            FilledButton(
                onPressed: () => Navigator.pop(ctx, true),
                child: Text(tr.confirm)),
          ],
        ),
      );
      if (ok != true) return;

      final verified =
          await verifyPasswordAgainstVerifier(filename, passCtrl.text);
      if (verified == false) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(tr.incorrectPassword)),
          );
        }
        return;
      }
      await storeWalletPassword(filename, passCtrl.text);
      ref.read(biometricEnabledProvider.notifier).set(true);
    } finally {
      passCtrl.dispose();
    }
  }

  // ── build ──────────────────────────────────────────────────────────────────

  String _autoLockLabel(S tr, int index) {
    return switch (index) {
      0 => tr.autoLockImmediately,
      1 => tr.autoLock1Min,
      2 => tr.autoLock5Min,
      3 => tr.autoLockNever,
      _ => '',
    };
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    final themeMode = ref.watch(themeModeProvider);
    final notifications = ref.watch(notificationsEnabledProvider);
    final autosave = ref.watch(autosaveEnabledProvider);
    final biometric = ref.watch(biometricEnabledProvider);
    final autoLockIdx = ref.watch(autoLockIndexProvider);
    final scanCoinbase = ref.watch(scanCoinbaseProvider);
    final status = ref.watch(statusProvider).valueOrNull;
    _syncPowForm(ref.watch(txPowServerProvider));
    final walletCaption = () {
      final fn = ref.read(activeWalletFilenameProvider);
      if (fn == null) return 'Wallet';
      return ref.read(walletRegistryProvider).findByFilename(fn)?.caption ??
          'Wallet';
    }();

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // ── Daemon Node ──────────────────────────────────────────────────
        _sectionTitle(tr.sectionDaemonNode),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                RadioGroup<int>(
                  groupValue: _customNode ? -1 : _nodePresetIndex,
                  onChanged: (v) => setState(() {
                    if (v == null || v == -1) {
                      _customNode = true;
                    } else {
                      _nodePresetIndex = v;
                      _customNode = false;
                    }
                  }),
                  child: Column(
                    children: [
                      ...List.generate(AppConfig.nodePresets.length, (i) {
                        final preset = AppConfig.nodePresets[i];
                        return RadioListTile<int>(
                          value: i,
                          title: Text(preset.label,
                              style: Theme.of(context).textTheme.bodyMedium),
                          subtitle: Text('${preset.host}:${preset.port}',
                              style: Theme.of(context).textTheme.bodySmall),
                          dense: true,
                          contentPadding: EdgeInsets.zero,
                        );
                      }),
                      RadioListTile<int>(
                        value: -1,
                        title: Text(tr.custom,
                            style: Theme.of(context).textTheme.bodyMedium),
                        dense: true,
                        contentPadding: EdgeInsets.zero,
                      ),
                    ],
                  ),
                ),
                if (_customNode) ...[
                  const SizedBox(height: 8),
                  TextField(
                    controller: _customHostCtrl,
                    decoration: InputDecoration(
                        hintText: tr.hostHint, labelText: tr.host),
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _customPortCtrl,
                          keyboardType: TextInputType.number,
                          decoration: InputDecoration(
                              hintText: '17856', labelText: tr.port),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Row(
                        children: [
                          Checkbox(
                            value: _customSsl,
                            onChanged: (v) =>
                                setState(() => _customSsl = v ?? false),
                          ),
                          Text(tr.ssl),
                        ],
                      ),
                    ],
                  ),
                ],
                // What the node actually holds. A lite node answers every
                // scan from its own start height, so how far back it goes is
                // not a detail — it decides whether this wallet's older
                // transactions can be seen at all. See LITENODE.md.
                if (status != null) ...[
                  const Divider(height: 24),
                  Row(
                    children: [
                      Expanded(
                        child: Text(tr.nodeServesFromLabel,
                            style: Theme.of(context).textTheme.bodySmall),
                      ),
                      Text(
                        status.isLiteNode
                            ? '${status.daemonLiteStartHeight}'
                            : tr.nodeFullChain,
                        style: Theme.of(context)
                            .textTheme
                            .bodyMedium
                            ?.copyWith(
                                color: status.isLiteNode ? kWarning : kSuccess),
                      ),
                    ],
                  ),
                  if (status.isLiteNode ||
                      status.hasReportableSyncGap) ...[
                    const SizedBox(height: 10),
                    LiteNodeBanner(status: status),
                  ],
                ],
                const SizedBox(height: 12),
                Text(
                  tr.localNodeMobileFuture,
                  style: Theme.of(context).textTheme.bodySmall,
                ),
                if (_nodeMsg != null) ...[
                  const SizedBox(height: 8),
                  Text(_nodeMsg!,
                      style: TextStyle(
                        color: _nodeMsgIsError ? kError : kSuccess,
                        fontSize: 13,
                      )),
                ],
                const SizedBox(height: 12),
                FilledButton(
                  onPressed: _nodeLoading ? null : _applyNode,
                  child: _nodeLoading
                      ? const SizedBox(
                          width: 18,
                          height: 18,
                          child: CircularProgressIndicator(
                              strokeWidth: 2, color: Colors.white),
                        )
                      : Text(tr.apply),
                ),
              ],
            ),
          ),
        ),

        const SizedBox(height: 24),

        // ── Transaction PoW server ───────────────────────────────────────
        _sectionTitle(tr.txPowServerSection),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                SwitchListTile(
                  title: Text(tr.txPowServerUse,
                      style: Theme.of(context).textTheme.bodyMedium),
                  subtitle: Text(tr.txPowServerSubtitle,
                      style: Theme.of(context).textTheme.bodySmall),
                  value: _powEnabled,
                  contentPadding: EdgeInsets.zero,
                  onChanged: (v) => setState(() => _powEnabled = v),
                ),
                if (_powEnabled) ...[
                  const SizedBox(height: 8),
                  TextField(
                    controller: _powHostCtrl,
                    decoration: InputDecoration(
                        hintText: tr.hostHint, labelText: tr.host),
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _powPortCtrl,
                          keyboardType: TextInputType.number,
                          decoration: InputDecoration(
                              hintText: '17870', labelText: tr.port),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Row(
                        children: [
                          Checkbox(
                            value: _powSsl,
                            onChanged: (v) =>
                                setState(() => _powSsl = v ?? false),
                          ),
                          Text(tr.ssl),
                        ],
                      ),
                    ],
                  ),
                ],
                if (_powMsg != null) ...[
                  const SizedBox(height: 8),
                  Text(_powMsg!,
                      style: TextStyle(
                        color: _powMsgIsError ? kError : kSuccess,
                        fontSize: 13,
                      )),
                ],
                const SizedBox(height: 12),
                FilledButton(
                  onPressed: _applyPowServer,
                  child: Text(tr.apply),
                ),
              ],
            ),
          ),
        ),

        const SizedBox(height: 24),

        // ── Current Wallet ───────────────────────────────────────────────
        _sectionTitle(tr.currentWallet(walletCaption)),
        Card(
          child: Column(
            children: [
              ListTile(
                leading: const Icon(Icons.save),
                title: Text(tr.saveWallet),
                onTap: _saveWallet,
              ),
              ListTile(
                leading: const Icon(Icons.vpn_key_outlined),
                title: Text(tr.backupSeed),
                onTap: _backupSeed,
              ),
              ListTile(
                leading: const Icon(Icons.lock_reset),
                title: Text(tr.changePassword),
                onTap: _changePassword,
              ),
              ListTile(
                leading: const Icon(Icons.restart_alt),
                title: Text(tr.resetScanHeight),
                subtitle: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    SizedBox(
                      width: 120,
                      child: TextField(
                        controller: _resetHeightCtrl,
                        keyboardType: TextInputType.number,
                        decoration: const InputDecoration(
                          hintText: '0',
                          isDense: true,
                          contentPadding:
                              EdgeInsets.symmetric(horizontal: 8, vertical: 8),
                        ),
                      ),
                    ),
                    // Say the floor before the button is pressed, not after
                    // the wallet has refused the request.
                    if (status != null && status.isLiteNode) ...[
                      const SizedBox(height: 6),
                      Text(
                        tr.liteNodeRescanHint(status.daemonLiteStartHeight),
                        style: Theme.of(context)
                            .textTheme
                            .bodySmall
                            ?.copyWith(color: kWarning),
                      ),
                    ],
                  ],
                ),
                trailing: TextButton(
                  onPressed: _resetScan,
                  child: Text(tr.reset),
                ),
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Wallet Management ────────────────────────────────────────────
        _sectionTitle(tr.walletManagement),
        Card(
          child: Column(
            children: [
              ListTile(
                leading: const Icon(Icons.swap_horiz),
                title: Text(tr.switchWallet),
                subtitle: Text(tr.switchWalletSubtitle),
                onTap: _switchWallet,
              ),
              ListTile(
                leading: const Icon(Icons.folder_open),
                title: Text(tr.manageWallets),
                subtitle: Text(tr.manageWalletsSubtitle),
                onTap: _manageWallets,
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Security ─────────────────────────────────────────────────────
        _sectionTitle(tr.security),
        Card(
          child: Column(
            children: [
              SwitchListTile(
                title: Text(tr.biometricUnlock),
                subtitle: Text(tr.biometricSubtitle),
                value: biometric,
                onChanged: (v) => _setBiometric(v),
              ),
              ListTile(
                title: Text(tr.autoLock),
                subtitle: Text(_autoLockLabel(tr, autoLockIdx)),
                trailing: DropdownButton<int>(
                  value: autoLockIdx,
                  underline: const SizedBox.shrink(),
                  items: List.generate(
                    AppConfig.autoLockOptions.length,
                    (i) => DropdownMenuItem(
                      value: i,
                      child: Text(_autoLockLabel(tr, i)),
                    ),
                  ),
                  onChanged: (v) {
                    if (v != null) {
                      ref.read(autoLockIndexProvider.notifier).set(v);
                    }
                  },
                ),
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Language ─────────────────────────────────────────────────────
        _sectionTitle(tr.language),
        Card(
          child: ListTile(
            leading: Text(currentLangInfo(ref.watch(localeProvider)).flag,
                style: const TextStyle(fontSize: 24)),
            title: Text(tr.language),
            subtitle:
                Text(currentLangInfo(ref.watch(localeProvider)).nativeName),
            trailing: const Icon(Icons.chevron_right),
            onTap: () => showLanguagePicker(context, ref),
          ),
        ),
        const SizedBox(height: 24),

        // ── Appearance ───────────────────────────────────────────────────
        _sectionTitle(tr.appearance),
        Card(
          child: ListTile(
            title: Text(tr.theme),
            trailing: SegmentedButton<ThemeMode>(
              segments: [
                ButtonSegment(
                    value: ThemeMode.system, label: Text(tr.themeAuto)),
                ButtonSegment(
                    value: ThemeMode.light, label: Text(tr.themeLight)),
                ButtonSegment(
                    value: ThemeMode.dark, label: Text(tr.themeDark)),
              ],
              selected: {themeMode},
              onSelectionChanged: (s) =>
                  ref.read(themeModeProvider.notifier).set(s.first),
            ),
          ),
        ),

        const SizedBox(height: 24),

        // ── Notifications & Autosave ─────────────────────────────────────
        _sectionTitle(tr.preferences),
        Card(
          child: Column(
            children: [
              SwitchListTile(
                title: Text(tr.transactionNotifications),
                subtitle: Text(tr.notificationsSubtitle),
                value: notifications,
                onChanged: (v) =>
                    ref.read(notificationsEnabledProvider.notifier).set(v),
              ),
              SwitchListTile(
                title: Text(tr.autosave),
                subtitle: Text(tr.autosaveSubtitle),
                value: autosave,
                onChanged: (v) =>
                    ref.read(autosaveEnabledProvider.notifier).set(v),
              ),
              SwitchListTile(
                title: Text(tr.scanCoinbaseTx),
                subtitle: Text(tr.scanCoinbaseSubtitle),
                value: scanCoinbase,
                onChanged: (v) {
                  ref.read(scanCoinbaseProvider.notifier).set(v);
                  ref.read(walletCApiProvider).setScanCoinbase(v);
                },
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Danger Zone ──────────────────────────────────────────────────
        _sectionTitle(tr.dangerZone),
        Card(
          child: ListTile(
            leading: const Icon(Icons.delete_forever, color: kError),
            title: Text(tr.deleteCurrentWallet,
                style: const TextStyle(color: kError)),
            subtitle: Text(tr.deleteCurrentWalletSubtitle),
            onTap: _deleteCurrentWallet,
          ),
        ),

        const SizedBox(height: 32),
      ],
    );
  }

  Widget _sectionTitle(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: Text(text, style: Theme.of(context).textTheme.titleLarge),
      );
}

// ── manage wallets dialog ────────────────────────────────────────────────────

class _ManageWalletsDialog extends StatefulWidget {
  final WalletRegistry registry;
  final String? currentFilename;

  const _ManageWalletsDialog({
    required this.registry,
    required this.currentFilename,
  });

  @override
  State<_ManageWalletsDialog> createState() => _ManageWalletsDialogState();
}

class _ManageWalletsDialogState extends State<_ManageWalletsDialog> {
  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    final List<WalletEntry> wallets = widget.registry.wallets.toList();

    return AlertDialog(
      title: Text(tr.manageWallets),
      content: SizedBox(
        width: double.maxFinite,
        child: ListView.builder(
          shrinkWrap: true,
          itemCount: wallets.length,
          itemBuilder: (_, i) {
            final entry = wallets[i];
            final isCurrent = entry.filename == widget.currentFilename;
            return ListTile(
              title: Text(entry.caption),
              subtitle: isCurrent ? Text(tr.currentlyOpen) : null,
              trailing: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  IconButton(
                    icon: const Icon(Icons.edit, size: 18),
                    onPressed: () => _rename(entry),
                  ),
                  if (!isCurrent)
                    IconButton(
                      icon: const Icon(Icons.delete, size: 18, color: kError),
                      onPressed: () => _delete(entry),
                    ),
                ],
              ),
            );
          },
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text(tr.close),
        ),
      ],
    );
  }

  Future<void> _rename(WalletEntry entry) async {
    final tr = S.of(context)!;
    final ctrl = TextEditingController(text: entry.caption);
    final newName = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr.renameWallet),
        content: TextField(
          controller: ctrl,
          decoration: InputDecoration(labelText: tr.newName),
          autofocus: true,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: Text(tr.cancel),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, ctrl.text.trim()),
            child: Text(tr.rename),
          ),
        ],
      ),
    );
    if (newName == null || newName.isEmpty) return;
    await widget.registry.renameWallet(entry.filename, newName);
    setState(() {});
  }

  Future<void> _delete(WalletEntry entry) async {
    final tr = S.of(context)!;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr.deleteWallet),
        content: Text(tr.deleteWalletConfirmShort(entry.caption)),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(tr.cancel),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: kError),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(tr.delete),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    await widget.registry.deleteWallet(entry.filename);
    setState(() {});
  }
}
