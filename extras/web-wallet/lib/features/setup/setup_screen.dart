import 'dart:math';

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
  bool _seedRevealed = false;

  /// Indices of the seed words the user must retype to prove they wrote the
  /// phrase down. A self-attested checkbox proves nothing, and the funds are
  /// unrecoverable if the seed is lost.
  List<int> _challengeIndices = const [];
  final _challengeCtrls = <int, TextEditingController>{};

  // SSL for daemon — updated from defaultNodeProvider when entering a form
  bool _daemonSSL = kDefaultDaemonSSL;

  // Existing wallets loaded from IndexedDB for the Open Wallet selector
  List<String>? _savedWallets; // null = not loaded yet
  String? _selectedWallet;    // currently selected wallet in the dropdown

  // Form fields — on web, wallet "filename" is just a logical name stored in IndexedDB
  final _fileCtrl = TextEditingController(text: 'my_wallet');
  final _passCtrl = TextEditingController();
  final _passConfirmCtrl = TextEditingController();
  final _daemonHostCtrl = TextEditingController(text: kDefaultDaemonHost);
  final _daemonPortCtrl = TextEditingController(text: '$kDefaultDaemonPort');
  final _viewKeyCtrl = TextEditingController();
  final _spendKeyCtrl = TextEditingController();
  final _seedCtrl = TextEditingController();
  final _scanHeightCtrl = TextEditingController(text: '0');

  @override
  void dispose() {
    for (final c in [
      _fileCtrl, _passCtrl, _passConfirmCtrl, _daemonHostCtrl, _daemonPortCtrl,
      _viewKeyCtrl, _spendKeyCtrl, _seedCtrl, _scanHeightCtrl,
    ]) { c.dispose(); }
    for (final c in _challengeCtrls.values) {
      c.dispose();
    }
    super.dispose();
  }

  /// Shared pre-flight for every "open a wallet" path.
  /// Returns an error message, or null when the form is usable.
  String? _validateCommon(S? tr, {required bool requireConfirm}) {
    if (_fileCtrl.text.trim().isEmpty) {
      return tr?.walletNameRequired ?? 'Enter a name for this wallet';
    }
    // An empty or trivial password leaves the file in browser storage
    // effectively unencrypted; the form used to accept anything.
    if (_passCtrl.text.length < kMinPasswordLength) {
      return tr?.passwordTooShort(kMinPasswordLength) ??
          'Use a password of at least $kMinPasswordLength characters';
    }
    if (requireConfirm && _passCtrl.text != _passConfirmCtrl.text) {
      return tr?.passwordsDoNotMatch ?? 'Passwords do not match.';
    }
    if (_daemonHostCtrl.text.trim().isEmpty) {
      return tr?.daemonHostRequired ?? 'Enter a daemon host';
    }
    final port = int.tryParse(_daemonPortCtrl.text.trim());
    if (port == null || port < 1 || port > 65535) {
      return tr?.invalidPort ?? 'Port must be between 1 and 65535';
    }
    return null;
  }

  Future<void> _doCreate() async {
    final tr = S.of(context);
    final problem = _validateCommon(tr, requireConfirm: true);
    if (problem != null) {
      setState(() => _error = problem);
      return;
    }
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.create(
        _fileCtrl.text.trim(),
        _passCtrl.text,
        _daemonHostCtrl.text.trim(),
        int.tryParse(_daemonPortCtrl.text) ?? kDefaultDaemonPort,
        ssl: _daemonSSL,
      );
      final address = await ffi.getPrimaryAddress();
      final seed = await ffi.getMnemonicSeed();
      final keys = await ffi.getSpendKeysJson(address);
      final viewKey = await ffi.getPrivateViewKey();
      await ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));

      await storeWalletPassword(_passCtrl.text);
      // Record which wallet is current, so Settings > Delete Wallet Data knows
      // what to remove. This was never written before, and the delete silently
      // removed nothing.
      await saveLastWalletName(_fileCtrl.text.trim());
      setState(() {
        _newWalletAddress = address;
        _newWalletSeed = seed;
        _newWalletSpendKey = keys['privateSpendKey'] as String? ?? '';
        _newWalletViewKey = viewKey;
        _seedConfirmed = false;
        _seedRevealed = false;
        _mode = _SetupMode.backupSeed;
        _makeChallenge();
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
    final tr = S.of(context);
    if (_fileCtrl.text.trim().isEmpty) {
      setState(() => _error = tr?.walletNameRequired ?? 'Select a wallet to open');
      return;
    }
    if (_passCtrl.text.isEmpty) {
      setState(() => _error = tr?.enterPasswordToContinue ?? 'Enter your wallet password');
      return;
    }
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      await ffi.open(
        _fileCtrl.text.trim(),
        _passCtrl.text,
        _daemonHostCtrl.text.trim(),
        int.tryParse(_daemonPortCtrl.text) ?? kDefaultDaemonPort,
        ssl: _daemonSSL,
      );
      await ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
      await storeWalletPassword(_passCtrl.text);
      await saveLastWalletName(_fileCtrl.text.trim());
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
    final tr = S.of(context);
    final problem = _validateCommon(tr, requireConfirm: false);
    if (problem != null) {
      setState(() => _error = problem);
      return;
    }
    // A 25-word mnemonic is the only thing restoreFromSeed accepts; catching
    // the count here beats an opaque failure from inside the WASM module.
    final words = _seedCtrl.text.trim().split(RegExp(r'\s+'))
        ..removeWhere((w) => w.isEmpty);
    if (words.length != kMnemonicWordCount) {
      setState(() => _error = tr?.seedWordCount(kMnemonicWordCount, words.length) ??
          'A seed phrase has $kMnemonicWordCount words — you entered ${words.length}');
      return;
    }
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
        ssl: _daemonSSL,
      );
      await ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
      await storeWalletPassword(_passCtrl.text);
      await saveLastWalletName(_fileCtrl.text.trim());
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
    final tr = S.of(context);
    final problem = _validateCommon(tr, requireConfirm: false);
    if (problem != null) {
      setState(() => _error = problem);
      return;
    }
    final hex64 = RegExp(r'^[0-9a-fA-F]{64}$');
    if (!hex64.hasMatch(_spendKeyCtrl.text.trim())) {
      setState(() => _error = tr?.invalidSpendKey ??
          'Private spend key must be 64 hexadecimal characters');
      return;
    }
    if (!hex64.hasMatch(_viewKeyCtrl.text.trim())) {
      setState(() => _error = tr?.invalidViewKey ??
          'Private view key must be 64 hexadecimal characters');
      return;
    }
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
        ssl: _daemonSSL,
      );
      await ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));
      await storeWalletPassword(_passCtrl.text);
      await saveLastWalletName(_fileCtrl.text.trim());
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

  Future<void> _deleteWallet(String name) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete wallet?'),
        content: Text('This will permanently delete "$name" from this browser. Make sure you have your seed phrase backed up.'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: kError),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    try {
      await ref.read(walletCApiProvider).deleteFile(name);
      final remaining = await ref.read(walletCApiProvider).listWallets();
      if (remaining.contains(name)) {
        throw StateError('Browser storage still reports "$name" after deletion.');
      }
    } catch (e) {
      // Swallowing this is how the broken delete stayed invisible: the dialog
      // closed, the list reloaded unchanged, and nothing said why.
      if (mounted) {
        setState(() => _error = (S.of(context)?.deleteFailed('$e') ?? 'Delete failed: $e'));
      }
    }
    // Reload the list
    setState(() { _savedWallets = null; _selectedWallet = null; });
    await _loadSavedWallets();
  }

  Future<void> _loadSavedWallets() async {
    try {
      final wallets = await ref.read(walletCApiProvider).listWallets()
          .timeout(const Duration(seconds: 10));
      if (!mounted) return;
      setState(() {
        _savedWallets = wallets;
        if (wallets.isNotEmpty) {
          _selectedWallet = wallets.first;
          _fileCtrl.text = wallets.first;
        }
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _savedWallets = [];
        // Listing browser storage should not fail silently — if it does, the
        // Open Wallet screen just looks empty for no stated reason.
        _error = S.of(context)?.errorPrefix('$e') ?? 'Error: $e';
      });
    }
  }

  /// Sync daemon form controllers from the saved default node preference.
  void _applyNodeDefaults() {
    final node = ref.read(defaultNodeProvider);
    _daemonHostCtrl.text = node.host;
    _daemonPortCtrl.text = '${node.port}';
    setState(() => _daemonSSL = node.ssl);
  }

  void _openPreWalletSettings() {
    showDialog<void>(
      context: context,
      builder: (_) => const _PreWalletSettingsDialog(),
    );
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    return Scaffold(
      body: Stack(
        children: [
          Center(
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
          Positioned(
            top: 8,
            right: 8,
            child: IconButton(
              icon: const Icon(Icons.settings_outlined),
              tooltip: tr?.settings ?? 'Settings',
              onPressed: _openPreWalletSettings,
            ),
          ),
        ],
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
        _MenuButton(icon: Icons.add_circle_outline, label: tr?.createNewWallet ?? 'Create New Wallet', onTap: () { _applyNodeDefaults(); setState(() => _mode = _SetupMode.create); }),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.folder_open_outlined, label: tr?.openExistingWallet ?? 'Open Existing Wallet', onTap: () { _applyNodeDefaults(); _loadSavedWallets(); setState(() => _mode = _SetupMode.open); }),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.vpn_key_outlined, label: tr?.importFromSeed ?? 'Import from Seed Phrase', onTap: () { _applyNodeDefaults(); setState(() => _mode = _SetupMode.importSeed); }),
        const SizedBox(height: 10),
        _MenuButton(icon: Icons.key_outlined, label: tr?.importFromKeys ?? 'Import from Private Keys', onTap: () { _applyNodeDefaults(); setState(() => _mode = _SetupMode.importKeys); }),
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
        _PassField(ctrl: _passConfirmCtrl, label: 'Confirm password'),
        _DaemonFields(hostCtrl: _daemonHostCtrl, portCtrl: _daemonPortCtrl, hostLabel: tr?.daemonHost ?? 'Daemon host', portLabel: tr?.port ?? 'Port'),
      ],
    );
  }

  Widget _buildOpen(S? tr) {
    final wallets = _savedWallets;
    return _FormWrapper(
      title: tr?.openWallet ?? 'Open Wallet',
      onBack: () => setState(() {
        _mode = _SetupMode.menu;
        _error = null;
        _savedWallets = null;
        _selectedWallet = null;
      }),
      loading: _loading,
      onSubmit: _doOpen,
      continueLabel: tr?.continueButton ?? 'Continue',
      children: [
        if (wallets == null)
          // Still loading from IndexedDB
          const Center(child: SizedBox(height: 48, child: CircularProgressIndicator(strokeWidth: 2)))
        else if (wallets.isEmpty)
          // No saved wallets — free-type name
          _TField(ctrl: _fileCtrl, label: tr?.walletFile ?? 'Wallet name')
        else
          // Dropdown of saved wallets with delete action
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              InputDecorator(
                decoration: InputDecoration(labelText: tr?.walletFile ?? 'Select wallet'),
                child: DropdownButton<String>(
                  value: _selectedWallet,
                  isExpanded: true,
                  underline: const SizedBox.shrink(),
                  hint: Text(tr?.walletFile ?? 'Select a saved wallet'),
                  items: wallets
                      .map((w) => DropdownMenuItem(value: w, child: Text(w)))
                      .toList(),
                  onChanged: (v) => setState(() {
                    _selectedWallet = v;
                    _fileCtrl.text = v ?? '';
                  }),
                ),
              ),
              if (_selectedWallet != null)
                Align(
                  alignment: Alignment.centerRight,
                  child: TextButton.icon(
                    icon: const Icon(Icons.delete_outline, size: 16),
                    label: const Text('Delete wallet'),
                    style: TextButton.styleFrom(foregroundColor: kError, padding: EdgeInsets.zero),
                    onPressed: () => _deleteWallet(_selectedWallet!),
                  ),
                ),
            ],
          ),
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

        // Key material is hidden until the user asks for it, so it is not
        // simply sitting on screen in an office, a cafe, or a screen share.
        if (!_seedRevealed)
          SizedBox(
            width: double.infinity,
            child: OutlinedButton.icon(
              icon: const Icon(Icons.visibility_outlined, size: 16),
              label: Text(tr?.tapToReveal ?? 'Tap to reveal seed phrase and keys'),
              onPressed: () => setState(() => _seedRevealed = true),
            ),
          )
        else ...[
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
          const Divider(),
          const SizedBox(height: 16),

          // Prove the phrase was written down. The old flow was a single
          // self-attested checkbox, which people tick without reading — and
          // there is no recovery once the seed is gone.
          Text(tr?.verifySeedTitle ?? 'Confirm your backup',
              style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 4),
          Text(
            tr?.verifySeedSubtitle ??
                'Type the requested words from the phrase you just wrote down.',
            style: TextStyle(fontSize: 12, color: context.textSecondary),
          ),
          const SizedBox(height: 12),
          ..._challengeIndices.map((i) => Padding(
                padding: const EdgeInsets.only(bottom: 10),
                child: TextField(
                  controller: _challengeCtrls[i],
                  autocorrect: false,
                  enableSuggestions: false,
                  onChanged: (_) => setState(_recheckChallenge),
                  decoration: InputDecoration(
                    isDense: true,
                    labelText: tr?.wordNumber(i + 1) ?? 'Word #${i + 1}',
                    suffixIcon: _isChallengeWordCorrect(i)
                        ? const Icon(Icons.check_circle, color: kSuccess, size: 18)
                        : null,
                  ),
                ),
              )),
          const SizedBox(height: 4),
          TextButton(
            onPressed: () => setState(_makeChallenge),
            child: Text(tr?.askDifferentWords ?? 'Ask me different words'),
          ),
        ],

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

  // ── Seed verification ─────────────────────────────────────────────────────

  List<String> get _seedWords => (_newWalletSeed ?? '')
      .trim()
      .split(RegExp(r'\s+'))
      .where((w) => w.isNotEmpty)
      .toList();

  /// Picks three distinct words to ask about, spread across the phrase.
  void _makeChallenge() {
    for (final c in _challengeCtrls.values) {
      c.dispose();
    }
    _challengeCtrls.clear();

    final words = _seedWords;
    if (words.length < 6) {
      _challengeIndices = const [];
      _seedConfirmed = words.isNotEmpty;
      return;
    }

    // Deterministic-but-varied: one word from each third of the phrase.
    final third = words.length ~/ 3;
    final rnd = Random.secure();
    _challengeIndices = [
      rnd.nextInt(third),
      third + rnd.nextInt(third),
      2 * third + rnd.nextInt(words.length - 2 * third),
    ]..sort();

    for (final i in _challengeIndices) {
      _challengeCtrls[i] = TextEditingController();
    }
    _seedConfirmed = false;
  }

  bool _isChallengeWordCorrect(int index) {
    final words = _seedWords;
    if (index >= words.length) return false;
    final typed = _challengeCtrls[index]?.text.trim().toLowerCase() ?? '';
    return typed.isNotEmpty && typed == words[index].toLowerCase();
  }

  void _recheckChallenge() {
    _seedConfirmed = _challengeIndices.isNotEmpty &&
        _challengeIndices.every(_isChallengeWordCorrect);
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

// ── Pre-wallet settings dialog ────────────────────────────────────────────────

class _PreWalletSettingsDialog extends ConsumerStatefulWidget {
  const _PreWalletSettingsDialog();

  @override
  ConsumerState<_PreWalletSettingsDialog> createState() => _PreWalletSettingsDialogState();
}

class _PreWalletSettingsDialogState extends ConsumerState<_PreWalletSettingsDialog> {
  late final TextEditingController _hostCtrl;
  late final TextEditingController _portCtrl;
  bool _ssl = false;
  bool _saving = false;
  bool _saved = false;

  @override
  void initState() {
    super.initState();
    final node = ref.read(defaultNodeProvider);
    _hostCtrl = TextEditingController(text: node.host);
    _portCtrl = TextEditingController(text: '${node.port}');
    _ssl = node.ssl;
  }

  @override
  void dispose() {
    _hostCtrl.dispose();
    _portCtrl.dispose();
    super.dispose();
  }

  Future<void> _saveNode() async {
    setState(() { _saving = true; _saved = false; });
    await ref.read(defaultNodeProvider.notifier).set(
      host: _hostCtrl.text.trim(),
      port: int.tryParse(_portCtrl.text) ?? kDefaultDaemonPort,
      ssl: _ssl,
    );
    if (mounted) setState(() { _saving = false; _saved = true; });
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final themeMode = ref.watch(themeModeProvider);
    final logLevel = ref.watch(logLevelProvider);

    return Dialog(
      child: SizedBox(
        width: 500,
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Title bar
              Row(
                children: [
                  const Icon(Icons.settings_outlined, size: 20),
                  const SizedBox(width: 10),
                  Text(tr?.settings ?? 'Settings', style: Theme.of(context).textTheme.titleLarge),
                  const Spacer(),
                  IconButton(
                    icon: const Icon(Icons.close, size: 18),
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
              const Divider(height: 24),

              // Default remote node
              Text(tr?.sectionDaemonNode ?? 'Default Remote Node',
                  style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
              const SizedBox(height: 4),
              Text(tr?.nodeDescription ?? 'Used when creating or opening a wallet.',
                  style: TextStyle(fontSize: 12, color: context.textSecondary)),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    flex: 3,
                    child: TextField(
                      controller: _hostCtrl,
                      decoration: InputDecoration(labelText: tr?.hostIpAddress ?? 'Host / IP'),
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: TextField(
                      controller: _portCtrl,
                      decoration: InputDecoration(labelText: tr?.port ?? 'Port'),
                      keyboardType: TextInputType.number,
                    ),
                  ),
                  const SizedBox(width: 8),
                  Column(
                    children: [
                      Text(tr?.ssl ?? 'SSL', style: TextStyle(fontSize: 12, color: context.textSecondary)),
                      Switch(value: _ssl, onChanged: (v) => setState(() { _ssl = v; _saved = false; })),
                    ],
                  ),
                ],
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  FilledButton(
                    onPressed: _saving ? null : _saveNode,
                    child: _saving
                        ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
                        : Text(tr?.apply ?? 'Apply'),
                  ),
                  if (_saved) ...[
                    const SizedBox(width: 10),
                    const Icon(Icons.check_circle_outline, color: kSuccess, size: 16),
                    const SizedBox(width: 4),
                    Text(tr?.nodeUpdatedSuccess ?? 'Saved', style: const TextStyle(color: kSuccess, fontSize: 13)),
                  ],
                ],
              ),
              const SizedBox(height: 24),

              // Appearance
              Text(tr?.sectionAppearance ?? 'Appearance',
                  style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
              const SizedBox(height: 12),
              SegmentedButton<ThemeMode>(
                segments: [
                  ButtonSegment(value: ThemeMode.system, icon: const Icon(Icons.brightness_auto, size: 16), label: Text(tr?.themeSystem ?? 'System')),
                  ButtonSegment(value: ThemeMode.light,  icon: const Icon(Icons.light_mode, size: 16),      label: Text(tr?.themeLight ?? 'Light')),
                  ButtonSegment(value: ThemeMode.dark,   icon: const Icon(Icons.dark_mode, size: 16),       label: Text(tr?.themeDark ?? 'Dark')),
                ],
                selected: {themeMode},
                onSelectionChanged: (s) => ref.read(themeModeProvider.notifier).set(s.first),
                style: const ButtonStyle(visualDensity: VisualDensity.compact),
              ),
              const SizedBox(height: 24),

              // Log level
              Text(tr?.sectionDebugLogs ?? 'Debug & Logs',
                  style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(tr?.logLevel ?? 'Log Level',
                            style: TextStyle(fontSize: 14, color: Theme.of(context).colorScheme.onSurface)),
                        Text(tr?.logLevelSubtitle ?? 'Controls wallet library verbosity',
                            style: TextStyle(fontSize: 12, color: context.textSecondary)),
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
                      if (l != null) ref.read(logLevelProvider.notifier).set(l);
                    },
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}
