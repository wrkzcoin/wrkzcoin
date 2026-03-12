import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/auth/wallet_auth.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
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
    if (ok && mounted) {
      await _openWallet();
    }
  }

  Future<void> _unlock() async {
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    setState(() {
      _loading = true;
      _error = null;
    });

    final ok = await verifyWalletPassword(filename, _passCtrl.text);
    if (!ok) {
      hapticError();
      setState(() {
        _loading = false;
        _error = 'Incorrect password';
      });
      return;
    }

    await _openWallet();
  }

  Future<void> _openWallet() async {
    final filename = ref.read(activeWalletFilenameProvider);
    if (filename == null) return;

    try {
      final ffi = ref.read(walletCApiProvider);
      if (!ffi.isOpen) {
        final registry = ref.read(walletRegistryProvider);
        final walletPath = registry.getWalletPath(filename);

        // Read stored password for FFI open.
        final password = _passCtrl.text.isNotEmpty
            ? _passCtrl.text
            : await _getStoredPassword(filename);

        if (password == null) {
          setState(() {
            _loading = false;
            _error = 'Enter your password';
          });
          return;
        }

        await ffi.open(
          walletPath,
          password,
          AppConfig.defaultDaemonHost,
          AppConfig.defaultDaemonPort,
          ssl: AppConfig.defaultDaemonSsl,
        );
      }

      // Apply scan coinbase setting
      ffi.setScanCoinbase(ref.read(scanCoinbaseProvider));

      await ref.read(walletRegistryProvider).setLastOpened(filename);
      ref.read(walletOpenProvider.notifier).state = true;
      ref.read(walletLockedProvider.notifier).state = false;

      hapticMedium();

      if (mounted) context.go('/overview');
    } catch (e) {
      hapticError();
      setState(() {
        _loading = false;
        _error = e.toString();
      });
    }
  }

  Future<String?> _getStoredPassword(String filename) async {
    // For biometric unlock, we read the stored password.
    final key = AppConfig.walletPasswordKey(filename);
    return readPref(key);
  }

  @override
  Widget build(BuildContext context) {
    final caption = _walletCaption ?? 'Wallet';

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
              Text('Enter your password to unlock',
                  style: Theme.of(context).textTheme.bodySmall),
              const SizedBox(height: 32),
              TextField(
                controller: _passCtrl,
                obscureText: _obscure,
                autofocus: true,
                onSubmitted: (_) => _unlock(),
                decoration: InputDecoration(
                  hintText: 'Password',
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
                    : const Text('Unlock'),
              ),
              const SizedBox(height: 12),
              TextButton(
                onPressed: () {
                  ref.read(activeWalletFilenameProvider.notifier).state = null;
                  context.go('/picker');
                },
                child: const Text('Switch Wallet'),
              ),
              const Spacer(flex: 3),
            ],
          ),
        ),
      ),
    );
  }
}
