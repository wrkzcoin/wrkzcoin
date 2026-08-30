import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:share_plus/share_plus.dart';
import '../../core/api/models/transaction.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/copy_button.dart';

enum _TransferStep { form, review, success }

class TransferScreen extends ConsumerStatefulWidget {
  const TransferScreen({super.key});

  @override
  ConsumerState<TransferScreen> createState() => _TransferScreenState();
}

class _TransferScreenState extends ConsumerState<TransferScreen> {
  _TransferStep _step = _TransferStep.form;
  bool _sweepMode = false;

  // form
  final _addressCtrl = TextEditingController();
  final _amountCtrl = TextEditingController();
  final _pidCtrl = TextEditingController();
  bool _loading = false;
  String? _error;

  // prepared tx
  SendResult? _prepared;

  // success
  String _sentTxHash = '';

  @override
  void dispose() {
    _addressCtrl.dispose();
    _amountCtrl.dispose();
    _pidCtrl.dispose();
    super.dispose();
  }

  // ── QR scan ──────────────────────────────────────────────────────────────

  Future<void> _scanQr() async {
    final result = await Navigator.of(context).push<String>(
      MaterialPageRoute(builder: (_) => const _QrScanPage()),
    );
    if (result == null || !mounted) return;

    // Accepts a bare address or a wrkz:<address>?amount=…&paymentId=… URI.
    final scanned = parseAddressPayload(result);

    // Confirmation dialog
    final use = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final dtr = S.of(ctx)!;
        return AlertDialog(
          title: Text(dtr.scannedAddress),
          content: SelectableText(
            scanned.address,
            style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: Text(dtr.cancel),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: Text(dtr.useThisAddress),
            ),
          ],
        );
      },
    );
    if (use != true) return;
    setState(() {
      _addressCtrl.text = scanned.address;
      if (scanned.amount != null && parseAmount(scanned.amount!) != null) {
        _amountCtrl.text = scanned.amount!;
      }
      if (scanned.paymentId != null && isValidPaymentId(scanned.paymentId!)) {
        _pidCtrl.text = scanned.paymentId!;
      }
    });
  }

  // ── prepare ──────────────────────────────────────────────────────────────

  Future<void> _prepare() async {
    final tr = S.of(context)!;
    final address = _addressCtrl.text.trim();
    if (address.isEmpty) {
      setState(() => _error = tr.recipientRequired);
      return;
    }
    if (!isValidWrkzAddress(address)) {
      setState(() => _error = tr.invalidAddress);
      return;
    }

    if (!_sweepMode) {
      final amount = parseAmount(_amountCtrl.text);
      if (amount == null || amount <= 0) {
        setState(() => _error = tr.enterValidAmount);
        return;
      }
    }

    if (!isValidPaymentId(_pidCtrl.text.trim())) {
      setState(() => _error = tr.paymentIdInvalid);
      return;
    }

    setState(() {
      _loading = true;
      _error = null;
    });

    try {
      final ffi = ref.read(walletCApiProvider);

      if (_sweepMode) {
        // For sweep, go directly to success (sweep sends immediately).
        final result = await ffi.sweepToAddress(
          address,
          paymentId: _pidCtrl.text.trim(),
        );
        final results = result['results'] as List<dynamic>? ?? [];
        final successes =
            results.where((r) => (r as Map).containsKey('txHash')).toList();
        if (successes.isEmpty) {
          final firstErr = results.isNotEmpty
              ? (results.first as Map)['errorMessage'] ?? tr.sweepFailed
              : tr.sweepFailed;
          throw Exception(firstErr);
        }
        final txHash = (successes.first as Map)['txHash'] as String;
        ref.read(balanceProvider.notifier).refresh();
        ref.read(transactionsProvider.notifier).refresh();
        hapticHeavy();
        if (!mounted) return;
        setState(() {
          _sentTxHash = txHash;
          _step = _TransferStep.success;
          _loading = false;
        });
        return;
      }

      // Normal send: prepare without broadcasting.
      final amount = parseAmount(_amountCtrl.text)!;
      final request = {
        'destinations': [
          {'address': address, 'amount': amount}
        ],
        if (_pidCtrl.text.trim().isNotEmpty)
          'paymentID': _pidCtrl.text.trim(),
      };
      final result =
          await ffi.sendAdvanced(jsonEncode(request), broadcast: false);
      _prepared = SendResult.fromJson(result);

      if (!mounted) return;
      setState(() {
        _step = _TransferStep.review;
        _loading = false;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = _describeError(e);
        _loading = false;
      });
      hapticError();
    }
  }

  /// A prepared transaction goes stale once one of its inputs is spent
  /// elsewhere; say so plainly instead of surfacing the raw native error.
  String _describeError(Object e) {
    final tr = S.of(context)!;
    if (e.toString().toLowerCase().contains('prepared')) {
      return tr.preparedTransactionExpired;
    }
    return e.toString();
  }

  // ── send ─────────────────────────────────────────────────────────────────

  Future<void> _send() async {
    if (_prepared == null) return;
    setState(() {
      _loading = true;
      _error = null;
    });

    try {
      final ffi = ref.read(walletCApiProvider);
      final preparedHash = _prepared!.transactionHash;
      // Report the hash the network actually accepted, not the prepared one.
      final txHash = await ffi.sendPrepared(preparedHash);
      ref.read(balanceProvider.notifier).refresh();
      ref.read(transactionsProvider.notifier).refresh();
      hapticHeavy();
      if (!mounted) return;
      setState(() {
        _sentTxHash = txHash.isNotEmpty ? txHash : preparedHash;
        _step = _TransferStep.success;
        _loading = false;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = _describeError(e);
        _loading = false;
      });
      hapticError();
    }
  }

  void _reset() {
    setState(() {
      _step = _TransferStep.form;
      _addressCtrl.clear();
      _amountCtrl.clear();
      _pidCtrl.clear();
      _prepared = null;
      _sentTxHash = '';
      _error = null;
      _sweepMode = false;
    });
  }

  /// Returns to the form, dropping the prepared transaction from the native
  /// cache (a no-op on wallet_capi builds without the export).
  void _cancelPrepared() {
    final prepared = _prepared;
    if (prepared != null) {
      final hash = prepared.transactionHash;
      if (hash.isNotEmpty) {
        try {
          ref.read(walletCApiProvider).deletePrepared(hash);
        } catch (_) {
          // Best effort — the entry is dropped when the wallet closes.
        }
      }
    }
    setState(() {
      _step = _TransferStep.form;
      _prepared = null;
      _error = null;
    });
  }

  // ── build ────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    switch (_step) {
      case _TransferStep.form:
        return _buildForm();
      case _TransferStep.review:
        return _buildReview();
      case _TransferStep.success:
        return _buildSuccess();
    }
  }

  // ── form ─────────────────────────────────────────────────────────────────

  Widget _buildForm() {
    final tr = S.of(context)!;
    final balanceAsync = ref.watch(balanceProvider);
    final available = balanceAsync.valueOrNull?.unlocked ?? 0;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Sweep toggle
        Row(
          children: [
            Text(
              _sweepMode ? tr.sweepAllFunds : tr.send,
              style: Theme.of(context).textTheme.titleLarge,
            ),
            const Spacer(),
            TextButton(
              onPressed: () => setState(() => _sweepMode = !_sweepMode),
              child: Text(_sweepMode ? tr.normalSend : tr.sweep),
            ),
          ],
        ),
        const SizedBox(height: 16),

        // Address
        TextField(
          controller: _addressCtrl,
          decoration: InputDecoration(
            labelText: tr.recipientAddress,
            hintText: 'Wrkz...',
            suffixIcon: IconButton(
              icon: const Icon(Icons.qr_code_scanner),
              onPressed: _scanQr,
              tooltip: tr.scanQr,
            ),
          ),
          maxLines: 2,
          minLines: 1,
        ),
        const SizedBox(height: 16),

        // Amount (only for normal send)
        if (!_sweepMode) ...[
          TextField(
            controller: _amountCtrl,
            keyboardType: const TextInputType.numberWithOptions(decimal: true),
            decoration: InputDecoration(
              labelText: tr.amount,
              hintText: '0.00',
              helperText: tr.availableBalance(
                  formatAmount(available, showTicker: true)),
            ),
          ),
          const SizedBox(height: 16),
        ] else ...[
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: kPrimary.withAlpha(15),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              children: [
                const Icon(Icons.info_outline, color: kPrimary, size: 18),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    tr.sweepInfo(
                        formatAmount(available, showTicker: true)),
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
        ],

        // Payment ID
        TextField(
          controller: _pidCtrl,
          onChanged: (_) => setState(() {}),
          decoration: InputDecoration(
            labelText: tr.paymentIdOptional,
            hintText: tr.hexCharacters,
            errorText: isValidPaymentId(_pidCtrl.text.trim())
                ? null
                : tr.mustBeHex,
          ),
        ),
        const SizedBox(height: 8),

        // Error
        if (_error != null) ...[
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: kError.withAlpha(25),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Text(_error!,
                style: const TextStyle(color: kError, fontSize: 13)),
          ),
        ],

        const SizedBox(height: 24),

        FilledButton(
          onPressed: _loading ? null : _prepare,
          child: _loading
              ? const SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(
                      strokeWidth: 2, color: Colors.white),
                )
              : Text(_sweepMode ? tr.sweepAllFunds : tr.reviewTransaction),
        ),
      ],
    );
  }

  // ── review ───────────────────────────────────────────────────────────────

  Widget _buildReview() {
    final tr = S.of(context)!;
    final p = _prepared!;
    final amount = parseAmount(_amountCtrl.text) ?? 0;
    final total = amount + p.fee;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(tr.reviewTransaction,
            style: Theme.of(context).textTheme.titleLarge),
        const SizedBox(height: 20),

        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                _reviewRow(tr.to, _addressCtrl.text.trim()),
                const Divider(height: 24),
                _reviewRow(tr.amount,
                    formatAmount(amount, showTicker: true)),
                _reviewRow(tr.fee,
                    formatAmount(p.fee, showTicker: true)),
                const Divider(height: 24),
                _reviewRow(tr.totalDeducted,
                    formatAmount(total, showTicker: true)),
                if (_pidCtrl.text.trim().isNotEmpty) ...[
                  const Divider(height: 24),
                  _reviewRow(tr.paymentId, _pidCtrl.text.trim()),
                ],
                // Zero means the native library predates reporting it, and a
                // made-up ring size would be worse than none.
                if (p.defaultMixin > 0) ...[
                  const Divider(height: 24),
                  _reviewRow(tr.ringSize, '${p.mixin + 1}'),
                ],
              ],
            ),
          ),
        ),

        const SizedBox(height: 16),
        Container(
          padding: const EdgeInsets.all(12),
          decoration: BoxDecoration(
            color: kWarning.withAlpha(25),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Row(
            children: [
              const Icon(Icons.warning_amber, color: kWarning, size: 18),
              const SizedBox(width: 10),
              Expanded(
                child: Text(
                  tr.transactionsIrreversible,
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: kWarning),
                ),
              ),
            ],
          ),
        ),

        // Say this before the send is approved, not after: the transaction is
        // less private than usual, and that is not something to find out once
        // it is on the chain.
        if (p.isMixinDegraded) ...[
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: kWarning.withAlpha(25),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              children: [
                const Icon(Icons.privacy_tip_outlined,
                    color: kWarning, size: 18),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(
                    tr.ringSizeReduced(p.mixin + 1, p.defaultMixin + 1),
                    style: Theme.of(context)
                        .textTheme
                        .bodySmall
                        ?.copyWith(color: kWarning),
                  ),
                ),
              ],
            ),
          ),
        ],

        if (_error != null) ...[
          const SizedBox(height: 12),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: kError.withAlpha(25),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Text(_error!,
                style: const TextStyle(color: kError, fontSize: 13)),
          ),
        ],

        const SizedBox(height: 24),
        Row(
          children: [
            Expanded(
              child: OutlinedButton(
                onPressed: _loading ? null : _cancelPrepared,
                child: Text(tr.back),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              flex: 2,
              child: FilledButton(
                onPressed: _loading ? null : _send,
                child: _loading
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(
                            strokeWidth: 2, color: Colors.white),
                      )
                    : Text(tr.confirmAndSend),
              ),
            ),
          ],
        ),
      ],
    );
  }

  Widget _reviewRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 100,
            child: Text(label,
                style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(
            child: Text(value,
                style: Theme.of(context).textTheme.bodyMedium,
                textAlign: TextAlign.end),
          ),
        ],
      ),
    );
  }

  // ── success ──────────────────────────────────────────────────────────────

  Widget _buildSuccess() {
    final tr = S.of(context)!;

    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(
              width: 72,
              height: 72,
              decoration: BoxDecoration(
                color: kSuccess.withAlpha(25),
                shape: BoxShape.circle,
              ),
              child: const Icon(Icons.check, color: kSuccess, size: 40),
            ),
            const SizedBox(height: 20),
            Text(tr.transactionSent,
                style: Theme.of(context).textTheme.headlineMedium),
            const SizedBox(height: 20),
            Text(tr.transactionHash,
                style: Theme.of(context).textTheme.labelLarge),
            const SizedBox(height: 8),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Theme.of(context).colorScheme.surface,
                borderRadius: BorderRadius.circular(8),
                border:
                    Border.all(color: Theme.of(context).dividerColor),
              ),
              child: SelectableText(
                _sentTxHash,
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
                CopyButton(text: _sentTxHash),
                const SizedBox(width: 8),
                IconButton(
                  icon: Icon(Icons.share,
                      size: 20,
                      color:
                          Theme.of(context).textTheme.bodySmall?.color),
                  tooltip: tr.share,
                  onPressed: () {
                    hapticLight();
                    Share.share(_sentTxHash);
                  },
                ),
              ],
            ),
            const SizedBox(height: 32),
            OutlinedButton(
              onPressed: _reset,
              child: Text(tr.sendAnother),
            ),
          ],
        ),
      ),
    );
  }
}

// ── QR scanner page ──────────────────────────────────────────────────────────

class _QrScanPage extends StatefulWidget {
  const _QrScanPage();

  @override
  State<_QrScanPage> createState() => _QrScanPageState();
}

class _QrScanPageState extends State<_QrScanPage> {
  final _controller = MobileScannerController();
  bool _scanned = false;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    return Scaffold(
      appBar: AppBar(title: Text(tr.scanQrCode)),
      body: MobileScanner(
        controller: _controller,
        onDetect: (capture) {
          if (_scanned || !mounted) return;
          final barcode = capture.barcodes.firstOrNull;
          if (barcode?.rawValue == null) return;
          _scanned = true;
          hapticMedium();
          Navigator.of(context).pop(barcode!.rawValue);
        },
      ),
    );
  }
}
