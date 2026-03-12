import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../core/api/models/transaction.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_ffi.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/widgets/copy_button.dart';
import '../addressbook/address_book_provider.dart';

enum _TransferStep { form, review, success }

class TransferScreen extends ConsumerStatefulWidget {
  const TransferScreen({super.key});

  @override
  ConsumerState<TransferScreen> createState() => _TransferScreenState();
}

class _TransferScreenState extends ConsumerState<TransferScreen> {
  _TransferStep _step = _TransferStep.form;
  bool _sweepMode = false;

  final _addressCtrl = TextEditingController();
  final _amountCtrl = TextEditingController();
  final _paymentIdCtrl = TextEditingController();

  bool _loading = false;
  String? _error;
  SendResult? _prepared;
  SendResult? _sent;

  // PoW progress polling
  Timer? _powTimer;
  String? _powLabel;

  void _startPowPolling() {
    _powLabel = null;
    _powTimer?.cancel();
    _powTimer = Timer.periodic(const Duration(milliseconds: 500), (_) {
      if (!mounted || !_loading) { _stopPowPolling(); return; }
      final ffi = ref.read(walletCApiProvider);
      final (:active, :elapsedMs, nonces: _) = ffi.getPowStatus();
      if (active) {
        final sec = (elapsedMs / 1000).round();
        setState(() => _powLabel = 'Computing PoW... ${sec}s');
      }
    });
  }

  void _stopPowPolling() {
    _powTimer?.cancel();
    _powTimer = null;
    if (mounted && _powLabel != null) setState(() => _powLabel = null);
  }

  @override
  void dispose() {
    _powTimer?.cancel();
    _addressCtrl.dispose();
    _amountCtrl.dispose();
    _paymentIdCtrl.dispose();
    super.dispose();
  }

  /// Prepare a transaction via FFI (no broadcast). Uses sendAdvanced so we get
  /// the fee back for the review screen.
  Future<void> _prepare() async {
    final dest = _addressCtrl.text.trim();
    final amountRaw = _amountCtrl.text.trim();
    if (dest.isEmpty) { setState(() => _error = 'Enter a destination address'); return; }
    final atomic = parseAmount(amountRaw);
    if (atomic == null || atomic <= 0) { setState(() => _error = 'Enter a valid amount'); return; }

    setState(() { _loading = true; _error = null; });
    _startPowPolling();
    try {
      final ffi = ref.read(walletCApiProvider);
      final paymentId = _paymentIdCtrl.text.trim();
      final requestJson = jsonEncode({
        'destinations': [{'address': dest, 'amount': atomic}],
        if (paymentId.isNotEmpty) 'paymentID': paymentId,
      });
      final result = await ffi.sendAdvanced(requestJson, broadcast: false);
      setState(() { _prepared = SendResult.fromJson(result); _step = _TransferStep.review; });
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      _stopPowPolling();
      if (mounted) setState(() => _loading = false);
    }
  }

  /// Broadcast the previously prepared transaction.
  Future<void> _send() async {
    if (_prepared == null) return;
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      final txHash = await ffi.sendPrepared(_prepared!.transactionHash);
      ref.read(balanceProvider.notifier).refresh();
      ref.read(transactionsProvider.notifier).refresh();
      setState(() {
        _sent = SendResult(
          transactionHash: txHash,
          fee: _prepared!.fee,
          relayedToNetwork: true,
        );
        _step = _TransferStep.success;
      });
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  /// Sweep all funds to an address.
  Future<void> _sweep() async {
    final dest = _addressCtrl.text.trim();
    if (dest.isEmpty) { setState(() => _error = 'Enter a destination address'); return; }
    setState(() { _loading = true; _error = null; });
    _startPowPolling();
    try {
      final ffi = ref.read(walletCApiProvider);
      final result = await ffi.sweepToAddress(dest);
      final results = result['results'] as List<dynamic>;
      final successes = results
          .whereType<Map>()
          .where((r) => r.containsKey('txHash'))
          .toList();
      if (successes.isEmpty) {
        final first = results.first as Map<String, dynamic>;
        throw WalletCApiException(
          first['error'] as int? ?? -1,
          first['errorMessage'] as String? ?? 'Sweep failed',
        );
      }
      final hashes = successes.map((r) => r['txHash'] as String).join(', ');
      ref.read(balanceProvider.notifier).refresh();
      ref.read(transactionsProvider.notifier).refresh();
      setState(() {
        _sent = SendResult(transactionHash: hashes, fee: 0, relayedToNetwork: true);
        _step = _TransferStep.success;
      });
    } on WalletCApiException catch (e) {
      setState(() => _error = e.message);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      _stopPowPolling();
      if (mounted) setState(() => _loading = false);
    }
  }

  /// Go back from review to form (prepared tx stays in memory but won't be sent).
  Future<void> _cancelPrepared() async {
    setState(() { _prepared = null; _step = _TransferStep.form; });
  }

  void _reset() {
    _addressCtrl.clear();
    _amountCtrl.clear();
    _paymentIdCtrl.clear();
    setState(() { _step = _TransferStep.form; _prepared = null; _sent = null; _error = null; });
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(28),
      child: Center(
        child: SizedBox(
          width: 560,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Transfer', style: Theme.of(context).textTheme.headlineMedium),
                        const SizedBox(height: 4),
                        Text(
                          _sweepMode
                              ? 'Send all funds to an address (consolidates UTXOs)'
                              : 'Send WRKZ to any address',
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      ],
                    ),
                  ),
                  if (_step == _TransferStep.form) ...[
                    const Text('Sweep all', style: TextStyle(fontSize: 13, color: kTextSecondary)),
                    const SizedBox(width: 6),
                    Switch(
                      value: _sweepMode,
                      onChanged: (v) => setState(() { _sweepMode = v; _error = null; }),
                    ),
                  ],
                ],
              ),
              const SizedBox(height: 24),
              if (!(_sweepMode && _step == _TransferStep.form)) ...[
                _StepIndicator(current: _step),
                const SizedBox(height: 24),
              ],

