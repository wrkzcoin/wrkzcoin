import 'dart:io';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';

const _storage = FlutterSecureStorage();
const _kLastWalletKey = 'pluton_last_wallet_path';

// ── Last-opened wallet persistence ───────────────────────────────────────────

final lastWalletPathProvider = FutureProvider<String?>((ref) async {
  return _storage.read(key: _kLastWalletKey);
});

Future<void> saveLastWalletPath(String path) async {
  await _storage.write(key: _kLastWalletKey, value: path);
}

Future<void> clearLastWalletPath() async {
  await _storage.delete(key: _kLastWalletKey);
}

// ── Screen ────────────────────────────────────────────────────────────────────

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
    super.dispose();
  }

  Future<void> _saveNode() async {
    setState(() { _savingNode = true; _nodeError = null; _nodeSuccess = null; });
    try {
      await ref.read(walletCApiProvider).swapNode(
        _nodeHostCtrl.text.trim(),
        int.tryParse(_nodePortCtrl.text) ?? kDefaultDaemonPort,
        ssl: _nodeSSL,
      );
      ref.read(statusProvider.notifier).refresh();
      setState(() => _nodeSuccess = 'Node updated successfully');
    } on WalletCApiException catch (e) {
      setState(() => _nodeError = e.message);
    } catch (e) {
      setState(() => _nodeError = e.toString());
    } finally {
      if (mounted) setState(() => _savingNode = false);
    }
  }

  Future<void> _saveWallet() async {
    try {
      await ref.read(walletCApiProvider).save();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Wallet saved'), backgroundColor: kSuccess),
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

  Future<void> _exportJson() async {
    final path = await FilePicker.platform.saveFile(
      dialogTitle: 'Export wallet JSON',
      fileName: 'wallet_export.json',
    );
    if (path == null) return;
    try {
      final jsonStr = await ref.read(walletCApiProvider).exportJson();
      await File(path).writeAsString(jsonStr);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Exported to $path'), backgroundColor: kSuccess),
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
          SnackBar(content: Text('Export failed: $e'), backgroundColor: kError),
        );
      }
    }
  }

  Future<void> _resetScanHeight() async {
    final heightCtrl = TextEditingController(text: '0');
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Reset Scan Height'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Enter a block height to rescan from. Use 0 for a full rescan.'),
            const SizedBox(height: 12),
            TextField(
              controller: heightCtrl,
              decoration: const InputDecoration(labelText: 'Scan height'),
              keyboardType: TextInputType.number,
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Reset')),
        ],
      ),
    );
    if (confirmed != true) return;
    try {
      await ref.read(walletCApiProvider).reset(
        scanHeight: int.tryParse(heightCtrl.text) ?? 0,
      );
      ref.read(statusProvider.notifier).refresh();
    } on WalletCApiException catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(e.message), backgroundColor: kError),
        );
      }
    }
  }

  Future<void> _deleteWalletData() async {
    // Step 1: first confirmation
    final step1 = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete Wallet Data'),
        content: const Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(Icons.warning_amber_rounded, color: kError, size: 40),
            SizedBox(height: 12),
            Text(
              'This will permanently delete your wallet file from disk.\n\n'
              'Make sure you have backed up your seed phrase and private keys before proceeding. '
              'This action cannot be undone.',
              style: TextStyle(height: 1.5),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            style: TextButton.styleFrom(foregroundColor: kError),
            child: const Text('I understand, continue'),
          ),
        ],
      ),
    );
    if (step1 != true) return;
    if (!mounted) return;

    // Step 2: type "DELETE" to confirm
    final typeCtrl = TextEditingController();
    final step2 = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Final Confirmation'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Type DELETE to confirm:'),
            const SizedBox(height: 10),
            TextField(
              controller: typeCtrl,
              autofocus: true,
              decoration: const InputDecoration(hintText: 'DELETE'),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: kError),
            onPressed: () => Navigator.pop(ctx, typeCtrl.text == 'DELETE'),
            child: const Text('Delete permanently'),
          ),
        ],
      ),
    );
    if (step2 != true) return;

    try {
      final ffi = ref.read(walletCApiProvider);
      // Save before closing so the file is in a clean state before we delete it
      try { await ffi.save(); } catch (_) {}
      ffi.close();

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
    final nodeAsync = ref.watch(statusProvider);
    final themeMode = ref.watch(themeModeProvider);
    final logLevel = ref.watch(logLevelProvider);

    return SingleChildScrollView(
      padding: const EdgeInsets.all(28),
      child: Center(
        child: SizedBox(
          width: 620,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Settings', style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 24),

              // ── Node section ──────────────────────────────────────────────
              _SectionHeader(title: 'Daemon Node', icon: Icons.cloud_outlined),
              const SizedBox(height: 12),

              // Node error banner (shown when node is unreachable)
              nodeAsync.whenOrNull(
                error: (e, _) => _NodeWarningBanner(
                  onSwitch: () => _scrollToNodeForm(),
                ),
              ) ?? const SizedBox.shrink(),

              Card(
                child: Padding(
                  padding: const EdgeInsets.all(20),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Text(
                        'Connect to a local or remote daemon node. '
                        'Changes take effect immediately.',
                        style: TextStyle(color: kTextSecondary, fontSize: 13),
                      ),
                      const SizedBox(height: 16),
                      Row(
                        children: [
                          Expanded(
                            flex: 3,
                            child: TextField(
                              controller: _nodeHostCtrl,
                              decoration: const InputDecoration(labelText: 'Host / IP address'),
                            ),
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: TextField(
                              controller: _nodePortCtrl,
                              decoration: const InputDecoration(labelText: 'Port'),
                              keyboardType: TextInputType.number,
                            ),
                          ),
                          const SizedBox(width: 10),
                          Column(
                            children: [
                              const Text('SSL', style: TextStyle(color: kTextSecondary, fontSize: 12)),
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
                            : const Text('Apply'),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // ── Wallet section ────────────────────────────────────────────
              _SectionHeader(title: 'Wallet', icon: Icons.account_balance_wallet_outlined),
              const SizedBox(height: 12),
              Card(
                child: Column(
                  children: [
                    _SettingsTile(
                      icon: Icons.save_outlined,
                      title: 'Save Wallet',
                      subtitle: 'Flush current state to disk',
                      onTap: _saveWallet,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.file_download_outlined,
                      title: 'Export to JSON',
                      subtitle: 'Save wallet data as a JSON file',
                      onTap: _exportJson,
                    ),
                    const Divider(height: 1, indent: 56),
                    _SettingsTile(
                      icon: Icons.sync_outlined,
                      title: 'Reset Scan Height',
                      subtitle: 'Rescan blockchain from a specific height',
                      onTap: _resetScanHeight,
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 24),

              // ── Appearance section ────────────────────────────────────────
              _SectionHeader(title: 'Appearance', icon: Icons.palette_outlined),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                  child: Row(
                    children: [
                      const Icon(Icons.brightness_6_outlined, size: 20, color: kTextSecondary),
                      const SizedBox(width: 12),
                      const Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text('Theme', style: TextStyle(color: kTextPrimary, fontSize: 14)),
                            Text('Choose app colour scheme', style: TextStyle(color: kTextSecondary, fontSize: 12)),
                          ],
                        ),
                      ),
                      SegmentedButton<ThemeMode>(
                        segments: const [
                          ButtonSegment(value: ThemeMode.system, icon: Icon(Icons.brightness_auto, size: 16), label: Text('System')),
                          ButtonSegment(value: ThemeMode.light,  icon: Icon(Icons.light_mode, size: 16),      label: Text('Light')),
                          ButtonSegment(value: ThemeMode.dark,   icon: Icon(Icons.dark_mode, size: 16),       label: Text('Dark')),
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

              // ── Debug / Logs section ──────────────────────────────────────
              _SectionHeader(title: 'Debug & Logs', icon: Icons.bug_report_outlined),
              const SizedBox(height: 12),
              Card(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
                  child: Row(
                    children: [
                      const Icon(Icons.tune_outlined, size: 20, color: kTextSecondary),
                      const SizedBox(width: 12),
                      const Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text('Log Level', style: TextStyle(color: kTextPrimary, fontSize: 14)),
                            Text('Controls wallet library verbosity', style: TextStyle(color: kTextSecondary, fontSize: 12)),
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
                            ref.read(walletCApiProvider).setLogLevel(l.value.toString());
                          }
                        },
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 24),

              // ── Danger zone ───────────────────────────────────────────────
              _SectionHeader(title: 'Danger Zone', icon: Icons.warning_amber_outlined, color: kError),
              const SizedBox(height: 12),
              Card(
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(12),
                  side: const BorderSide(color: kError, width: 1),
                ),
                child: _SettingsTile(
                  icon: Icons.delete_forever_outlined,
                  title: 'Delete Wallet Data',
                  subtitle: 'Permanently remove wallet file from disk',
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

  void _scrollToNodeForm() {
    _nodeHostCtrl.selection = TextSelection(
        baseOffset: 0, extentOffset: _nodeHostCtrl.text.length);
  }
}

// ── Local helper widgets ──────────────────────────────────────────────────────

class _SectionHeader extends StatelessWidget {
  final String title;
  final IconData icon;
  final Color color;

  const _SectionHeader({
    required this.title,
    required this.icon,
    this.color = kTextPrimary,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Icon(icon, size: 16, color: color),
        const SizedBox(width: 8),
        Text(title, style: TextStyle(color: color, fontSize: 14, fontWeight: FontWeight.w600)),
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
    return ListTile(
      leading: Icon(icon, size: 20, color: iconColor ?? kTextSecondary),
      title: Text(title, style: TextStyle(color: titleColor ?? kTextPrimary, fontSize: 14)),
      subtitle: Text(subtitle, style: const TextStyle(color: kTextSecondary, fontSize: 12)),
      trailing: const Icon(Icons.chevron_right, size: 16, color: kTextSecondary),
      onTap: onTap,
    );
  }
}

class _NodeWarningBanner extends StatelessWidget {
  final VoidCallback onSwitch;
  const _NodeWarningBanner({required this.onSwitch});

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
          const Expanded(
            child: Text(
              'Cannot reach the current node. Enter a new node address below and tap Apply.',
              style: TextStyle(color: kError, fontSize: 13),
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
