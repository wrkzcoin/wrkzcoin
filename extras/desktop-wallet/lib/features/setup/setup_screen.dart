import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/providers/providers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/copy_button.dart';
import '../../shared/widgets/pluton_logo.dart';

enum _SetupMode { menu, create, open, importSeed, importKeys, backupSeed }

class SetupScreen extends ConsumerStatefulWidget {
  const SetupScreen({super.key});

  @override
  ConsumerState<SetupScreen> createState() => _SetupScreenState();
}

class _SetupScreenState extends ConsumerState<SetupScreen> {
  _SetupMode _mode = _SetupMode.menu;
  bool _loading = false;
  String? _error;

  // Backup confirmation state
  String? _newWalletAddress;
  String? _newWalletSeed;
  String? _newWalletViewKey;
  String? _newWalletSpendKey;
  bool _seedConfirmed = false;

  // Form fields
  final _fileCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  final _daemonHostCtrl = TextEditingController(text: kDefaultDaemonHost);
  final _daemonPortCtrl = TextEditingController(text: '$kDefaultDaemonPort');
  final _viewKeyCtrl = TextEditingController();
  final _spendKeyCtrl = TextEditingController();
  final _seedCtrl = TextEditingController();
  final _scanHeightCtrl = TextEditingController(text: '0');

  @override
  void dispose() {
    for (final c in [
      _fileCtrl, _passCtrl, _daemonHostCtrl, _daemonPortCtrl,
      _viewKeyCtrl, _spendKeyCtrl, _seedCtrl, _scanHeightCtrl,
    ]) { c.dispose(); }
    super.dispose();
  }

