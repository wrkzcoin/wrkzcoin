import 'dart:async';
import 'dart:convert';
import 'dart:js_interop';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:web/web.dart' as web;
import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_web.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/copy_button.dart';
import '../../l10n/generated/app_localizations.dart';

// Last-opened wallet persistence now lives in app_providers.dart
// (readLastWalletName / saveLastWalletName / clearLastWalletName) so the setup
// screen can record it. It was previously declared here and never written,
// which is why "Delete Wallet Data" had no wallet name to delete.

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

  @override
  void initState() {
    super.initState();
    // Pre-fill from current node info
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
    super.dispose();
  }

  Future<void> _saveNode() async {
    final tr = S.of(context);
    setState(() { _savingNode = true; _nodeError = null; _nodeSuccess = null; });
    try {
      await ref.read(walletCApiProvider).swapNode(
        _nodeHostCtrl.text.trim(),
        int.tryParse(_nodePortCtrl.text) ?? kDefaultDaemonPort,
        ssl: _nodeSSL,
      );
      await ref.read(statusProvider.notifier).refresh();
      if (!mounted) return;
      setState(() => _nodeSuccess = tr?.nodeUpdatedSuccess ?? 'Node updated successfully');
    } on WalletCApiException catch (e) {
      setState(() => _nodeError = e.message);
    } catch (e) {
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

  /// Export wallet JSON as a browser download.
  ///
  /// This writes an *unencrypted* dump containing key material, so it asks
  /// first — previously one tap put the keys in the downloads folder with no
  /// warning at all.
  Future<void> _exportJson() async {
    final tr = S.of(context);

    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.exportJsonTitle ?? 'Export to JSON'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Icon(Icons.warning_amber_rounded, color: kWarning, size: 36),
              const SizedBox(height: 12),
              Text(
                dlgTr?.exportJsonWarning ??
                    'This file is NOT encrypted. It contains your private keys in '
                    'plain text — anyone who opens it can spend your funds.\n\n'
                    'Only save it somewhere you control, and delete it when you are done.',
                style: const TextStyle(height: 1.5),
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(dlgTr?.cancel ?? 'Cancel'),
            ),
            FilledButton(
              style: FilledButton.styleFrom(backgroundColor: kWarning),
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(dlgTr?.iUnderstandContinue ?? 'I understand, continue'),
            ),
          ],
        );
      },
    );
    if (confirmed != true || !mounted) return;

    try {
      final jsonStr = await ref.read(walletCApiProvider).exportJson();
      // Trigger browser download via Blob + anchor click
      final bytes = utf8.encode(jsonStr);
      final blob = web.Blob(
        [bytes.toJS].toJS,
        web.BlobPropertyBag(type: 'application/json'),
      );
      final url = web.URL.createObjectURL(blob);
      final anchor = web.document.createElement('a') as web.HTMLAnchorElement;
      anchor.href = url;
      anchor.download = 'wallet_export.json';
      anchor.style.display = 'none';
      web.document.body?.append(anchor);
      anchor.click();
      anchor.remove();
      web.URL.revokeObjectURL(url);

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(tr?.exportedTo('download') ?? 'Exported — check your downloads'), backgroundColor: kSuccess),
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


  /// Download the encrypted wallet file.
  ///
  /// Unlike the JSON export this stays encrypted with the wallet password, so
  /// it is the backup people should actually be taking. The bridge already
  /// supported it; nothing in the UI ever called it.
  Future<void> _downloadWalletFile() async {
    final tr = S.of(context);
    try {
      final name = await readLastWalletName();
      await ref.read(walletCApiProvider).downloadWallet(name);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(tr?.walletFileDownloaded ??
              'Encrypted wallet file downloaded — keep it somewhere safe.'),
          backgroundColor: kSuccess,
        ),
      );
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('$e'), backgroundColor: kError),
      );
    }
  }

  /// Asks for the wallet password and returns true when it checks out.
  /// Gates every action that reveals key material.
  Future<bool> _confirmPassword(String purpose) async {
    final ctrl = TextEditingController();
    var wrong = false;
    try {
      final ok = await showDialog<bool>(
        context: context,
        builder: (ctx) => StatefulBuilder(
          builder: (ctx, setLocal) {
            final dlgTr = S.of(ctx);
            return AlertDialog(
              title: Text(dlgTr?.confirmPassword ?? 'Confirm password'),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(purpose),
                  const SizedBox(height: 12),
                  TextField(
                    controller: ctrl,
                    obscureText: true,
                    autofocus: true,
                    decoration: InputDecoration(
                      labelText: dlgTr?.password ?? 'Password',
                      errorText: wrong
                          ? (dlgTr?.incorrectPassword ?? 'Incorrect password')
                          : null,
                    ),
                    onSubmitted: (_) async {
                      if (await verifyWalletPassword(ctrl.text)) {
                        if (ctx.mounted) Navigator.pop(ctx, true);
                      } else {
                        setLocal(() => wrong = true);
                      }
                    },
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(ctx, false),
                  child: Text(dlgTr?.cancel ?? 'Cancel'),
                ),
                FilledButton(
                  onPressed: () async {
                    if (await verifyWalletPassword(ctrl.text)) {
                      if (ctx.mounted) Navigator.pop(ctx, true);
                    } else {
                      setLocal(() => wrong = true);
                    }
                  },
                  child: Text(dlgTr?.continueButton ?? 'Continue'),
                ),
              ],
            );
          },
        ),
      );
      return ok == true;
    } finally {
      ctrl.dispose();
    }
  }

  /// Reveal the seed phrase and private keys.
  ///
  /// There was previously no way to see these again after wallet creation —
  /// if you skipped writing them down, they were gone.
  Future<void> _showSecrets() async {
    final tr = S.of(context);
    final ok = await _confirmPassword(
      tr?.revealSecretsPurpose ??
          'Your seed phrase and private keys will be shown on screen.',
    );
    if (!ok || !mounted) return;

    final ffi = ref.read(walletCApiProvider);
    String seed = '';
    String viewKey = '';
    String spendKey = '';
    try {
      final address = await ffi.getPrimaryAddress();
      viewKey = await ffi.getPrivateViewKey();
      if (!await ffi.isViewWallet()) {
        seed = await ffi.getMnemonicSeed();
        final keys = await ffi.getSpendKeysJson(address);
        spendKey = keys['privateSpendKey'] as String? ?? '';
      }
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('$e'), backgroundColor: kError),
      );
      return;
    }

    if (!mounted) return;
    await showDialog<void>(
      context: context,
      builder: (ctx) => _SecretsDialog(
        seed: seed,
        viewKey: viewKey,
        spendKey: spendKey,
      ),
    );
  }

  /// Change the wallet password.
  ///
  /// Goes through the bridge's async channel so the re-encrypted file is
  /// written back to browser storage; the local verifier is updated to match,
  /// otherwise the lock screen would keep checking against the old password.
  Future<void> _changePassword() async {
    final tr = S.of(context);
    final currentOk = await _confirmPassword(
      tr?.changePasswordPurpose ?? 'Enter your current wallet password.',
    );
    if (!currentOk || !mounted) return;

    final newCtrl = TextEditingController();
    final confirmCtrl = TextEditingController();
    String? newPassword;
    try {
      newPassword = await showDialog<String>(
        context: context,
        builder: (ctx) => StatefulBuilder(
          builder: (ctx, setLocal) {
            final dlgTr = S.of(ctx);
            String? error;
            void submit() {
              if (newCtrl.text.length < kMinPasswordLength) {
                setLocal(() => error = dlgTr?.passwordTooShort(kMinPasswordLength) ??
                    'Use at least $kMinPasswordLength characters');
                return;
              }
              if (newCtrl.text != confirmCtrl.text) {
                setLocal(() => error = dlgTr?.passwordsDoNotMatch ?? 'Passwords do not match');
                return;
              }
              Navigator.pop(ctx, newCtrl.text);
            }

            return AlertDialog(
              title: Text(dlgTr?.changePassword ?? 'Change password'),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  TextField(
                    controller: newCtrl,
                    obscureText: true,
                    autofocus: true,
                    decoration: InputDecoration(
                      labelText: dlgTr?.newPassword ?? 'New password',
                      errorText: error,
                    ),
                  ),
                  const SizedBox(height: 12),
                  TextField(
                    controller: confirmCtrl,
                    obscureText: true,
                    onSubmitted: (_) => submit(),
                    decoration: InputDecoration(
                      labelText: dlgTr?.confirmPasswordField ?? 'Confirm new password',
                    ),
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.pop(ctx, null),
                  child: Text(dlgTr?.cancel ?? 'Cancel'),
                ),
                FilledButton(onPressed: submit, child: Text(dlgTr?.save ?? 'Save')),
              ],
            );
          },
        ),
      );
    } finally {
      newCtrl.dispose();
      confirmCtrl.dispose();
    }

    if (newPassword == null || !mounted) return;

    try {
      await ref.read(walletCApiProvider).changePassword(newPassword);
      await storeWalletPassword(newPassword);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(tr?.passwordChanged ?? 'Password changed'),
          backgroundColor: kSuccess,
        ),
      );
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('$e'), backgroundColor: kError),
      );
    }
  }

  /// Turning notifications on has to request browser permission from inside a
  /// user gesture. Asking when a transaction arrives — as this used to — is
  /// blocked outright by current browsers.
  Future<void> _setNotifications(bool enabled) async {
    await ref.read(notificationsEnabledProvider.notifier).set(enabled);
    if (!enabled) return;
    try {
      if (web.Notification.permission == 'default') {
        await web.Notification.requestPermission().toDart;
      }
      if (!mounted) return;
      if (web.Notification.permission == 'denied') {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(S.of(context)?.notificationsBlocked ??
                'Your browser is blocking notifications for this site. Enable them in site settings.'),
            backgroundColor: kWarning,
          ),
        );
      }
    } catch (_) {
      // Notification API unavailable — the toggle is simply inert.
    }
  }

  Future<void> _resetScanHeight() async {
    // Disposed in the finally below — dialog-local controllers were leaking.
    final heightCtrl = TextEditingController(text: '0');
    try {
      final confirmed = await showDialog<bool>(
        context: context,
        builder: (ctx) {
          final dlgTr = S.of(ctx);
          return AlertDialog(
            title: Text(dlgTr?.resetScanHeight ?? 'Reset Scan Height'),
            content: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(dlgTr?.resetScanHeightDescription ?? 'Enter a block height to rescan from. Use 0 for a full rescan.'),
                const SizedBox(height: 12),
                TextField(
                  controller: heightCtrl,
                  decoration: InputDecoration(labelText: dlgTr?.scanHeight ?? 'Scan height'),
                  keyboardType: TextInputType.number,
                ),
              ],
            ),
            actions: [
              TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(dlgTr?.cancel ?? 'Cancel')),
              FilledButton(onPressed: () => Navigator.pop(ctx, true), child: Text(dlgTr?.reset ?? 'Reset')),
            ],
          );
        },
      );
      if (confirmed != true) return;
      try {
        await ref.read(walletCApiProvider).reset(
          scanHeight: int.tryParse(heightCtrl.text) ?? 0,
        );
        await ref.read(statusProvider.notifier).refresh();
      } on WalletCApiException catch (e) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(e.message), backgroundColor: kError),
          );
        }
      }
    } finally {
      heightCtrl.dispose();
    }
  }

  /// Delete wallet data from IndexedDB (browser storage).
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
                    'This will permanently delete your wallet data from browser storage.\n\n'
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
    final bool? step2;
    try {
      step2 = await showDialog<bool>(
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
                onPressed: () => Navigator.pop(ctx, typeCtrl.text == (dlgTr?.deleteHint ?? 'DELETE')),
                child: Text(dlgTr?.deletePermanently ?? 'Delete permanently'),
              ),
            ],
          );
        },
      );
    } finally {
      typeCtrl.dispose();
    }
    if (step2 != true) return;

    final ffi = ref.read(walletCApiProvider);
    try {
      // Work out which file to remove *before* closing the wallet. The name is
      // recorded when the wallet is opened; if that record is missing, fall
      // back to enumerating browser storage rather than deleting nothing and
      // still reporting success — which is what used to happen.
      var walletName = await readLastWalletName();
      if (walletName == null || walletName.isEmpty) {
        final stored = await ffi.listWallets();
        if (stored.length == 1) walletName = stored.single;
      }

      if (walletName == null || walletName.isEmpty) {
        throw StateError(
            'Could not determine which wallet to delete. Open the wallet again, '
            'or remove it from the Open Wallet screen.');
      }

      await ffi.close();
      await ffi.deleteFile(walletName);

      // Verify it is actually gone before telling the user it was deleted.
      final remaining = await ffi.listWallets();
      if (remaining.contains(walletName)) {
        throw StateError('Browser storage still reports "$walletName" after deletion.');
      }

      await clearLastWalletName();
      await clearWalletPassword();
      if (!mounted) return;
      ref.read(walletOpenProvider.notifier).state = false;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(S.of(context)?.walletDataDeleted ?? 'Wallet data deleted'),
          backgroundColor: kSuccess,
        ),
      );
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(S.of(context)?.deleteFailed(e.toString()) ??
                'Delete failed: $e'),
            backgroundColor: kError,
            duration: const Duration(seconds: 10),
          ),
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
    final autoLockMinutes = ref.watch(autoLockMinutesProvider);

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
                        style: TextStyle(color: context.textSecondary, fontSize: 13),
                      ),
                      const SizedBox(height: 16),
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
                              Text(tr?.ssl ?? 'SSL', style: TextStyle(color: context.textSecondary, fontSize: 12)),
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
                      subtitle: tr?.saveWalletSubtitle ?? 'Flush current state to browser storage',
                      onTap: _saveWallet,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.download_outlined,
                      title: tr?.downloadWalletFile ?? 'Download Wallet File',
                      subtitle: tr?.downloadWalletFileSubtitle ??
                          'Save an encrypted backup of this wallet to your device',
                      onTap: _downloadWalletFile,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.key_outlined,
                      title: tr?.showSeedAndKeys ?? 'Seed Phrase & Private Keys',
                      subtitle: tr?.showSeedAndKeysSubtitle ??
                          'Reveal your recovery seed and keys (asks for your password)',
                      onTap: _showSecrets,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.lock_reset_outlined,
                      title: tr?.changePassword ?? 'Change Password',
                      subtitle: tr?.changePasswordSubtitle ??
                          'Re-encrypt this wallet with a new password',
                      onTap: _changePassword,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.file_download_outlined,
                      title: tr?.exportToJson ?? 'Export to JSON',
                      subtitle: tr?.exportToJsonSubtitle ?? 'Download wallet data as a JSON file',
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
                                Text(tr?.autosaveSubtitle ?? 'Save wallet to browser storage after sync and every 5 minutes', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
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

              // -- Security section ---------------------------------------------
              _SectionHeader(title: tr?.sectionSecurity ?? 'Security', icon: Icons.shield_outlined),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                  child: Row(
                    children: [
                      Icon(Icons.timer_outlined, size: 20, color: context.textSecondary),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(tr?.autoLock ?? 'Auto-lock',
                                style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                            Text(
                                tr?.autoLockSubtitle ??
                                    'Lock the wallet after a period of inactivity',
                                style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                          ],
                        ),
                      ),
                      DropdownButton<int>(
                        value: kAutoLockChoices.contains(autoLockMinutes)
                            ? autoLockMinutes
                            : kDefaultAutoLockMinutes,
                        underline: const SizedBox.shrink(),
                        items: kAutoLockChoices
                            .map((m) => DropdownMenuItem(
                                  value: m,
                                  child: Text(
                                    m == 0
                                        ? (tr?.autoLockNever ?? 'Never')
                                        : (tr?.autoLockMinutes(m) ?? '$m min'),
                                    style: const TextStyle(fontSize: 13),
                                  ),
                                ))
                            .toList(),
                        onChanged: (m) {
                          if (m != null) {
                            unawaited(ref.read(autoLockMinutesProvider.notifier).set(m));
                          }
                        },
                      ),
                    ],
                  ),
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
                      Icon(Icons.brightness_6_outlined, size: 20, color: context.textSecondary),
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
                      Icon(Icons.notifications_active_outlined, size: 20, color: context.textSecondary),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(tr?.incomingTxAlerts ?? 'Incoming Transaction Alerts', style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 14)),
                            Text(tr?.incomingTxAlertsSubtitle ?? 'Show a browser notification when WRKZ is received', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
                          ],
                        ),
                      ),
                      Switch(
                        value: notificationsEnabled,
                        onChanged: (v) => unawaited(_setNotifications(v)),
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
                          Icon(Icons.tune_outlined, size: 20, color: context.textSecondary),
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
                                ref.read(walletCApiProvider).setLogLevel(l.name);
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
                  subtitle: tr?.deleteWalletDataSubtitle ?? 'Permanently remove wallet from browser storage',
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
  const _NodeWarningBanner({required this.message});

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

// -- Seed / private key reveal ------------------------------------------------

/// Shows key material behind an explicit tap-to-reveal, with a reminder that
/// the screen is now the weakest link.
class _SecretsDialog extends StatefulWidget {
  final String seed;
  final String viewKey;
  final String spendKey;

  const _SecretsDialog({
    required this.seed,
    required this.viewKey,
    required this.spendKey,
  });

  @override
  State<_SecretsDialog> createState() => _SecretsDialogState();
}

class _SecretsDialogState extends State<_SecretsDialog> {
  bool _revealed = false;

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    return AlertDialog(
      title: Text(tr?.showSeedAndKeys ?? 'Seed Phrase & Private Keys'),
      content: SizedBox(
        width: 460,
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: kError.withAlpha(20),
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: kError.withAlpha(80)),
                ),
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Icon(Icons.visibility_off_outlined, color: kError, size: 18),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Text(
                        tr?.secretsWarning ??
                            'Anyone with these can spend your funds. Make sure nobody can '
                            'see your screen, and never share them — no support channel '
                            'will ever ask for them.',
                        style: const TextStyle(color: kError, fontSize: 12, height: 1.5),
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 16),
              if (!_revealed)
                SizedBox(
                  width: double.infinity,
                  child: OutlinedButton.icon(
                    icon: const Icon(Icons.visibility_outlined, size: 16),
                    label: Text(tr?.tapToReveal ?? 'Tap to reveal'),
                    onPressed: () => setState(() => _revealed = true),
                  ),
                )
              else ...[
                if (widget.seed.isNotEmpty) ...[
                  Text(tr?.seedPhrase25Words ?? 'Seed Phrase (25 words)',
                      style: Theme.of(context).textTheme.titleSmall),
                  const SizedBox(height: 6),
                  _SecretField(value: widget.seed, maxLines: 4),
                  const SizedBox(height: 16),
                ],
                Text(tr?.privateViewKey ?? 'Private View Key',
                    style: Theme.of(context).textTheme.titleSmall),
                const SizedBox(height: 6),
                _SecretField(value: widget.viewKey),
                if (widget.spendKey.isNotEmpty) ...[
                  const SizedBox(height: 16),
                  Text(tr?.privateSpendKey ?? 'Private Spend Key',
                      style: Theme.of(context).textTheme.titleSmall),
                  const SizedBox(height: 6),
                  _SecretField(value: widget.spendKey),
                ],
                if (widget.seed.isEmpty) ...[
                  const SizedBox(height: 12),
                  Text(tr?.viewOnlyNoSeed ??
                      'This is a view-only wallet — it has no seed phrase or spend key.',
                      style: TextStyle(color: context.textSecondary, fontSize: 12)),
                ],
              ],
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text(tr?.close ?? 'Close'),
        ),
      ],
    );
  }
}

