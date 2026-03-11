import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:qr_flutter/qr_flutter.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/providers/providers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/copy_button.dart';

final _primaryAddressProvider = FutureProvider<String>((ref) async {
  final ffi = ref.watch(walletCApiProvider);
  if (!ffi.isOpen) throw Exception('Wallet not connected');
  return ffi.getPrimaryAddress();
});

class ReceiveScreen extends ConsumerStatefulWidget {
  const ReceiveScreen({super.key});

  @override
  ConsumerState<ReceiveScreen> createState() => _ReceiveScreenState();
}

class _ReceiveScreenState extends ConsumerState<ReceiveScreen> {
  final _paymentIdCtrl = TextEditingController();
  String? _integratedAddress;
  bool _generatingIntegrated = false;
  String? _integrationError;

  @override
  void dispose() {
    _paymentIdCtrl.dispose();
    super.dispose();
  }

  Future<void> _generateIntegrated(String baseAddress) async {
    final pid = _paymentIdCtrl.text.trim();
    if (pid.isEmpty) {
      setState(() => _integrationError = 'Enter a payment ID (16 or 64 hex chars)');
      return;
    }
    final validLen = pid.length == 16 || pid.length == 64;
    final validHex = RegExp(r'^[0-9a-fA-F]+$').hasMatch(pid);
    if (!validLen || !validHex) {
      setState(() => _integrationError = 'Payment ID must be 16 or 64 hex characters');
      return;
    }
    setState(() { _generatingIntegrated = true; _integrationError = null; });
    try {
      final integrated = await ref.read(walletCApiProvider)
          .createIntegratedAddress(baseAddress, pid);
      setState(() => _integratedAddress = integrated);
    } on WalletCApiException catch (e) {
      setState(() => _integrationError = e.message);
    } catch (e) {
      setState(() => _integrationError = e.toString());
    } finally {
      if (mounted) setState(() => _generatingIntegrated = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final addrAsync = ref.watch(_primaryAddressProvider);

    return SingleChildScrollView(
      padding: const EdgeInsets.all(28),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('Receive', style: Theme.of(context).textTheme.headlineMedium),
          const SizedBox(height: 6),
          Text('Share your address to receive WRKZ', style: Theme.of(context).textTheme.bodyMedium),
          const SizedBox(height: 24),

          addrAsync.when(
            data: (address) => Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // ── QR code ─────────────────────────────────────────────
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: QrImageView(
                      data: address,
                      version: QrVersions.auto,
                      size: 200,
                      backgroundColor: Colors.white,
                      eyeStyle: const QrEyeStyle(eyeShape: QrEyeShape.square, color: Colors.black),
                      dataModuleStyle: const QrDataModuleStyle(dataModuleShape: QrDataModuleShape.square, color: Colors.black),
                    ),
                  ),
                ),
                const SizedBox(width: 24),

                // ── Address details ──────────────────────────────────────
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('Your Address', style: Theme.of(context).textTheme.titleSmall),
                      const SizedBox(height: 8),
                      _AddressBox(address: address),
                      const SizedBox(height: 24),

                      // ── Integrated address generator ─────────────────
                      Text('Generate Integrated Address', style: Theme.of(context).textTheme.titleSmall),
                      const SizedBox(height: 4),
                      Text(
                        'Combine your address with a payment ID for exchanges or tracking.',
                        style: Theme.of(context).textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 10),
                      Row(
                        children: [
                          Expanded(
                            child: TextField(
                              controller: _paymentIdCtrl,
                              decoration: InputDecoration(
                                labelText: 'Payment ID (16 or 64 hex chars)',
                                errorText: _integrationError,
                              ),
                              onChanged: (_) => setState(() { _integrationError = null; _integratedAddress = null; }),
                            ),
                          ),
                          const SizedBox(width: 10),
                          FilledButton(
                            onPressed: _generatingIntegrated ? null : () => _generateIntegrated(address),
                            child: _generatingIntegrated
                                ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
                                : const Text('Generate'),
                          ),
                        ],
                      ),
                      if (_integratedAddress != null) ...[
                        const SizedBox(height: 12),
                        Text('Integrated Address', style: Theme.of(context).textTheme.titleSmall),
                        const SizedBox(height: 8),
                        _AddressBox(address: _integratedAddress!),
                      ],
                    ],
                  ),
                ),
              ],
            ),
            loading: () => const Center(child: CircularProgressIndicator()),
            error: (e, _) => Text('Error: $e', style: const TextStyle(color: kError)),
          ),
        ],
      ),
    );
  }
}

class _AddressBox extends StatelessWidget {
  final String address;
  const _AddressBox({required this.address});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: kSurfaceVariant,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: kDivider),
      ),
      child: Row(
        children: [
          Expanded(
            child: SelectableText(
              address,
              style: const TextStyle(fontSize: 12, fontFamily: 'monospace', color: kTextPrimary, height: 1.5),
            ),
          ),
          CopyButton(text: address, tooltip: 'Copy address'),
        ],
      ),
    );
  }
}
