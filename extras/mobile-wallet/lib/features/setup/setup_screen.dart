import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/providers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/copy_button.dart';

enum _SetupMode { menu, create, importSeed, importKeys, importView, backupSeed }

class SetupScreen extends ConsumerStatefulWidget {
  const SetupScreen({super.key});

  @override
  ConsumerState<SetupScreen> createState() => _SetupScreenState();
}

class _SetupScreenState extends ConsumerState<SetupScreen> {
  _SetupMode _mode = _SetupMode.menu;

  // form fields
  final _captionCtrl = TextEditingController(text: 'My Wallet');
  final _passwordCtrl = TextEditingController();
  final _confirmPwCtrl = TextEditingController();
  final _seedCtrl = TextEditingController();
  final _spendKeyCtrl = TextEditingController();
  final _viewKeyCtrl = TextEditingController();
  final _addressCtrl = TextEditingController();
  final _scanHeightCtrl = TextEditingController(text: '0');

  // node
  int _nodePresetIndex = 0;
  bool _customNode = false;
  final _customHostCtrl = TextEditingController();
  final _customPortCtrl = TextEditingController(text: '17856');
  bool _customSsl = false;

  // state
  bool _loading = false;
  String? _error;
  bool _obscurePassword = true;
  bool _obscureConfirm = true;

  // backup display
  String _backupSeed = '';
  String _backupViewKey = '';
  String _backupSpendKey = '';
  bool _backupConfirmed = false;

  @override
  void dispose() {
    _captionCtrl.dispose();
    _passwordCtrl.dispose();
    _confirmPwCtrl.dispose();
    _seedCtrl.dispose();
    _spendKeyCtrl.dispose();
    _viewKeyCtrl.dispose();
    _addressCtrl.dispose();
    _scanHeightCtrl.dispose();
    _customHostCtrl.dispose();
    _customPortCtrl.dispose();
    super.dispose();
  }

  String get _daemonHost {
    if (_customNode) return _customHostCtrl.text.trim();
    return AppConfig.nodePresets[_nodePresetIndex].host;
  }

  int get _daemonPort {
    if (_customNode) return int.tryParse(_customPortCtrl.text) ?? 17856;
    return AppConfig.nodePresets[_nodePresetIndex].port;
  }

  bool get _daemonSsl {
    if (_customNode) return _customSsl;
    return AppConfig.nodePresets[_nodePresetIndex].ssl;
  }

  String? _validate() {
    final tr = S.of(context)!;
    final caption = _captionCtrl.text.trim();
    if (caption.isEmpty) return tr.walletNameRequired;
    if (_mode != _SetupMode.menu && _mode != _SetupMode.backupSeed) {
      if (_passwordCtrl.text.isEmpty) return tr.passwordRequired;
      if (_passwordCtrl.text.length < 6) {
        return tr.passwordTooShort;
      }
      if (_passwordCtrl.text != _confirmPwCtrl.text) {
        return tr.passwordsDoNotMatch;
      }
    }
    if (_mode == _SetupMode.importSeed && _seedCtrl.text.trim().isEmpty) {
      return tr.seedRequired;
    }
    if (_mode == _SetupMode.importKeys) {
      if (_spendKeyCtrl.text.trim().isEmpty) return tr.spendKeyRequired;
      if (_viewKeyCtrl.text.trim().isEmpty) return tr.viewKeyRequired;
    }
    if (_mode == _SetupMode.importView) {
      if (_viewKeyCtrl.text.trim().isEmpty) return tr.viewKeyRequired;
      if (_addressCtrl.text.trim().isEmpty) return tr.addressRequired;
    }
    if (_customNode && _customHostCtrl.text.trim().isEmpty) {
      return tr.daemonHostRequired;
    }
    return null;
  }