  Future<void> _doCreate() async {
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.create(
        _fileCtrl.text.trim(),
        _passCtrl.text,
        _daemonHostCtrl.text.trim(),
        int.tryParse(_daemonPortCtrl.text) ?? kDefaultDaemonPort,
      );
      final address = await ffi.getPrimaryAddress();
      final seed = await ffi.getMnemonicSeed();
      final keys = await ffi.getSpendKeysJson(address);
      final viewKey = await ffi.getPrivateViewKey();

      await storeWalletPassword(_passCtrl.text);
      setState(() {
        _newWalletAddress = address;
        _newWalletSeed = seed;
        _newWalletSpendKey = keys['privateSpendKey'] as String? ?? '';
        _newWalletViewKey = viewKey;
        _seedConfirmed = false;
        _mode = _SetupMode.backupSeed;
      });
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _doOpen() async {
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.open(
        _fileCtrl.text.trim(),
        _passCtrl.text,
        _daemonHostCtrl.text.trim(),
        int.tryParse(_daemonPortCtrl.text) ?? kDefaultDaemonPort,
      );
      await storeWalletPassword(_passCtrl.text);
      ref.read(walletOpenProvider.notifier).state = true;
      if (mounted) context.go('/overview');
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _doImportSeed() async {
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.restoreFromSeed(
        _seedCtrl.text.trim(),
        _fileCtrl.text.trim(),
        _passCtrl.text,
        _daemonHostCtrl.text.trim(),
        int.tryParse(_daemonPortCtrl.text) ?? kDefaultDaemonPort,
        scanHeight: int.tryParse(_scanHeightCtrl.text) ?? 0,
      );
      await storeWalletPassword(_passCtrl.text);
      ref.read(walletOpenProvider.notifier).state = true;
      if (mounted) context.go('/overview');
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _doImportKeys() async {
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.restoreFromKeys(
        _spendKeyCtrl.text.trim(),
        _viewKeyCtrl.text.trim(),
        _fileCtrl.text.trim(),
        _passCtrl.text,
        _daemonHostCtrl.text.trim(),
        int.tryParse(_daemonPortCtrl.text) ?? kDefaultDaemonPort,
        scanHeight: int.tryParse(_scanHeightCtrl.text) ?? 0,
      );
      await storeWalletPassword(_passCtrl.text);
      ref.read(walletOpenProvider.notifier).state = true;
      if (mounted) context.go('/overview');
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _pickFile() async {
    final r = await FilePicker.platform.pickFiles(type: FileType.any, dialogTitle: 'Select wallet file');
    if (r != null) _fileCtrl.text = r.files.single.path ?? '';
  }

  Future<void> _saveFile() async {
    final r = await FilePicker.platform.saveFile(dialogTitle: 'Save wallet as', fileName: 'my_wallet.wallet');
    if (r != null) _fileCtrl.text = r;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: kBgDark,
      body: Center(
        child: SizedBox(
          width: _mode == _SetupMode.backupSeed ? 560 : 460,
          child: Card(
            child: Padding(
              padding: const EdgeInsets.all(32),
              child: SingleChildScrollView(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const PlutonLogo(size: 48),
                    const SizedBox(height: 28),
                    if (_mode == _SetupMode.menu) _buildMenu(),
                    if (_mode == _SetupMode.create) _buildCreate(),
                    if (_mode == _SetupMode.open) _buildOpen(),
                    if (_mode == _SetupMode.importSeed) _buildImportSeed(),
                    if (_mode == _SetupMode.importKeys) _buildImportKeys(),
                    if (_mode == _SetupMode.backupSeed) _buildBackupSeed(),
                    if (_error != null) ...[
                      const SizedBox(height: 16),
                      _ErrorBox(message: _error!),
                    ],
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildMenu() {
    return Column(
      children: [
        Text('Welcome to PLUTON v2', style: Theme.of(context).textTheme.headlineSmall),
        const SizedBox(height: 8),
        Text('Select an option to get started', style: Theme.of(context).textTheme.bodyMedium),
        const SizedBox(height: 28),
        _MenuButton(icon: Icons.add_circle_outline, label: 'Create New Wallet', onTap: () => setState(() => _mode = _SetupMode.create)),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.folder_open_outlined, label: 'Open Existing Wallet', onTap: () => setState(() => _mode = _SetupMode.open)),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.vpn_key_outlined, label: 'Import from Seed Phrase', onTap: () => setState(() => _mode = _SetupMode.importSeed)),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.key_outlined, label: 'Import from Private Keys', onTap: () => setState(() => _mode = _SetupMode.importKeys)),
      ],
    );
  }

  Widget _buildCreate() {
    return _FormWrapper(
      title: 'Create New Wallet',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doCreate,
      children: [
        _FileField(ctrl: _fileCtrl, onPick: _saveFile, label: 'Save wallet to'),
        _PassField(ctrl: _passCtrl),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl),
      ],
    );
  }

  Widget _buildOpen() {
    return _FormWrapper(
      title: 'Open Wallet',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doOpen,
      children: [
        _FileField(ctrl: _fileCtrl, onPick: _pickFile, label: 'Wallet file'),
        _PassField(ctrl: _passCtrl),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl),
      ],
    );
  }

  Widget _buildImportSeed() {
    return _FormWrapper(
      title: 'Import from Seed',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doImportSeed,
      children: [
        _FileField(ctrl: _fileCtrl, onPick: _saveFile, label: 'Save wallet to'),
        _PassField(ctrl: _passCtrl),
        _TField(ctrl: _seedCtrl, label: 'Mnemonic Seed Phrase', maxLines: 3),
        _TField(ctrl: _scanHeightCtrl, label: 'Scan from height (0 = full scan)'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl),
      ],
    );
  }

  Widget _buildImportKeys() {
    return _FormWrapper(
      title: 'Import from Keys',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doImportKeys,
      children: [
        _FileField(ctrl: _fileCtrl, onPick: _saveFile, label: 'Save wallet to'),
        _PassField(ctrl: _passCtrl),
        _TField(ctrl: _viewKeyCtrl, label: 'Private View Key'),
        _TField(ctrl: _spendKeyCtrl, label: 'Private Spend Key'),
        _TField(ctrl: _scanHeightCtrl, label: 'Scan from height (0 = full scan)'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl),
      ],
    );
  }

  Widget _buildBackupSeed() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Warning header
        Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            color: kWarning.withAlpha(25),
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: kWarning.withAlpha(100)),
          ),
          child: Row(
            children: [
              const Icon(Icons.warning_amber_rounded, color: kWarning, size: 22),
              const SizedBox(width: 10),
              Expanded(
                child: Text(
                  'Back up your wallet before continuing.\nThese keys cannot be recovered if lost.',
                  style: const TextStyle(color: kWarning, fontSize: 13, height: 1.4),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 20),
        Text('Your Wallet Address', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletAddress ?? ''),

        const SizedBox(height: 16),
        Text('Seed Phrase (25 words)', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletSeed ?? '', monospace: true, maxLines: 4),

        const SizedBox(height: 16),
        Text('Private View Key', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletViewKey ?? '', monospace: true),

        const SizedBox(height: 16),
        Text('Private Spend Key', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletSpendKey ?? '', monospace: true),

        const SizedBox(height: 24),
        // Confirmation checkbox
        InkWell(
          onTap: () => setState(() => _seedConfirmed = !_seedConfirmed),
          borderRadius: BorderRadius.circular(6),
          child: Row(
            children: [
              Checkbox(
                value: _seedConfirmed,
                onChanged: (v) => setState(() => _seedConfirmed = v ?? false),
                activeColor: kPrimary,
              ),
              const SizedBox(width: 6),
              const Expanded(
                child: Text(
                  'I have written down my seed phrase and private keys in a safe place.',
                  style: TextStyle(fontSize: 13, color: kTextPrimary),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        SizedBox(
          width: double.infinity,
          child: FilledButton(
            onPressed: _seedConfirmed
                ? () {
                    ref.read(walletOpenProvider.notifier).state = true;
                    context.go('/overview');
                  }
                : null,
            child: const Text('I\'ve backed up my wallet — Continue'),
          ),
        ),
      ],
    );
  }
}

// ── Backup display field ──────────────────────────────────────────────────────

class _BackupField extends StatelessWidget {
  final String value;
  final bool monospace;
  final int maxLines;
  const _BackupField({required this.value, this.monospace = false, this.maxLines = 1});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: kSurfaceVariant,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: kDivider),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(
            child: SelectableText(
              value,
              style: TextStyle(
                fontSize: 12,
                fontFamily: monospace ? 'monospace' : null,
                color: kTextPrimary,
                height: 1.5,
              ),
              maxLines: maxLines,
            ),
          ),
          const SizedBox(width: 8),
          CopyButton(text: value, size: 16),
        ],
      ),
    );
  }
}

// ── Local helper widgets ──────────────────────────────────────────────────────

class _ErrorBox extends StatelessWidget {
  final String message;
  const _ErrorBox({required this.message});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: kError.withAlpha(25),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: kError.withAlpha(80)),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: kError, size: 16),
          const SizedBox(width: 8),
          Expanded(child: Text(message, style: const TextStyle(color: kError, fontSize: 13))),
        ],
      ),
    );
  }
}

