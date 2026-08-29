import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../core/auth/wallet_auth.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/pluton_logo.dart';

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

  /// Failed attempts since the last success, used to rate-limit guessing.
  int _failedAttempts = 0;
  Timer? _cooldownTimer;
  int _cooldownRemaining = 0;

  /// Attempts allowed before a delay kicks in, and how long that delay grows.
  static const _freeAttempts = 3;
  static const _cooldownStep = Duration(seconds: 5);
  static const _maxCooldown = Duration(minutes: 2);

  @override
  void dispose() {
    _cooldownTimer?.cancel();
    _passCtrl.dispose();
    super.dispose();
  }

  void _startCooldown() {
    final over = _failedAttempts - _freeAttempts;
    if (over <= 0) return;
    // 5s, 10s, 20s, 40s… capped at two minutes.
    var seconds = _cooldownStep.inSeconds * (1 << (over - 1));
    if (seconds > _maxCooldown.inSeconds) seconds = _maxCooldown.inSeconds;

    setState(() => _cooldownRemaining = seconds);
    _cooldownTimer?.cancel();
    _cooldownTimer = Timer.periodic(const Duration(seconds: 1), (t) {
      if (!mounted) {
        t.cancel();
        return;
      }
      setState(() => _cooldownRemaining--);
      if (_cooldownRemaining <= 0) t.cancel();
    });
  }

  Future<void> _unlock() async {
    if (_cooldownRemaining > 0) return;
    final tr = S.of(context);
    setState(() { _loading = true; _error = null; });
    try {
      final ok = await verifyWalletPassword(_passCtrl.text);
      if (!mounted) return;
      if (!ok) {
        _failedAttempts++;
        setState(() => _error = tr?.incorrectPassword ?? 'Incorrect password');
        _startCooldown();
        return;
      }
      _failedAttempts = 0;
      _passCtrl.clear();
      ref.read(walletLockedProvider.notifier).state = false;
      // Resume the background work that locking suspended.
      ref.read(appActiveProvider.notifier).state = true;
      unawaited(refreshAllWalletData(ref));
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  Future<void> _closeWallet() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dlgTr = S.of(ctx);
        return AlertDialog(
          title: Text(dlgTr?.closeWallet ?? 'Close Wallet'),
          content: Text(
              dlgTr?.closeWalletDescription ??
              'This will save and close the wallet.\n\n'
              'You will be returned to the login screen.'),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(dlgTr?.cancel ?? 'Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(dlgTr?.closeWallet ?? 'Close Wallet'),
            ),
          ],
        );
      },
    );
    if (confirmed != true || !mounted) return;

    setState(() => _loading = true);
    final ffi = ref.read(walletCApiProvider);
    try {
      // close() saves first; awaiting it matters — the old code fired it and
      // immediately flipped the state, racing the flush to browser storage.
      await ffi.close();
    } catch (e) {
      debugPrint('[lock] close failed: $e');
    }

    await clearWalletPassword();
    if (!mounted) return;
    ref.read(walletLockedProvider.notifier).state = false;
    ref.read(appActiveProvider.notifier).state = true;
    ref.read(walletOpenProvider.notifier).state = false;
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final blocked = _cooldownRemaining > 0;

    return Scaffold(
      backgroundColor: context.appBackground,
      body: Center(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: SizedBox(
            width: 380,
            child: Card(
              child: Padding(
                padding: const EdgeInsets.all(32),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const PlutonLogo(size: 48),
                    const SizedBox(height: 24),
                    Text(tr?.walletLocked ?? 'Wallet Locked',
                        style: Theme.of(context).textTheme.headlineSmall),
                    const SizedBox(height: 6),
                    Text(
                      tr?.enterPasswordToContinue ?? 'Enter your wallet password to continue',
                      style: TextStyle(color: context.textSecondary, fontSize: 13),
                      textAlign: TextAlign.center,
                    ),
                    const SizedBox(height: 28),
                    TextField(
                      controller: _passCtrl,
                      obscureText: _obscure,
                      autofocus: true,
                      enabled: !blocked && !_loading,
                      onSubmitted: (_) { if (!_loading && !blocked) _unlock(); },
                      decoration: InputDecoration(
                        labelText: tr?.password ?? 'Password',
                        suffixIcon: IconButton(
                          tooltip: _obscure
                              ? (tr?.show ?? 'Show')
                              : (tr?.hide ?? 'Hide'),
                          icon: Icon(
                            _obscure
                                ? Icons.visibility_outlined
                                : Icons.visibility_off_outlined,
                            size: 18,
                          ),
                          onPressed: () => setState(() => _obscure = !_obscure),
                        ),
                      ),
                    ),
                    if (_error != null) ...[
                      const SizedBox(height: 12),
                      _InlineError(
                        message: blocked
                            ? (tr?.tooManyAttempts(_cooldownRemaining) ??
                                'Too many attempts — try again in $_cooldownRemaining s')
                            : _error!,
                      ),
                    ],
                    const SizedBox(height: 20),
                    SizedBox(
                      width: double.infinity,
                      child: FilledButton(
                        onPressed: (_loading || blocked) ? null : _unlock,
                        child: _loading
                            ? const SizedBox(
                                width: 18,
                                height: 18,
                                child: CircularProgressIndicator(
                                    color: Colors.white, strokeWidth: 2),
                              )
                            : Text(blocked
                                ? '${_cooldownRemaining}s'
                                : (tr?.unlock ?? 'Unlock')),
                      ),
                    ),
                    const SizedBox(height: 12),
                    TextButton(
                      onPressed: _loading ? null : _closeWallet,
                      child: Text(
                        tr?.closeWalletInstead ?? 'Close wallet instead',
                        style: TextStyle(color: context.textSecondary, fontSize: 12),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
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
          Expanded(
            child: Text(message, style: const TextStyle(color: kError, fontSize: 13)),
          ),
        ],
      ),
    );
  }
}