  Future<void> _submit() async {
    final err = _validate();
    if (err != null) {
      setState(() => _error = err);
      hapticError();
      return;
    }

    setState(() {
      _loading = true;
      _error = null;
    });

    try {
      final registry = ref.read(walletRegistryProvider);
      final ffi = ref.read(walletCApiProvider);
      final entry = await registry.addWallet(_captionCtrl.text.trim());
      final walletPath = registry.getWalletPath(entry.filename);
      final password = _passwordCtrl.text;
      final scanHeight = int.tryParse(_scanHeightCtrl.text) ?? 0;

      switch (_mode) {
        case _SetupMode.create:
          await ffi.create(walletPath, password, _daemonHost, _daemonPort,
              ssl: _daemonSsl);
          // Fetch backup info.
          _backupSeed = await ffi.getMnemonicSeed();
          _backupViewKey = await ffi.getPrivateViewKey();
          final addr = await ffi.getPrimaryAddress();
          final keys = await ffi.getSpendKeysJson(addr);
          _backupSpendKey = keys['privateSpendKey'] as String? ?? '';
          break;

        case _SetupMode.importSeed:
          await ffi.restoreFromSeed(
            _seedCtrl.text.trim(),
            walletPath,
            password,
            _daemonHost,
            _daemonPort,
            scanHeight: scanHeight,
            ssl: _daemonSsl,
          );
          break;

        case _SetupMode.importKeys:
          await ffi.restoreFromKeys(
            _spendKeyCtrl.text.trim(),
            _viewKeyCtrl.text.trim(),
            walletPath,
            password,
            _daemonHost,
            _daemonPort,
            scanHeight: scanHeight,
            ssl: _daemonSsl,
          );
          break;

        case _SetupMode.importView:
          await ffi.restoreViewWallet(
            _viewKeyCtrl.text.trim(),
            _addressCtrl.text.trim(),
            walletPath,
            password,
            _daemonHost,
            _daemonPort,
            scanHeight: scanHeight,
            ssl: _daemonSsl,
          );
          break;

        default:
          break;
      }

      await storeWalletPassword(entry.filename, password);
      await registry.setLastOpened(entry.filename);
      ref.read(activeWalletFilenameProvider.notifier).state = entry.filename;

      hapticHeavy();

      if (_mode == _SetupMode.create) {
        setState(() {
          _mode = _SetupMode.backupSeed;
          _loading = false;
        });
      } else {
        await markSeedBackupConfirmed(entry.filename);
        _openWallet();
      }
    } catch (e) {
      setState(() {
        _error = e.toString();
        _loading = false;
      });
      hapticError();
    }
  }

  void _openWallet() {
    ref.read(walletOpenProvider.notifier).state = true;
    ref.read(walletLockedProvider.notifier).state = false;
    context.go('/overview');
  }