class _MenuButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final VoidCallback onTap;
  const _MenuButton({required this.icon, required this.label, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: double.infinity,
      child: OutlinedButton.icon(
        icon: Icon(icon, size: 18),
        label: Text(label),
        onPressed: onTap,
        style: OutlinedButton.styleFrom(
          alignment: Alignment.centerLeft,
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
        ),
      ),
    );
  }
}

class _FormWrapper extends StatelessWidget {
  final String title;
  final VoidCallback onBack;
  final bool loading;
  final VoidCallback onSubmit;
  final List<Widget> children;

  const _FormWrapper({
    required this.title,
    required this.onBack,
    required this.loading,
    required this.onSubmit,
    required this.children,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(children: [
          IconButton(icon: const Icon(Icons.arrow_back), onPressed: onBack, iconSize: 18),
          const SizedBox(width: 4),
          Text(title, style: Theme.of(context).textTheme.headlineSmall),
        ]),
        const SizedBox(height: 16),
        ...children.map((w) => Padding(padding: const EdgeInsets.only(bottom: 12), child: w)),
        const SizedBox(height: 8),
        SizedBox(
          width: double.infinity,
          child: FilledButton(
            onPressed: loading ? null : onSubmit,
            child: loading
                ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
                : const Text('Continue'),
          ),
        ),
      ],
    );
  }
}

class _FileField extends StatelessWidget {
  final TextEditingController ctrl;
  final VoidCallback onPick;
  final String label;
  const _FileField({required this.ctrl, required this.onPick, required this.label});

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      Expanded(child: TextField(controller: ctrl, decoration: InputDecoration(labelText: label))),
      const SizedBox(width: 8),
      IconButton(icon: const Icon(Icons.folder_outlined), onPressed: onPick, tooltip: 'Browse'),
    ]);
  }
}

class _PassField extends StatefulWidget {
  final TextEditingController ctrl;
  const _PassField({required this.ctrl});

  @override
  State<_PassField> createState() => _PassFieldState();
}

class _PassFieldState extends State<_PassField> {
  bool _obscure = true;

  @override
  Widget build(BuildContext context) {
    return TextField(
      controller: widget.ctrl,
      obscureText: _obscure,
      decoration: InputDecoration(
        labelText: 'Wallet password',
        suffixIcon: IconButton(
          icon: Icon(_obscure ? Icons.visibility_outlined : Icons.visibility_off_outlined, size: 18),
          onPressed: () => setState(() => _obscure = !_obscure),
        ),
      ),
    );
  }
}

class _TField extends StatelessWidget {
  final TextEditingController ctrl;
  final String label;
  final int maxLines;
  const _TField({required this.ctrl, required this.label, this.maxLines = 1});

  @override
  Widget build(BuildContext context) {
    return TextField(controller: ctrl, maxLines: maxLines, decoration: InputDecoration(labelText: label));
  }
}

class _DaemonFields extends StatelessWidget {
  final TextEditingController hostCtrl;
  final TextEditingController portCtrl;
  const _DaemonFields({required this.hostCtrl, required this.portCtrl});

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      Expanded(flex: 3, child: TextField(controller: hostCtrl, decoration: const InputDecoration(labelText: 'Daemon host'))),
      const SizedBox(width: 8),
      Expanded(child: TextField(controller: portCtrl, decoration: const InputDecoration(labelText: 'Port'), keyboardType: TextInputType.number)),
    ]);
  }
}
