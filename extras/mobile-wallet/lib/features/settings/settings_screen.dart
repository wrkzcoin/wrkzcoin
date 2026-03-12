import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/copy_button.dart';

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
    _resetHeightCtrl.dispose();
    super.dispose();
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
    String host;
    int port;
    bool ssl;

    if (_customNode) {
      host = _customHostCtrl.text.trim();
      port = int.tryParse(_customPortCtrl.text) ?? 17856;
      ssl = _customSsl;
      if (host.isEmpty) {
        setState(() {
          _nodeMsg = 'Host is required';
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
        _nodeMsg = 'Node updated to $host:$port';
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
    try {
      await ref.read(walletCApiProvider).save();
      hapticLight();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Wallet saved')),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Save failed: $e')),
        );
      }
    }
  }

  Future<void> _resetScan() async {
    final height = int.tryParse(_resetHeightCtrl.text) ?? 0;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Reset Scan Height'),
        content: Text(
            'This will rescan the blockchain from block $height. This may take a while. Continue?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Reset'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;

    try {
      await ref.read(walletCApiProvider).reset(scanHeight: height);
      hapticMedium();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Scan reset to block $height')),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Reset failed: $e')),
        );
      }
    }
  }

  Future<void> _backupSeed() async {
    // Re-authenticate first
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    final passCtrl = TextEditingController();
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Enter Password'),
        content: TextField(
          controller: passCtrl,
          obscureText: true,
          decoration: const InputDecoration(hintText: 'Password'),
          autofocus: true,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Confirm'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    final verified = await verifyWalletPassword(filename, passCtrl.text);
    if (!verified) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Incorrect password')),
        );
      }
      return;
    }

    try {
      final ffi = ref.read(walletCApiProvider);
      final seed = await ffi.getMnemonicSeed();
      final viewKey = await ffi.getPrivateViewKey();

      if (!mounted) return;
      await showDialog(
        context: context,
        builder: (ctx) => AlertDialog(
          title: const Text('Seed Backup'),
          content: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Seed Phrase:',
                    style: TextStyle(fontWeight: FontWeight.bold)),
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
                const Text('Private View Key:',
                    style: TextStyle(fontWeight: FontWeight.bold)),
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
              child: const Text('I\'ve backed up'),
            ),
          ],
        ),
      );
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error: $e')),
        );
      }
    }
  }

  Future<void> _changePassword() async {
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    final currentCtrl = TextEditingController();
    final newCtrl = TextEditingController();
    final confirmCtrl = TextEditingController();

    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Change Password'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: currentCtrl,
              obscureText: true,
              decoration: const InputDecoration(labelText: 'Current password'),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: newCtrl,
              obscureText: true,
              decoration: const InputDecoration(labelText: 'New password'),
            ),
            const SizedBox(height: 8),
            TextField(
              controller: confirmCtrl,
              obscureText: true,
              decoration:
                  const InputDecoration(labelText: 'Confirm new password'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Change'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    final verified = await verifyWalletPassword(filename, currentCtrl.text);
    if (!verified) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Current password is incorrect')),
        );
      }
      return;
    }
    if (newCtrl.text != confirmCtrl.text) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('New passwords do not match')),
        );
      }
      return;
    }
    if (newCtrl.text.length < 6) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
              content: Text('Password must be at least 6 characters')),
        );
      }
      return;
    }

    try {
      await ref.read(walletCApiProvider).changePassword(newCtrl.text);
      await storeWalletPassword(filename, newCtrl.text);
      hapticMedium();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Password changed')),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error: $e')),
        );
      }
    }
  }

  Future<void> _switchWallet() async {
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.save();
      ffi.close();
    } catch (_) {}

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
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    final deleteCtrl = TextEditingController();
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete Wallet'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text(
              'This will permanently delete the wallet file and keys. '
              'Make sure you have backed up your seed phrase.\n\n'
              'Type DELETE to confirm:',
            ),
            const SizedBox(height: 12),
            TextField(
              controller: deleteCtrl,
              decoration: const InputDecoration(hintText: 'DELETE'),
              autofocus: true,
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: kError),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirmed != true || deleteCtrl.text != 'DELETE') return;

    try {
      final ffi = ref.read(walletCApiProvider);
      ffi.close();
      final registry = ref.read(walletRegistryProvider);
      await registry.deleteWallet(filename);
      await clearWalletPassword(filename);
      await clearSeedBackupFlag(filename);
      hapticHeavy();

      ref.read(walletOpenProvider.notifier).state = false;
      ref.read(walletLockedProvider.notifier).state = false;
      ref.read(activeWalletFilenameProvider.notifier).state = null;
      if (mounted) context.go('/picker');
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error: $e')),
        );
      }
    }
  }

  // ── build ──────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    final themeMode = ref.watch(themeModeProvider);
    final notifications = ref.watch(notificationsEnabledProvider);
    final autosave = ref.watch(autosaveEnabledProvider);
    final biometric = ref.watch(biometricEnabledProvider);
    final autoLockIdx = ref.watch(autoLockIndexProvider);
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
        _sectionTitle('Daemon Node'),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                ...List.generate(AppConfig.nodePresets.length, (i) {
                  final preset = AppConfig.nodePresets[i];
                  return RadioListTile<int>(
                    value: i,
                    groupValue: _customNode ? -1 : _nodePresetIndex,
                    onChanged: (v) => setState(() {
                      _nodePresetIndex = v!;
                      _customNode = false;
                    }),
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
                  groupValue: _customNode ? -1 : _nodePresetIndex,
                  onChanged: (_) => setState(() => _customNode = true),
                  title: Text('Custom',
                      style: Theme.of(context).textTheme.bodyMedium),
                  dense: true,
                  contentPadding: EdgeInsets.zero,
                ),
                if (_customNode) ...[
                  const SizedBox(height: 8),
                  TextField(
                    controller: _customHostCtrl,
                    decoration: const InputDecoration(
                        hintText: 'Host / IP', labelText: 'Host'),
                  ),
                  const SizedBox(height: 8),
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _customPortCtrl,
                          keyboardType: TextInputType.number,
                          decoration: const InputDecoration(
                              hintText: '17856', labelText: 'Port'),
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
                          const Text('SSL'),
                        ],
                      ),
                    ],
                  ),
                ],
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
                      : const Text('Apply'),
                ),
              ],
            ),
          ),
        ),

        const SizedBox(height: 24),

        // ── Current Wallet ───────────────────────────────────────────────
        _sectionTitle('Current Wallet — $walletCaption'),
        Card(
          child: Column(
            children: [
              ListTile(
                leading: const Icon(Icons.save),
                title: const Text('Save Wallet'),
                onTap: _saveWallet,
              ),
              ListTile(
                leading: const Icon(Icons.vpn_key_outlined),
                title: const Text('Backup Seed'),
                onTap: _backupSeed,
              ),
              ListTile(
                leading: const Icon(Icons.lock_reset),
                title: const Text('Change Password'),
                onTap: _changePassword,
              ),
              ListTile(
                leading: const Icon(Icons.restart_alt),
                title: const Text('Reset Scan Height'),
                subtitle: SizedBox(
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
                trailing: TextButton(
                  onPressed: _resetScan,
                  child: const Text('Reset'),
                ),
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Wallet Management ────────────────────────────────────────────
        _sectionTitle('Wallet Management'),
        Card(
          child: Column(
            children: [
              ListTile(
                leading: const Icon(Icons.swap_horiz),
                title: const Text('Switch Wallet'),
                subtitle: const Text('Save & close, pick another'),
                onTap: _switchWallet,
              ),
              ListTile(
                leading: const Icon(Icons.folder_open),
                title: const Text('Manage Wallets'),
                subtitle: const Text('Rename or delete wallets'),
                onTap: _manageWallets,
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Security ─────────────────────────────────────────────────────
        _sectionTitle('Security'),
        Card(
          child: Column(
            children: [
              SwitchListTile(
                title: const Text('Biometric Unlock'),
                subtitle: const Text('Fingerprint / Face ID'),
                value: biometric,
                onChanged: (v) async {
                  if (v) {
                    final available = await isBiometricAvailable();
                    if (!available) {
                      if (mounted) {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(
                              content: Text('Biometric not available')),
                        );
                      }
                      return;
                    }
                  }
                  ref.read(biometricEnabledProvider.notifier).set(v);
                },
              ),
              ListTile(
                title: const Text('Auto-Lock'),
                subtitle:
                    Text(AppConfig.autoLockOptions[autoLockIdx].label),
                trailing: DropdownButton<int>(
                  value: autoLockIdx,
                  underline: const SizedBox.shrink(),
                  items: List.generate(
                    AppConfig.autoLockOptions.length,
                    (i) => DropdownMenuItem(
                      value: i,
                      child: Text(AppConfig.autoLockOptions[i].label),
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

        // ── Appearance ───────────────────────────────────────────────────
        _sectionTitle('Appearance'),
        Card(
          child: ListTile(
            title: const Text('Theme'),
            trailing: SegmentedButton<ThemeMode>(
              segments: const [
                ButtonSegment(
                    value: ThemeMode.system, label: Text('Auto')),
                ButtonSegment(
                    value: ThemeMode.light, label: Text('Light')),
                ButtonSegment(
                    value: ThemeMode.dark, label: Text('Dark')),
              ],
              selected: {themeMode},
              onSelectionChanged: (s) =>
                  ref.read(themeModeProvider.notifier).set(s.first),
            ),
          ),
        ),

        const SizedBox(height: 24),

        // ── Notifications & Autosave ─────────────────────────────────────
        _sectionTitle('Preferences'),
        Card(
          child: Column(
            children: [
              SwitchListTile(
                title: const Text('Transaction Notifications'),
                subtitle: const Text('Alert on incoming transactions'),
                value: notifications,
                onChanged: (v) =>
                    ref.read(notificationsEnabledProvider.notifier).set(v),
              ),
              SwitchListTile(
                title: const Text('Autosave'),
                subtitle:
                    const Text('Save after sync, then every 5 minutes'),
                value: autosave,
                onChanged: (v) =>
                    ref.read(autosaveEnabledProvider.notifier).set(v),
              ),
            ],
          ),
        ),

        const SizedBox(height: 24),

        // ── Danger Zone ──────────────────────────────────────────────────
        _sectionTitle('Danger Zone'),
        Card(
          child: ListTile(
            leading: const Icon(Icons.delete_forever, color: kError),
            title: const Text('Delete Current Wallet',
                style: TextStyle(color: kError)),
            subtitle: const Text('Permanently remove wallet data'),
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
  final dynamic registry;
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
    final wallets = widget.registry.wallets.toList();

    return AlertDialog(
      title: const Text('Manage Wallets'),
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
              subtitle: isCurrent ? const Text('(currently open)') : null,
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
          child: const Text('Close'),
        ),
      ],
    );
  }

  Future<void> _rename(dynamic entry) async {
    final ctrl = TextEditingController(text: entry.caption);
    final newName = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Rename Wallet'),
        content: TextField(
          controller: ctrl,
          decoration: const InputDecoration(labelText: 'New name'),
          autofocus: true,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, ctrl.text.trim()),
            child: const Text('Rename'),
          ),
        ],
      ),
    );
    if (newName == null || newName.isEmpty) return;
    await widget.registry.renameWallet(entry.filename, newName);
    setState(() {});
  }

  Future<void> _delete(dynamic entry) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete Wallet'),
        content: Text('Delete "${entry.caption}"? This cannot be undone.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: kError),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    await widget.registry.deleteWallet(entry.filename);
    setState(() {});
  }
}
