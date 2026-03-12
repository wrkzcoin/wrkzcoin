import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
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
  Map<String, dynamic>? _prepared;
  int _preparedFee = 0;

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

    // Confirmation dialog
    final use = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Scanned Address'),
        content: SelectableText(
          result,
          style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Use this address'),
          ),
        ],
      ),
    );
    if (use == true) {
      _addressCtrl.text = result;
    }
  }

  // ── prepare ──────────────────────────────────────────────────────────────

  Future<void> _prepare() async {
    final address = _addressCtrl.text.trim();
    if (address.isEmpty) {
      setState(() => _error = 'Recipient address is required');
      return;
    }
    if (!isValidWrkzAddress(address)) {
      setState(() => _error = 'Invalid WRKZ address');
      return;
    }

    if (!_sweepMode) {
      final amount = parseAmount(_amountCtrl.text);
      if (amount == null || amount <= 0) {
        setState(() => _error = 'Enter a valid amount');
        return;
      }
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
              ? (results.first as Map)['errorMessage'] ?? 'Sweep failed'
              : 'Sweep failed';
          throw Exception(firstErr);
        }
        final txHash = (successes.first as Map)['txHash'] as String;
        ref.read(balanceProvider.notifier).refresh();
        ref.read(transactionsProvider.notifier).refresh();
        hapticHeavy();
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
      _prepared = result;
      _preparedFee = (result['fee'] as num?)?.toInt() ?? 0;

      setState(() {
        _step = _TransferStep.review;
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _error = e.toString();
        _loading = false;
      });
      hapticError();
    }
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
      final txHash = _prepared!['transactionHash'] as String;
      await ffi.sendPrepared(txHash);
      ref.read(balanceProvider.notifier).refresh();
      ref.read(transactionsProvider.notifier).refresh();
      hapticHeavy();
      setState(() {
        _sentTxHash = txHash;
        _step = _TransferStep.success;
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _error = e.toString();
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
    final balanceAsync = ref.watch(balanceProvider);
    final available = balanceAsync.valueOrNull?.unlocked ?? 0;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        // Sweep toggle
        Row(
          children: [
            Text(
              _sweepMode ? 'Sweep All Funds' : 'Send',
              style: Theme.of(context).textTheme.titleLarge,
            ),
            const Spacer(),
            TextButton(
              onPressed: () => setState(() => _sweepMode = !_sweepMode),
              child: Text(_sweepMode ? 'Normal Send' : 'Sweep'),
            ),
          ],
        ),
        const SizedBox(height: 16),

        // Address
        TextField(
          controller: _addressCtrl,
          decoration: InputDecoration(
            labelText: 'Recipient Address',
            hintText: 'Wrkz...',
            suffixIcon: IconButton(
              icon: const Icon(Icons.qr_code_scanner),
              onPressed: _scanQr,
              tooltip: 'Scan QR',
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
              labelText: 'Amount',
              hintText: '0.00',
              helperText:
                  'Available: ${formatAmount(available, showTicker: true)}',
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
                    'Sweep consolidates all UTXOs and sends your entire unlocked balance (${formatAmount(available, showTicker: true)}) minus fees.',
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
          decoration: const InputDecoration(
            labelText: 'Payment ID (optional)',
            hintText: '16 or 64 hex characters',
          ),
        ),
        const SizedBox(height: 8),

        // Validate PID if entered
        if (_pidCtrl.text.trim().isNotEmpty) ...[
          Builder(builder: (_) {
            final pid = _pidCtrl.text.trim();
            final valid = (pid.length == 16 || pid.length == 64) &&
                RegExp(r'^[0-9a-fA-F]+$').hasMatch(pid);
            if (!valid) {
              return Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text('Must be 16 or 64 hex characters',
                    style: TextStyle(color: kError, fontSize: 12)),
              );
            }
            return const SizedBox.shrink();
          }),
        ],

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
              : Text(_sweepMode ? 'Sweep All Funds' : 'Review Transaction'),
        ),
      ],
    );
  }

  // ── review ───────────────────────────────────────────────────────────────

  Widget _buildReview() {
    final amount = parseAmount(_amountCtrl.text) ?? 0;
    final total = amount + _preparedFee;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text('Review Transaction',
            style: Theme.of(context).textTheme.titleLarge),
        const SizedBox(height: 20),

        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                _reviewRow('To', _addressCtrl.text.trim()),
                const Divider(height: 24),
                _reviewRow('Amount',
                    formatAmount(amount, showTicker: true)),
                _reviewRow('Fee',
                    formatAmount(_preparedFee, showTicker: true)),
                const Divider(height: 24),
                _reviewRow('Total Deducted',
                    formatAmount(total, showTicker: true)),
                if (_pidCtrl.text.trim().isNotEmpty) ...[
                  const Divider(height: 24),
                  _reviewRow('Payment ID', _pidCtrl.text.trim()),
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
                  'Transactions are irreversible. Please verify the details.',
                  style: Theme.of(context)
                      .textTheme
                      .bodySmall
                      ?.copyWith(color: kWarning),
                ),
              ),
            ],
          ),
        ),

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
                onPressed: () => setState(() {
                  _step = _TransferStep.form;
                  _error = null;
                }),
                child: const Text('Back'),
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
                    : const Text('Confirm & Send'),
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
            Text('Transaction Sent!',
                style: Theme.of(context).textTheme.headlineMedium),
            const SizedBox(height: 20),
            Text('Transaction Hash',
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
                  onPressed: () => hapticLight(),
                ),
              ],
            ),
            const SizedBox(height: 32),
            OutlinedButton(
              onPressed: _reset,
              child: const Text('Send Another'),
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
    return Scaffold(
      appBar: AppBar(title: const Text('Scan QR Code')),
      body: MobileScanner(
        controller: _controller,
        onDetect: (capture) {
          if (_scanned) return;
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