  // ── build ──────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    return Scaffold(
      appBar: _mode != _SetupMode.menu
          ? AppBar(
              leading: _mode == _SetupMode.backupSeed
                  ? null
                  : IconButton(
                      icon: const Icon(Icons.arrow_back),
                      onPressed: () => setState(() {
                        _mode = _SetupMode.menu;
                        _error = null;
                      }),
                    ),
              title: Text(_modeTitle(tr)),
              automaticallyImplyLeading: false,
            )
          : null,
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: _mode == _SetupMode.menu
              ? _buildMenu(tr)
              : _mode == _SetupMode.backupSeed
                  ? _buildBackup(tr)
                  : _buildForm(tr),
        ),
      ),
    );
  }

  String _modeTitle(S tr) {
    switch (_mode) {
      case _SetupMode.create:
        return tr.createWallet;
      case _SetupMode.importSeed:
        return tr.importFromSeed;
      case _SetupMode.importKeys:
        return tr.importFromKeys;
      case _SetupMode.importView:
        return tr.viewOnlyWallet;
      case _SetupMode.backupSeed:
        return tr.backupSeedTitle;
      default:
        return '';
    }
  }

  // ── menu ───────────────────────────────────────────────────────────────────

  Widget _buildMenu(S tr) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(height: 20),
        Center(
          child: Container(
            width: 64,
            height: 64,
            decoration: BoxDecoration(
              gradient: const LinearGradient(
                colors: [kPrimary, kAccent],
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
              ),
              borderRadius: BorderRadius.circular(16),
            ),
            child: const Icon(Icons.add, size: 32, color: Colors.white),
          ),
        ),
        const SizedBox(height: 20),
        Center(
          child: Text(
            tr.addWallet,
            style: Theme.of(context).textTheme.headlineMedium,
          ),
        ),
        const SizedBox(height: 32),
        _menuCard(
          icon: Icons.create_new_folder_outlined,
          title: tr.createNewWallet,
          subtitle: tr.createNewWalletSubtitle,
          onTap: () => setState(() => _mode = _SetupMode.create),
        ),
        const SizedBox(height: 12),
        _menuCard(
          icon: Icons.restore,
          title: tr.importFromSeed,
          subtitle: tr.importFromSeedSubtitle,
          onTap: () => setState(() => _mode = _SetupMode.importSeed),
        ),
        const SizedBox(height: 12),
        _menuCard(
          icon: Icons.key,
          title: tr.importFromKeys,
          subtitle: tr.importFromKeysSubtitle,
          onTap: () => setState(() => _mode = _SetupMode.importKeys),
        ),
        const SizedBox(height: 12),
        _menuCard(
          icon: Icons.visibility_outlined,
          title: tr.viewOnlyWallet,
          subtitle: tr.viewOnlyWalletSubtitle,
          onTap: () => setState(() => _mode = _SetupMode.importView),
        ),
      ],
    );
  }

  Widget _menuCard({
    required IconData icon,
    required String title,
    required String subtitle,
    required VoidCallback onTap,
  }) {
    return Card(
      child: ListTile(
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        leading: CircleAvatar(
          backgroundColor: kPrimary.withAlpha(30),
          child: Icon(icon, color: kPrimary),
        ),
        title: Text(title, style: Theme.of(context).textTheme.titleMedium),
        subtitle: Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
        trailing: const Icon(Icons.chevron_right),
        onTap: onTap,
      ),
    );
  }

  // ── form ───────────────────────────────────────────────────────────────────

  Widget _buildForm(S tr) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Caption
        _label(tr.walletName),
        TextField(
          controller: _captionCtrl,
          decoration: InputDecoration(hintText: tr.walletNameHint),
          textInputAction: TextInputAction.next,
        ),
        const SizedBox(height: 16),

        // Password
        _label(tr.passwordLabel),
        TextField(
          controller: _passwordCtrl,
          obscureText: _obscurePassword,
          decoration: InputDecoration(
            hintText: tr.enterPassword,
            suffixIcon: IconButton(
              icon: Icon(_obscurePassword
                  ? Icons.visibility_off
                  : Icons.visibility),
              onPressed: () =>
                  setState(() => _obscurePassword = !_obscurePassword),
            ),
          ),
          textInputAction: TextInputAction.next,
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _confirmPwCtrl,
          obscureText: _obscureConfirm,
          decoration: InputDecoration(
            hintText: tr.confirmPassword,
            suffixIcon: IconButton(
              icon: Icon(
                  _obscureConfirm ? Icons.visibility_off : Icons.visibility),
              onPressed: () =>
                  setState(() => _obscureConfirm = !_obscureConfirm),
            ),
          ),
          textInputAction: TextInputAction.next,
        ),
        const SizedBox(height: 16),

        // Mode-specific fields
        if (_mode == _SetupMode.importSeed) ...[
          _label(tr.seedPhrase),
          TextField(
            controller: _seedCtrl,
            maxLines: 3,
            decoration: InputDecoration(hintText: tr.enterSeedPhrase),
          ),
          const SizedBox(height: 16),
          _label(tr.scanHeight),
          TextField(
            controller: _scanHeightCtrl,
            keyboardType: TextInputType.number,
            decoration: InputDecoration(hintText: tr.scanHeightHint),
          ),
          const SizedBox(height: 16),
        ],

        if (_mode == _SetupMode.importKeys) ...[
          _label(tr.privateSpendKey),
          TextField(
            controller: _spendKeyCtrl,
            decoration: InputDecoration(hintText: tr.hexKey),
          ),
          const SizedBox(height: 12),
          _label(tr.privateViewKey),
          TextField(
            controller: _viewKeyCtrl,
            decoration: InputDecoration(hintText: tr.hexKey),
          ),
          const SizedBox(height: 12),
          _label(tr.scanHeight),
          TextField(
            controller: _scanHeightCtrl,
            keyboardType: TextInputType.number,
            decoration: InputDecoration(hintText: tr.scanHeightHint),
          ),
          const SizedBox(height: 16),
        ],

        if (_mode == _SetupMode.importView) ...[
          _label(tr.privateViewKey),
          TextField(
            controller: _viewKeyCtrl,
            decoration: InputDecoration(hintText: tr.hexKey),
          ),
          const SizedBox(height: 12),
          _label(tr.walletAddress),
          TextField(
            controller: _addressCtrl,
            decoration: InputDecoration(hintText: tr.walletAddressHint),
          ),
          const SizedBox(height: 12),
          _label(tr.scanHeight),
          TextField(
            controller: _scanHeightCtrl,
            keyboardType: TextInputType.number,
            decoration: InputDecoration(hintText: tr.scanHeightHint),
          ),
          const SizedBox(height: 16),
        ],

        // Node selection
        _label(tr.daemonNode),
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
            textInputAction: TextInputAction.next,
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
          const SizedBox(height: 16),
        ],

        // Error
        if (_error != null) ...[
          const SizedBox(height: 12),
          Container(
            width: double.infinity,
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: kError.withAlpha(25),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Text(_error!,
                style: TextStyle(color: kError, fontSize: 13)),
          ),
        ],

        const SizedBox(height: 24),

        // Submit
        FilledButton(
          onPressed: _loading ? null : _submit,
          child: _loading
              ? const SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(
                      strokeWidth: 2, color: Colors.white),
                )
              : Text(_mode == _SetupMode.create
                  ? tr.createWallet
                  : tr.importWallet),
        ),
      ],
    );
  }

  Widget _label(String text) => Padding(
        padding: const EdgeInsets.only(bottom: 6),
        child: Text(text, style: Theme.of(context).textTheme.labelLarge),
      );

  // ── backup seed ────────────────────────────────────────────────────────────

  Widget _buildBackup(S tr) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: double.infinity,
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            color: kWarning.withAlpha(25),
            borderRadius: BorderRadius.circular(12),
          ),
          child: Row(
            children: [
              const Icon(Icons.warning_amber, color: kWarning),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  tr.backupWarning,
                  style: Theme.of(context)
                      .textTheme
                      .bodyMedium
                      ?.copyWith(color: kWarning),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 24),
        _backupField(tr.seedPhraseLabel, _backupSeed),
        const SizedBox(height: 16),
        _backupField(tr.privateViewKeyLabel, _backupViewKey),
        const SizedBox(height: 16),
        _backupField(tr.privateSpendKeyLabel, _backupSpendKey),
        const SizedBox(height: 24),
        CheckboxListTile(
          value: _backupConfirmed,
          onChanged: (v) => setState(() => _backupConfirmed = v ?? false),
          controlAffinity: ListTileControlAffinity.leading,
          contentPadding: EdgeInsets.zero,
          title: Text(
            tr.backupConfirmCheck,
            style: Theme.of(context).textTheme.bodyMedium,
          ),
        ),
        const SizedBox(height: 16),
        FilledButton(
          onPressed: _backupConfirmed
              ? () async {
                  final filename =
                      ref.read(activeWalletFilenameProvider);
                  if (filename != null) {
                    await markSeedBackupConfirmed(filename);
                  }
                  _openWallet();
                }
              : null,
          child: Text(tr.continueToWallet),
        ),
      ],
    );
  }

  Widget _backupField(String label, String value) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Text(label, style: Theme.of(context).textTheme.labelLarge),
            const Spacer(),
            CopyButton(text: value),
          ],
        ),
        const SizedBox(height: 4),
        Container(
          width: double.infinity,
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: Theme.of(context).colorScheme.surface,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(
              color: Theme.of(context).dividerColor,
            ),
          ),
          child: SelectableText(
            value,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  fontFamily: 'monospace',
                  height: 1.5,
                ),
          ),
        ),
      ],
    );
  }
}