class _SecretField extends StatelessWidget {
  final String value;
  final int maxLines;
  const _SecretField({required this.value, this.maxLines = 1});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: context.dividerColor),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(
            child: SelectableText(
              value,
              maxLines: maxLines,
              style: TextStyle(
                fontSize: 12,
                height: 1.5,
                fontFamily: 'monospace',
                color: Theme.of(context).colorScheme.onSurface,
              ),
            ),
          ),
          const SizedBox(width: 8),
          CopyButton(text: value, size: 16),
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

  /// Log-level tint. Resolved per build so it tracks the active theme rather
  /// than being frozen to the dark palette.
  static Color _levelColor(BuildContext context, String level) => switch (level) {
        'fatal' || 'error' => kError,
        'warning' => kWarning,
        'debug' => context.textSecondary,
        'trace' => context.textDisabled,
        _ => context.textPrimary,
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

  Future<void> _poll() async {
    try {
      final data = await widget.ffi.takeLogsAsync();
      final list = (data['entries'] as List<dynamic>? ?? [])
          .map((e) => _LogEntry.fromJson(e as Map<String, dynamic>))
          .toList();
      if (list.isEmpty) return;
      if (mounted) setState(() => _entries.addAll(list));
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
                  Icon(Icons.article_outlined, size: 18, color: context.textSecondary),
                  const SizedBox(width: 8),
                  Text(tr?.walletLogs ?? 'Wallet Logs', style: Theme.of(context).textTheme.titleMedium),
                  const Spacer(),
                  Text(tr?.logEntries(count) ?? '$count entries',
                      style: TextStyle(fontSize: 12, color: context.textDisabled)),
                  const SizedBox(width: 12),
                  // Auto-scroll toggle
                  Row(
                    children: [
                      Text(tr?.autoScroll ?? 'Auto-scroll', style: TextStyle(fontSize: 12, color: context.textSecondary)),
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
                          style: TextStyle(color: context.textDisabled, fontSize: 13)),
                    )
                  : ListView.builder(
                      controller: _scroll,
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                      itemCount: _entries.length,
                      itemBuilder: (_, i) {
                        final e = _entries[i];
                        final color = _levelColor(context, e.level);
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