              if (_step == _TransferStep.form) (_sweepMode ? _buildSweepForm() : _buildForm()),
              if (_step == _TransferStep.review) _buildReview(),
              if (_step == _TransferStep.success) _buildSuccess(),

              if (_error != null) ...[
                const SizedBox(height: 16),
                _ErrorBox(message: _error!),
              ],
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildSweepForm() {
    final bookEntries = ref.watch(addressBookProvider);
    final balance = ref.watch(balanceProvider).whenOrNull(data: (b) => b.unlocked);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: kWarning.withAlpha(20),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: kWarning.withAlpha(80)),
              ),
              child: const Row(
                children: [
                  Icon(Icons.info_outline, color: kWarning, size: 16),
                  SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      'Sweep consolidates all UTXOs into one output. '
                      'Use this when transactions fail due to too many inputs.',
                      style: TextStyle(color: kWarning, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
            if (balance != null) ...[
              Text(
                'Available: ${formatAmount(balance)} $kCoinTicker (entire balance will be sent minus fee)',
                style: const TextStyle(color: kTextSecondary, fontSize: 12),
              ),
              const SizedBox(height: 14),
            ],
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _addressCtrl,
                    decoration: const InputDecoration(labelText: 'Destination address'),
                  ),
                ),
                if (bookEntries.isNotEmpty) ...[
                  const SizedBox(width: 8),
                  IconButton(
                    icon: const Icon(Icons.contacts_outlined),
                    tooltip: 'Address book',
                    onPressed: () => _showAddressBook(bookEntries),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 20),
            SizedBox(
              width: double.infinity,
              child: FilledButton(
                onPressed: _loading ? null : _sweep,
                child: _loading
                    ? Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2)),
                          if (_powLabel != null) ...[
                            const SizedBox(width: 10),
                            Text(_powLabel!, style: const TextStyle(fontSize: 13)),
                          ],
                        ],
                      )
                    : const Text('Sweep All Funds'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildForm() {
    final bookEntries = ref.watch(addressBookProvider);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _addressCtrl,
                    decoration: const InputDecoration(labelText: 'Recipient address'),
                  ),
                ),
                if (bookEntries.isNotEmpty) ...[
                  const SizedBox(width: 8),
                  IconButton(
                    icon: const Icon(Icons.contacts_outlined),
                    tooltip: 'Address book',
                    onPressed: () => _showAddressBook(bookEntries),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 14),
            TextField(
              controller: _amountCtrl,
              keyboardType: const TextInputType.numberWithOptions(decimal: true),
              decoration: InputDecoration(
                labelText: 'Amount',
                suffixText: kCoinTicker,
                helperText: 'Available: ${ref.watch(balanceProvider).whenOrNull(data: (b) => formatAmount(b.unlocked)) ?? '…'} $kCoinTicker',
              ),
            ),
            const SizedBox(height: 14),
            TextField(
              controller: _paymentIdCtrl,
              decoration: const InputDecoration(
                labelText: 'Payment ID (optional)',
                hintText: '16 or 64 hex characters',
              ),
            ),
            const SizedBox(height: 20),
            SizedBox(
              width: double.infinity,
              child: FilledButton(
                onPressed: _loading ? null : _prepare,
                child: _loading
                    ? Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2)),
                          if (_powLabel != null) ...[
                            const SizedBox(width: 10),
                            Text(_powLabel!, style: const TextStyle(fontSize: 13)),
                          ],
                        ],
                      )
                    : const Text('Review Transaction'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildReview() {
    final p = _prepared!;
    final dest = _addressCtrl.text.trim();
    final int amount = parseAmount(_amountCtrl.text.trim()) ?? 0;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Review & Confirm', style: Theme.of(context).textTheme.headlineSmall),
            const SizedBox(height: 20),
            _ReviewRow(label: 'To', value: dest, monospace: true),
            _ReviewRow(label: 'Amount', value: '${formatAmount(amount)} $kCoinTicker'),
            _ReviewRow(label: 'Fee', value: '${formatAmount(p.fee)} $kCoinTicker'),
            _ReviewRow(
              label: 'Total deducted',
              value: '${formatAmount(amount + p.fee)} $kCoinTicker',
              bold: true,
            ),
            if (_paymentIdCtrl.text.trim().isNotEmpty)
              _ReviewRow(label: 'Payment ID', value: _paymentIdCtrl.text.trim(), monospace: true),
            const SizedBox(height: 8),
            const Divider(),
            const SizedBox(height: 12),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: kWarning.withAlpha(20),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: kWarning.withAlpha(80)),
              ),
              child: const Row(
                children: [
                  Icon(Icons.warning_amber_outlined, color: kWarning, size: 16),
                  SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      'Transactions are irreversible. Verify the address before confirming.',
                      style: TextStyle(color: kWarning, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 20),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton(
                    onPressed: _loading ? null : _cancelPrepared,
                    child: const Text('Back'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  flex: 2,
                  child: FilledButton(
                    onPressed: _loading ? null : _send,
                    child: _loading
                        ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
                        : const Text('Confirm & Send'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSuccess() {
    final s = _sent!;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          children: [
            const Icon(Icons.check_circle_outline, color: kSuccess, size: 56),
            const SizedBox(height: 16),
            Text('Transaction Sent!', style: Theme.of(context).textTheme.headlineSmall),
            const SizedBox(height: 8),
            Text('Your transaction has been broadcast to the network.', style: Theme.of(context).textTheme.bodyMedium, textAlign: TextAlign.center),
            const SizedBox(height: 20),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(color: Theme.of(context).colorScheme.surfaceContainerHighest, borderRadius: BorderRadius.circular(8)),
              child: Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('Transaction Hash', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11)),
                        const SizedBox(height: 4),
                        SelectableText(s.transactionHash, style: TextStyle(fontSize: 11, fontFamily: 'monospace', color: Theme.of(context).colorScheme.onSurface)),
                      ],
                    ),
                  ),
                  CopyButton(text: s.transactionHash),
                ],
              ),
            ),
            const SizedBox(height: 24),
            FilledButton(onPressed: _reset, child: const Text('Send Another')),
          ],
        ),
      ),
    );
  }

  void _showAddressBook(List<AddressBookEntry> entries) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Address Book'),
        content: SizedBox(
          width: 400,
          child: ListView.separated(
            shrinkWrap: true,
            itemCount: entries.length,
            separatorBuilder: (_, _) => const Divider(height: 1),
            itemBuilder: (_, i) {
              final e = entries[i];
              return ListTile(
                title: Text(e.name),
                subtitle: Text(e.address, style: const TextStyle(fontSize: 11, fontFamily: 'monospace')),
                onTap: () {
                  _addressCtrl.text = e.address;
                  Navigator.of(ctx).pop();
                },
              );
            },
          ),
        ),
        actions: [TextButton(onPressed: () => Navigator.of(ctx).pop(), child: const Text('Cancel'))],
      ),
    );
  }
}

