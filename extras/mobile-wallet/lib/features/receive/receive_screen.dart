import 'dart:math';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:qr_flutter/qr_flutter.dart';
import 'package:share_plus/share_plus.dart';

import '../../core/providers/providers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/copy_button.dart';

class ReceiveScreen extends ConsumerStatefulWidget {
  const ReceiveScreen({super.key});

  @override
  ConsumerState<ReceiveScreen> createState() => _ReceiveScreenState();
}

class _ReceiveScreenState extends ConsumerState<ReceiveScreen> {
  String _address = '';
  bool _loading = true;

  // integrated address
  final _pidCtrl = TextEditingController();
  String? _integratedAddress;
  String? _integratedPid;
  String? _pidError;

  @override
  void initState() {
    super.initState();
    _loadAddress();
  }

  @override
  void dispose() {
    _pidCtrl.dispose();
    super.dispose();
  }

  bool _addressFailed = false;

  Future<void> _loadAddress() async {
    try {
      final ffi = ref.read(walletCApiProvider);
      final addr = await ffi.getPrimaryAddress();
      if (!mounted) return;
      setState(() {
        _address = addr;
        _addressFailed = false;
        _loading = false;
      });
    } catch (_) {
      if (!mounted) return;
      // Flag the failure rather than storing the message in `_address` —
      // that string used to end up encoded in the QR code and copied by the
      // copy/share buttons.
      setState(() {
        _address = '';
        _addressFailed = true;
        _loading = false;
      });
    }
  }

  String _randomHex(int length) {
    final rng = Random.secure();
    return List.generate(
        length, (_) => rng.nextInt(16).toRadixString(16)).join();
  }

  Future<void> _generateIntegrated(String paymentId) async {
    try {
      final ffi = ref.read(walletCApiProvider);
      final integrated =
          await ffi.createIntegratedAddress(_address, paymentId);
      setState(() {
        _integratedAddress = integrated;
        _integratedPid = paymentId;
        _pidError = null;
      });
      hapticLight();
    } catch (e) {
      setState(() => _pidError = e.toString());
    }
  }

  void _validateAndGenerate(BuildContext context) {
    final tr = S.of(context)!;
    final pid = _pidCtrl.text.trim();
    if (pid.isEmpty) {
      setState(() => _pidError = tr.enterPaymentId);
      return;
    }
    final validLen = pid.length == 16 || pid.length == 64;
    final validHex = RegExp(r'^[0-9a-fA-F]+$').hasMatch(pid);
    if (!validLen || !validHex) {
      setState(() => _pidError = tr.paymentIdInvalid);
      return;
    }
    _generateIntegrated(pid);
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;

    if (_loading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (_addressFailed || _address.isEmpty) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.error_outline, color: kError, size: 40),
              const SizedBox(height: 12),
              Text(tr.errorLoadingAddress, textAlign: TextAlign.center),
              const SizedBox(height: 16),
              OutlinedButton(
                onPressed: () {
                  setState(() => _loading = true);
                  _loadAddress();
                },
                child: Text(tr.retry),
              ),
            ],
          ),
        ),
      );
    }

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Primary address
        Card(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: Column(
              children: [
                Text(tr.yourAddress,
                    style: Theme.of(context).textTheme.titleMedium),
                const SizedBox(height: 16),
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Colors.white,
                    borderRadius: BorderRadius.circular(12),
                  ),
                  child: QrImageView(
                    data: _address,
                    version: QrVersions.auto,
                    size: 200,
                    backgroundColor: Colors.white,
                    eyeStyle: const QrEyeStyle(
                      eyeShape: QrEyeShape.square,
                      color: Colors.black,
                    ),
                    dataModuleStyle: const QrDataModuleStyle(
                      dataModuleShape: QrDataModuleShape.square,
                      color: Colors.black,
                    ),
                  ),
                ),
                const SizedBox(height: 16),
                Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Theme.of(context).colorScheme.surface,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                        color: Theme.of(context).dividerColor),
                  ),
                  child: SelectableText(
                    _address,
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          fontFamily: 'monospace',
                          fontSize: 11,
                        ),
                    textAlign: TextAlign.center,
                  ),
                ),
                const SizedBox(height: 12),
                Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    CopyButton(text: _address),
                    const SizedBox(width: 8),
                    IconButton(
                      icon: Icon(Icons.share,
                          size: 20,
                          color: Theme.of(context)
                              .textTheme
                              .bodySmall
                              ?.color),
                      onPressed: () {
                        Share.share(_address);
                        hapticLight();
                      },
                      tooltip: tr.share,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),

        const SizedBox(height: 24),

        // Integrated address generator
        Text(tr.integratedAddress,
            style: Theme.of(context).textTheme.titleLarge),
        const SizedBox(height: 4),
        Text(tr.embedPaymentId,
            style: Theme.of(context).textTheme.bodySmall),
        const SizedBox(height: 12),

        Row(
          children: [
            Expanded(
              child: OutlinedButton(
                onPressed: () => _generateIntegrated(_randomHex(16)),
                child: Text(tr.randomShort),
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: OutlinedButton(
                onPressed: () => _generateIntegrated(_randomHex(64)),
                child: Text(tr.randomLong),
              ),
            ),
          ],
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _pidCtrl,
          decoration: InputDecoration(
            hintText: tr.enterCustomPaymentId,
            errorText: _pidError,
            suffixIcon: IconButton(
              icon: const Icon(Icons.check),
              onPressed: () => _validateAndGenerate(context),
            ),
          ),
          onSubmitted: (_) => _validateAndGenerate(context),
        ),

        // Integrated address result
        if (_integratedAddress != null) ...[
          const SizedBox(height: 16),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Container(
                        padding: const EdgeInsets.symmetric(
                            horizontal: 8, vertical: 3),
                        decoration: BoxDecoration(
                          color: kAccent.withAlpha(25),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Text(
                          _integratedPid!.length == 16
                              ? tr.shortPid
                              : tr.longPid,
                          style: Theme.of(context)
                              .textTheme
                              .labelSmall
                              ?.copyWith(color: kAccent),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: Colors.white,
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: QrImageView(
                      data: _integratedAddress!,
                      version: QrVersions.auto,
                      size: 180,
                      backgroundColor: Colors.white,
                      eyeStyle: const QrEyeStyle(
                        eyeShape: QrEyeShape.square,
                        color: Colors.black,
                      ),
                      dataModuleStyle: const QrDataModuleStyle(
                        dataModuleShape: QrDataModuleShape.square,
                        color: Colors.black,
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  Container(
                    width: double.infinity,
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: Theme.of(context).colorScheme.surface,
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                          color: Theme.of(context).dividerColor),
                    ),
                    child: SelectableText(
                      _integratedAddress!,
                      style:
                          Theme.of(context).textTheme.bodySmall?.copyWith(
                                fontFamily: 'monospace',
                                fontSize: 11,
                              ),
                      textAlign: TextAlign.center,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      CopyButton(text: _integratedAddress!),
                      const SizedBox(width: 8),
                      IconButton(
                        icon: Icon(Icons.share,
                            size: 20,
                            color: Theme.of(context)
                                .textTheme
                                .bodySmall
                                ?.color),
                        onPressed: () {
                          Share.share(_integratedAddress!);
                          hapticLight();
                        },
                        tooltip: tr.share,
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
        ],
      ],
    );
  }
}
