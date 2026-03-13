import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_web.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../l10n/generated/app_localizations.dart';
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

  // Form fields — on web, wallet "filename" is just a logical name stored in IndexedDB
  final _fileCtrl = TextEditingController(text: 'my_wallet');
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
        ssl: kDefaultDaemonSSL,
      );
      ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
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
      setState(() => _error = e.message.isNotEmpty ? e.message : e.toString());
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
        ssl: kDefaultDaemonSSL,
      );
      ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
      await storeWalletPassword(_passCtrl.text);
      ref.read(walletOpenProvider.notifier).state = true;
      if (mounted) context.go('/overview');
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message.isNotEmpty ? e.message : e.toString());
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
        ssl: kDefaultDaemonSSL,
      );
      ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
      await storeWalletPassword(_passCtrl.text);
      ref.read(walletOpenProvider.notifier).state = true;
      if (mounted) context.go('/overview');
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message.isNotEmpty ? e.message : e.toString());
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
        ssl: kDefaultDaemonSSL,
      );
      ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
      await storeWalletPassword(_passCtrl.text);
      ref.read(walletOpenProvider.notifier).state = true;
      if (mounted) context.go('/overview');
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message.isNotEmpty ? e.message : e.toString());
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    return Scaffold(
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
                    if (_mode == _SetupMode.menu) _buildMenu(tr),
                    if (_mode == _SetupMode.create) _buildCreate(tr),
                    if (_mode == _SetupMode.open) _buildOpen(tr),
                    if (_mode == _SetupMode.importSeed) _buildImportSeed(tr),
                    if (_mode == _SetupMode.importKeys) _buildImportKeys(tr),
                    if (_mode == _SetupMode.backupSeed) _buildBackupSeed(tr),
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

  Widget _buildMenu(S? tr) {
    return Column(
      children: [
        Text(tr?.welcomeToPluton ?? 'Welcome to PLUTON Web', style: Theme.of(context).textTheme.headlineSmall),
        const SizedBox(height: 8),
        Text(tr?.selectOptionToStart ?? 'Select an option to get started', style: Theme.of(context).textTheme.bodyMedium),
        const SizedBox(height: 28),
        _MenuButton(icon: Icons.add_circle_outline, label: tr?.createNewWallet ?? 'Create New Wallet', onTap: () => setState(() => _mode = _SetupMode.create)),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.folder_open_outlined, label: tr?.openExistingWallet ?? 'Open Existing Wallet', onTap: () => setState(() => _mode = _SetupMode.open)),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.vpn_key_outlined, label: tr?.importFromSeed ?? 'Import from Seed Phrase', onTap: () => setState(() => _mode = _SetupMode.importSeed)),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.key_outlined, label: tr?.importFromKeys ?? 'Import from Private Keys', onTap: () => setState(() => _mode = _SetupMode.importKeys)),
      ],
    );
  }

  Widget _buildCreate(S? tr) {
    return _FormWrapper(
      title: tr?.createNewWallet ?? 'Create New Wallet',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doCreate,
      continueLabel: tr?.continueButton ?? 'Continue',
      children: [
        _TField(ctrl: _fileCtrl, label: tr?.walletFile ?? 'Wallet name'),
        _PassField(ctrl: _passCtrl, label: tr?.walletPassword ?? 'Wallet password'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl, hostLabel: tr?.daemonHost ?? 'Daemon host', portLabel: tr?.port ?? 'Port'),
      ],
    );
  }

  Widget _buildOpen(S? tr) {
    return _FormWrapper(
      title: tr?.openWallet ?? 'Open Wallet',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doOpen,
      continueLabel: tr?.continueButton ?? 'Continue',
      children: [
        _TField(ctrl: _fileCtrl, label: tr?.walletFile ?? 'Wallet name'),
        _PassField(ctrl: _passCtrl, label: tr?.walletPassword ?? 'Wallet password'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl, hostLabel: tr?.daemonHost ?? 'Daemon host', portLabel: tr?.port ?? 'Port'),
      ],
    );
  }

  Widget _buildImportSeed(S? tr) {
    return _FormWrapper(
      title: tr?.importFromSeedTitle ?? 'Import from Seed',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doImportSeed,
      continueLabel: tr?.continueButton ?? 'Continue',
      children: [
        _TField(ctrl: _fileCtrl, label: tr?.walletFile ?? 'Wallet name'),
        _PassField(ctrl: _passCtrl, label: tr?.walletPassword ?? 'Wallet password'),
        _TField(ctrl: _seedCtrl, label: tr?.mnemonicSeedPhrase ?? 'Mnemonic Seed Phrase', maxLines: 3),
        _TField(ctrl: _scanHeightCtrl, label: tr?.scanFromHeight ?? 'Scan from height (0 = full scan)'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl, hostLabel: tr?.daemonHost ?? 'Daemon host', portLabel: tr?.port ?? 'Port'),
      ],
    );
  }

  Widget _buildImportKeys(S? tr) {
    return _FormWrapper(
      title: tr?.importFromKeysTitle ?? 'Import from Keys',
      onBack: () => setState(() { _mode = _SetupMode.menu; _error = null; }),
      loading: _loading,
      onSubmit: _doImportKeys,
      continueLabel: tr?.continueButton ?? 'Continue',
      children: [
        _TField(ctrl: _fileCtrl, label: tr?.walletFile ?? 'Wallet name'),
        _PassField(ctrl: _passCtrl, label: tr?.walletPassword ?? 'Wallet password'),
        _TField(ctrl: _viewKeyCtrl, label: tr?.privateViewKey ?? 'Private View Key'),
        _TField(ctrl: _spendKeyCtrl, label: tr?.privateSpendKey ?? 'Private Spend Key'),
        _TField(ctrl: _scanHeightCtrl, label: tr?.scanFromHeight ?? 'Scan from height (0 = full scan)'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl, hostLabel: tr?.daemonHost ?? 'Daemon host', portLabel: tr?.port ?? 'Port'),
      ],
    );
  }

  Widget _buildBackupSeed(S? tr) {
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
                  tr?.backupWarning ?? 'Back up your wallet before continuing.\nThese keys cannot be recovered if lost.',
                  style: const TextStyle(color: kWarning, fontSize: 13, height: 1.4),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 20),
        Text(tr?.yourWalletAddress ?? 'Your Wallet Address', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletAddress ?? ''),

        const SizedBox(height: 16),
        Text(tr?.seedPhrase25Words ?? 'Seed Phrase (25 words)', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletSeed ?? '', monospace: true, maxLines: 4),

        const SizedBox(height: 16),
        Text(tr?.privateViewKey ?? 'Private View Key', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        _BackupField(value: _newWalletViewKey ?? '', monospace: true),

        const SizedBox(height: 16),
        Text(tr?.privateSpendKey ?? 'Private Spend Key', style: Theme.of(context).textTheme.titleSmall),
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
              Expanded(
                child: Text(
                  tr?.seedBackupConfirm ?? 'I have written down my seed phrase and private keys in a safe place.',
                  style: TextStyle(fontSize: 13, color: Theme.of(context).colorScheme.onSurface),
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
            child: Text(tr?.backedUpContinue ?? 'I\'ve backed up my wallet \u2014 Continue'),
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
        color: Theme.of(context).colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
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
                color: Theme.of(context).colorScheme.onSurface,
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
          Expanded(child: Text(
            message.isNotEmpty ? message : 'An unknown error occurred.',
            style: const TextStyle(color: kError, fontSize: 13),
          )),
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
  final String continueLabel;

  const _FormWrapper({
    required this.title,
    required this.onBack,
    required this.loading,
    required this.onSubmit,
    required this.children,
    this.continueLabel = 'Continue',
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
                : Text(continueLabel),
          ),
        ),
      ],
    );
  }
}

class _PassField extends StatefulWidget {
  final TextEditingController ctrl;
  final String label;
  const _PassField({required this.ctrl, this.label = 'Wallet password'});

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
        labelText: widget.label,
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
  final String hostLabel;
  final String portLabel;
  const _DaemonFields({required this.hostCtrl, required this.portCtrl, this.hostLabel = 'Daemon host', this.portLabel = 'Port'});

  @override
  Widget build(BuildContext context) {
    return Row(children: [
      Expanded(flex: 3, child: TextField(controller: hostCtrl, decoration: InputDecoration(labelText: hostLabel))),
      const SizedBox(width: 8),
      Expanded(child: TextField(controller: portCtrl, decoration: InputDecoration(labelText: portLabel), keyboardType: TextInputType.number)),
    ]);
  }
}
