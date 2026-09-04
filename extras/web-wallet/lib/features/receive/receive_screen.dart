import 'dart:math';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:qr_flutter/qr_flutter.dart';
import '../../core/ffi/wallet_web.dart';
import '../../core/providers/providers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/copy_button.dart';

// The address provider lives in core/providers so it can be keyed on the
// wallet session. Held here it cached the first wallet's address for the life
// of the page, and every wallet opened afterwards was shown that address.

// ── helpers ───────────────────────────────────────────────────────────────────

String _randomHex(int length) {
  const chars = '0123456789abcdef';
  final rng = Random.secure();
  return List.generate(length, (_) => chars[rng.nextInt(16)]).join();
}

class ReceiveScreen extends ConsumerStatefulWidget {
  const ReceiveScreen({super.key});

  @override
  ConsumerState<ReceiveScreen> createState() => _ReceiveScreenState();
}

class _ReceiveScreenState extends ConsumerState<ReceiveScreen> {
  final _paymentIdCtrl = TextEditingController();

  // result state
  String? _integratedAddress;
  String? _usedPaymentId;
  bool _generating = false;
  String? _error;

  @override
  void dispose() {
    _paymentIdCtrl.dispose();
    super.dispose();
  }

  Future<void> _generate(String baseAddress, {String? overridePid}) async {
    final tr = S.of(context);
    String pid;
    if (overridePid != null) {
      pid = overridePid;
    } else {
      pid = _paymentIdCtrl.text.trim();
      if (pid.isEmpty) {
        setState(() => _error = tr?.enterPaymentIdError ?? 'Enter a payment ID (16 or 64 hex chars)');
        return;
      }
      final validLen = pid.length == 16 || pid.length == 64;
      final validHex = RegExp(r'^[0-9a-fA-F]+$').hasMatch(pid);
      if (!validLen || !validHex) {
        setState(() => _error = tr?.paymentIdInvalidError ?? 'Payment ID must be 16 or 64 hex characters');
        return;
      }
    }

    setState(() { _generating = true; _error = null; });
    try {
      final integrated = await ref.read(walletCApiProvider)
          .createIntegratedAddress(baseAddress, pid);
      setState(() {
        _integratedAddress = integrated;
        _usedPaymentId = pid;
      });
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = tr?.errorPrefix(e.toString()) ?? 'Error: $e');
    } finally {
      if (mounted) setState(() => _generating = false);
    }
  }

  void _onCustomPidChanged() {
    setState(() { _error = null; _integratedAddress = null; _usedPaymentId = null; });
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final addrAsync = ref.watch(primaryAddressProvider);

    return SingleChildScrollView(
      padding: const EdgeInsets.all(28),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(tr?.tabReceive ?? 'Receive', style: Theme.of(context).textTheme.headlineMedium),
          const SizedBox(height: 6),
          Text(tr?.shareAddressSubtitle ?? 'Share your address to receive WRKZ',
              style: Theme.of(context).textTheme.bodyMedium),
          const SizedBox(height: 24),

          addrAsync.when(
            data: (address) => Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // ── Primary address row ──────────────────────────────────
                Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    _QrCard(data: address),
                    const SizedBox(width: 24),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(tr?.yourAddress ?? 'Your Address',
                              style: Theme.of(context).textTheme.titleSmall),
                          const SizedBox(height: 8),
                          _AddressBox(address: address),
                        ],
                      ),
                    ),
                  ],
                ),

                const SizedBox(height: 32),
                const Divider(),
                const SizedBox(height: 24),

                // ── Integrated address generator ─────────────────────────
                Text(tr?.generateIntegratedAddress ?? 'Generate Integrated Address',
                    style: Theme.of(context).textTheme.titleMedium),
                const SizedBox(height: 4),
                Text(
                  tr?.integratedAddressDescription ?? 'Combine your address with a payment ID. Use the random buttons for a new ID, or enter your own below.',
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
                const SizedBox(height: 16),

                // Random buttons
                Row(
                  children: [
                    FilledButton.icon(
                      onPressed: _generating
                          ? null
                          : () => _generate(address,
                              overridePid: _randomHex(16)),
                      icon: const Icon(Icons.shuffle, size: 16),
                      label: Text(tr?.randomShort16 ?? 'Random Short (16)'),
                    ),
                    const SizedBox(width: 12),
                    FilledButton.icon(
                      onPressed: _generating
                          ? null
                          : () => _generate(address,
                              overridePid: _randomHex(64)),
                      icon: const Icon(Icons.shuffle, size: 16),
                      label: Text(tr?.randomLong64 ?? 'Random Long (64)'),
                    ),
                    if (_generating) ...[
                      const SizedBox(width: 16),
                      const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2)),
                    ],
                  ],
                ),

                const SizedBox(height: 16),

                // Custom payment ID field
                Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _paymentIdCtrl,
                        decoration: InputDecoration(
                          labelText: tr?.customPaymentIdLabel ?? 'Custom payment ID (16 or 64 hex chars)',
                          errorText: _error,
                          suffixIcon: _paymentIdCtrl.text.isNotEmpty
                              ? IconButton(
                                  icon: const Icon(Icons.clear, size: 18),
                                  onPressed: () {
                                    _paymentIdCtrl.clear();
                                    _onCustomPidChanged();
                                  },
                                )
                              : null,
                        ),
                        onChanged: (_) => _onCustomPidChanged(),
                      ),
                    ),
                    const SizedBox(width: 10),
                    Padding(
                      padding: const EdgeInsets.only(top: 4),
                      child: OutlinedButton(
                        onPressed:
                            _generating ? null : () => _generate(address),
                        child: Text(tr?.generate ?? 'Generate'),
                      ),
                    ),
                  ],
                ),

                // ── Result ──────────────────────────────────────────────
                if (_integratedAddress != null && _usedPaymentId != null) ...[
                  const SizedBox(height: 28),
                  const Divider(),
                  const SizedBox(height: 20),
                  Text(tr?.integratedAddress ?? 'Integrated Address',
                      style: Theme.of(context).textTheme.titleSmall),
                  const SizedBox(height: 4),
                  _PaymentIdBadge(paymentId: _usedPaymentId!),
                  const SizedBox(height: 16),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      _QrCard(data: _integratedAddress!),
                      const SizedBox(width: 24),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            const SizedBox(height: 4),
                            _AddressBox(address: _integratedAddress!),
                          ],
                        ),
                      ),
                    ],
                  ),
                ],
              ],
            ),
            loading: () => const Center(child: CircularProgressIndicator()),
            error: (e, _) =>
                Text(tr?.errorPrefix(e.toString()) ?? 'Error: $e', style: const TextStyle(color: kError)),
          ),
        ],
      ),
    );
  }
}

