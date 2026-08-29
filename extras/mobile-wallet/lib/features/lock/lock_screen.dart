import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/haptics.dart';

class LockScreen extends ConsumerStatefulWidget {
  const LockScreen({super.key});

  @override
  ConsumerState<LockScreen> createState() => _LockScreenState();
}

class _LockScreenState extends ConsumerState<LockScreen> {
  final _passCtrl = TextEditingController();
  bool _obscure = true;
  bool _loading = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _tryBiometric();
  }

  @override
  void dispose() {
    _passCtrl.dispose();
    super.dispose();
  }

  String? get _walletCaption {
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return null;
    final registry = ref.read(walletRegistryProvider);
    return registry.findByFilename(filename)?.caption;
  }

  Future<void> _tryBiometric() async {
    // Read directly from storage — the provider's async _load() may not have
    // completed yet when initState fires, so ref.read would return the default
    // (false) and biometric would never trigger.
    final stored = await readPref(AppConfig.skBiometricEnabled);
    final biometricOn = stored == 'true';
    if (!biometricOn) return;
    final available = await isBiometricAvailable();
    if (!available) return;
    final ok = await authenticateWithBiometric();
    if (!ok || !mounted) return;

    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;
    // Biometric unlock is the only path that needs the escrowed password.
    final password = await readWalletPassword(filename);
    if (password == null || !mounted) return;
    await _openWallet(password);
  }

  Future<void> _unlock() async {
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    final password = _passCtrl.text;
    if (password.isEmpty) {
      setState(() =>
          _error = S.of(context)?.enterYourPassword ?? 'Enter your password');
      return;
    }

    setState(() {
      _loading = true;
      _error = null;
    });

    final ffi = ref.read(walletCApiProvider);

    // When the wallet is already open — auto-lock re-auth — the file cannot
    // arbitrate, so fall back to the stored verifier. A null result means
    // "cannot tell" (no verifier, or a keychain that lost it); accept the
    // password rather than locking the user out of a wallet they can still
    // open. `_openWallet` re-checks against the file whenever it has to open.
    if (ffi.isOpen) {
      final ok = await verifyPasswordAgainstVerifier(filename, password);
      if (ok == false) {
        hapticError();
        if (!mounted) return;
        setState(() {
          _loading = false;
          _error = S.of(context)?.incorrectPassword ?? 'Incorrect password';
        });
        return;
      }
    }

    await _openWallet(password);
  }

  /// Opens the wallet (if it is not open already) and, on success, refreshes
  /// the stored verifier.
  ///
  /// The wallet file is the authority here: a wrong password surfaces as a
  /// native error, which is what gets reported. Nothing rejects the user
  /// before the file has had a chance to answer.
  Future<void> _openWallet(String password) async {
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    try {
      final ffi = ref.read(walletCApiProvider);
      if (!ffi.isOpen) {
        final registry = ref.read(walletRegistryProvider);
        final walletPath = registry.getWalletPath(filename);

        await ffi.open(
          walletPath,
          password,
          AppConfig.defaultDaemonHost,
          AppConfig.defaultDaemonPort,
          ssl: AppConfig.defaultDaemonSsl,
        );
        // The password just proved itself against the file — (re)record the
        // verifier so a lost or stale keychain entry self-heals.
        await storePasswordVerifier(filename, password);
      }

      ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));

      await ref.read(walletRegistryProvider).setLastOpened(filename);
      ref.read(walletOpenProvider.notifier).state = true;
      ref.read(walletLockedProvider.notifier).state = false;

      hapticMedium();

      if (mounted) context.go('/overview');
    } catch (e) {
      hapticError();
      if (!mounted) return;
      setState(() {
        _loading = false;
        _error = _describeUnlockError(e);
      });
    }
  }

  String _describeUnlockError(Object e) {
    final tr = S.of(context);
    final text = e.toString();
    // wallet_capi reports a bad password as a decryption failure.
    if (text.toLowerCase().contains('password') ||
        text.toLowerCase().contains('decrypt')) {
      return tr?.incorrectPassword ?? 'Incorrect password';
    }
    return text;
  }

  @override
  Widget build(BuildContext context) {
    final caption = _walletCaption ?? 'Wallet';
    final tr = S.of(context);

    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            children: [
              const Spacer(flex: 2),
              Container(
                width: 72,
                height: 72,
                decoration: BoxDecoration(
                  gradient: const LinearGradient(
                    colors: [kPrimary, kAccent],
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                  ),
                  borderRadius: BorderRadius.circular(18),
                ),
                child: const Icon(Icons.lock_outline,
                    size: 36, color: Colors.white),
              ),
              const SizedBox(height: 20),
              Text(caption,
                  style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 8),
              Text(tr?.enterPasswordToUnlock ?? 'Enter your password to unlock',
                  style: Theme.of(context).textTheme.bodySmall),
              const SizedBox(height: 32),
              TextField(
                controller: _passCtrl,
                obscureText: _obscure,
                autofocus: true,
                onSubmitted: (_) => _unlock(),
                decoration: InputDecoration(
                  hintText: tr?.password ?? 'Password',
                  errorText: _error,
                  prefixIcon: const Icon(Icons.lock_outline),
                  suffixIcon: IconButton(
                    icon: Icon(
                        _obscure ? Icons.visibility_off : Icons.visibility),
                    onPressed: () =>
                        setState(() => _obscure = !_obscure),
                  ),
                ),
              ),
              const SizedBox(height: 20),
              FilledButton(
                onPressed: _loading ? null : _unlock,
                child: _loading
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(
                            strokeWidth: 2, color: Colors.white),
                      )
                    : Text(tr?.unlock ?? 'Unlock'),
              ),
              const SizedBox(height: 12),
              TextButton(
                onPressed: () {
                  ref.read(activeWalletFilenameProvider.notifier).state = null;
                  context.go('/picker');
                },
                child: Text(tr?.switchWallet ?? 'Switch Wallet'),
              ),
              const Spacer(flex: 3),
            ],
          ),
        ),
      ),
    );
  }
}