class _ReviewRow extends StatelessWidget {
  final String label;
  final String value;
  final bool monospace;
  final bool bold;
  const _ReviewRow({required this.label, required this.value, this.monospace = false, this.bold = false});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(width: 120, child: Text(label, style: const TextStyle(color: kTextSecondary, fontSize: 13))),
          Expanded(
            child: Text(
              value,
              style: TextStyle(
                fontSize: 13,
                fontFamily: monospace ? 'monospace' : null,
                fontWeight: bold ? FontWeight.w600 : FontWeight.normal,
                color: Theme.of(context).colorScheme.onSurface,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _StepIndicator extends StatelessWidget {
  final _TransferStep current;
  const _StepIndicator({required this.current});

  @override
  Widget build(BuildContext context) {
    const steps = ['Fill Details', 'Review', 'Done'];
    final idx = _TransferStep.values.indexOf(current);
    return Row(
      children: List.generate(steps.length, (i) {
        final done = i < idx;
        final active = i == idx;
        return Expanded(
          child: Row(
            children: [
              if (i > 0) Expanded(child: Container(height: 1, color: done ? kPrimary : kDivider)),
              Container(
                width: 24, height: 24,
                decoration: BoxDecoration(
                  color: active || done ? kPrimary : kSurfaceVariant,
                  shape: BoxShape.circle,
                  border: Border.all(color: active || done ? kPrimary : kDivider),
                ),
                child: Center(
                  child: done
                      ? const Icon(Icons.check, size: 12, color: Colors.white)
                      : Text('${i + 1}', style: TextStyle(fontSize: 11, color: active ? Colors.white : kTextDisabled)),
                ),
              ),
              const SizedBox(width: 6),
              Text(steps[i], style: TextStyle(fontSize: 11, color: active ? kPrimary : kTextSecondary)),
              if (i < steps.length - 1) const SizedBox(width: 6),
            ],
          ),
        );
      }),
    );
  }
}

class _ErrorBox extends StatelessWidget {
  final String message;
  const _ErrorBox({required this.message});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: kError.withAlpha(25),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: kError.withAlpha(80)),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: kError, size: 16),
          const SizedBox(width: 8),
          Expanded(child: Text(message, style: const TextStyle(color: kError, fontSize: 13))),
        ],
      ),
    );
  }
}