// ── shared widgets ────────────────────────────────────────────────────────────

class _QrCard extends StatelessWidget {
  final String data;
  const _QrCard({required this.data});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: QrImageView(
          data: data,
          version: QrVersions.auto,
          size: 200,
          backgroundColor: Colors.white,
          eyeStyle: const QrEyeStyle(
              eyeShape: QrEyeShape.square, color: Colors.black),
          dataModuleStyle: const QrDataModuleStyle(
              dataModuleShape: QrDataModuleShape.square, color: Colors.black),
        ),
      ),
    );
  }
}

class _AddressBox extends StatelessWidget {
  final String address;
  const _AddressBox({required this.address});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
      ),
      child: Row(
        children: [
          Expanded(
            child: SelectableText(
              address,
              style: TextStyle(
                  fontSize: 12,
                  fontFamily: 'monospace',
                  color: Theme.of(context).colorScheme.onSurface,
                  height: 1.5),
            ),
          ),
          CopyButton(text: address, tooltip: tr?.copyAddress ?? 'Copy address'),
        ],
      ),
    );
  }
}

class _PaymentIdBadge extends StatelessWidget {
  final String paymentId;
  const _PaymentIdBadge({required this.paymentId});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final label = paymentId.length == 16
        ? (tr?.paymentIdShort ?? 'Short (16)')
        : (tr?.paymentIdLong ?? 'Long (64)');
    return Row(
      children: [
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
          decoration: BoxDecoration(
            color: Theme.of(context).colorScheme.surfaceContainerHighest,
            borderRadius: BorderRadius.circular(4),
            border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
          ),
          child: Text(
            tr?.paymentIdLabel(label) ?? 'Payment ID \u00b7 $label',
            style: Theme.of(context)
                .textTheme
                .labelSmall
                ?.copyWith(color: Theme.of(context).colorScheme.onSurfaceVariant),
          ),
        ),
        const SizedBox(width: 8),
        Expanded(
          child: SelectableText(
            paymentId,
            style: TextStyle(
                fontSize: 11,
                fontFamily: 'monospace',
                color: Theme.of(context).colorScheme.onSurface),
          ),
        ),
        CopyButton(text: paymentId, tooltip: tr?.copyPaymentId ?? 'Copy payment ID'),
      ],
    );
  }
}
