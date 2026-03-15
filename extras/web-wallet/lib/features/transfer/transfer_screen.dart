import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../core/api/models/transaction.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_web.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
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
    // In single-threaded WASM, PoW runs synchronously inside the worker —
    // no async poll can update the label during computation. Show a static
    // "Computing PoW…" message immediately so the user knows what is happening.
    final tr = S.of(context);
    setState(() => _powLabel = tr?.computingPow(0) ?? 'Computing PoW…');
    _powTimer?.cancel();
    _powTimer = null;
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

  /// Prepare a transaction (no broadcast). Uses sendAdvanced so we get
  /// the fee back for the review screen.
  Future<void> _prepare() async {
    final tr = S.of(context);
    final dest = _addressCtrl.text.trim();
    final amountRaw = _amountCtrl.text.trim();
    if (dest.isEmpty) { setState(() => _error = tr?.enterDestinationAddress ?? 'Enter a destination address'); return; }
    final atomic = parseAmount(amountRaw);
    if (atomic == null || atomic <= 0) { setState(() => _error = tr?.enterValidAmount ?? 'Enter a valid amount'); return; }

    setState(() { _loading = true; _error = null; });
    _startPowPolling();
    try {
      final ffi = ref.read(walletCApiProvider);
      final paymentId = _paymentIdCtrl.text.trim();
      // Use a fixed fee >= TRANSACTION_POW_PASS_WITH_FEE (10000 atomic = 100 WRKZ)
      // to bypass the extremely slow tx PoW in single-threaded WASM.
      final requestJson = jsonEncode({
        'destinations': [{'address': dest, 'amount': atomic}],
        'fee': 10000,
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
    final tr = S.of(context);
    final dest = _addressCtrl.text.trim();
    if (dest.isEmpty) { setState(() => _error = tr?.enterDestinationAddress ?? 'Enter a destination address'); return; }
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
          first['errorMessage'] as String? ?? (tr?.sweepFailed ?? 'Sweep failed'),
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
    final tr = S.of(context);
    return SingleChildScrollView(
      padding: EdgeInsets.all(MediaQuery.sizeOf(context).width < 600 ? 16 : 28),
      child: Center(
        child: SizedBox(
          width: MediaQuery.sizeOf(context).width < 600 ? double.infinity : 560,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(tr?.transfer ?? 'Transfer', style: Theme.of(context).textTheme.headlineMedium),
                        const SizedBox(height: 4),
                        Text(
                          _sweepMode
                              ? (tr?.sweepAllDescription ?? 'Send all funds to an address (consolidates UTXOs)')
                              : (tr?.sendWrkzToAny ?? 'Send WRKZ to any address'),
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      ],
                    ),
                  ),
                  if (_step == _TransferStep.form) ...[
                    Text(tr?.sweepAll ?? 'Sweep all', style: const TextStyle(fontSize: 13, color: kTextSecondary)),
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
                _StepIndicator(current: _step, tr: tr),
                const SizedBox(height: 24),
              ],

              if (_step == _TransferStep.form) (_sweepMode ? _buildSweepForm(tr) : _buildForm(tr)),
              if (_step == _TransferStep.review) _buildReview(tr),
              if (_step == _TransferStep.success) _buildSuccess(tr),

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

  Widget _buildSweepForm(S? tr) {
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
              child: Row(
                children: [
                  const Icon(Icons.info_outline, color: kWarning, size: 16),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      tr?.sweepWarning ?? 'Sweep consolidates all UTXOs into one output. Use this when transactions fail due to too many inputs.',
                      style: const TextStyle(color: kWarning, fontSize: 12),
                    ),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
            if (balance != null) ...[
              Text(
                tr?.sweepAvailableBalance(formatAmount(balance), kCoinTicker) ?? 'Available: ${formatAmount(balance)} $kCoinTicker (entire balance will be sent minus fee)',
                style: const TextStyle(color: kTextSecondary, fontSize: 12),
              ),
              const SizedBox(height: 14),
            ],
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _addressCtrl,
                    decoration: InputDecoration(labelText: tr?.destinationAddress ?? 'Destination address'),
                  ),
                ),
                if (bookEntries.isNotEmpty) ...[
                  const SizedBox(width: 8),
                  IconButton(
                    icon: const Icon(Icons.contacts_outlined),
                    tooltip: tr?.addressBook ?? 'Address book',
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
                    : Text(tr?.sweepAllFunds ?? 'Sweep All Funds'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildForm(S? tr) {
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
                    decoration: InputDecoration(labelText: tr?.recipientAddress ?? 'Recipient address'),
                  ),
                ),
                if (bookEntries.isNotEmpty) ...[
                  const SizedBox(width: 8),
                  IconButton(
                    icon: const Icon(Icons.contacts_outlined),
                    tooltip: tr?.addressBook ?? 'Address book',
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
                labelText: tr?.amount ?? 'Amount',
                suffixText: kCoinTicker,
                helperText: '${tr?.available ?? 'Available'}: ${ref.watch(balanceProvider).whenOrNull(data: (b) => formatAmount(b.unlocked)) ?? '\u2026'} $kCoinTicker',
              ),
            ),
            const SizedBox(height: 14),
            TextField(
              controller: _paymentIdCtrl,
              decoration: InputDecoration(
                labelText: tr?.paymentIdOptional ?? 'Payment ID (optional)',
                hintText: tr?.hexCharacters ?? '16 or 64 hex characters',
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
                    : Text(tr?.reviewTransaction ?? 'Review Transaction'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildReview(S? tr) {
    final p = _prepared!;
    final dest = _addressCtrl.text.trim();
    final int amount = parseAmount(_amountCtrl.text.trim()) ?? 0;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(tr?.reviewAndConfirm ?? 'Review & Confirm', style: Theme.of(context).textTheme.headlineSmall),
            const SizedBox(height: 20),
            _ReviewRow(label: tr?.to ?? 'To', value: dest, monospace: true),
            _ReviewRow(label: tr?.amount ?? 'Amount', value: '${formatAmount(amount)} $kCoinTicker'),
            _ReviewRow(label: tr?.fee ?? 'Fee', value: '${formatAmount(p.fee)} $kCoinTicker'),
            _ReviewRow(
              label: tr?.totalDeducted ?? 'Total deducted',
              value: '${formatAmount(amount + p.fee)} $kCoinTicker',
              bold: true,
            ),
            if (_paymentIdCtrl.text.trim().isNotEmpty)
              _ReviewRow(label: tr?.paymentId ?? 'Payment ID', value: _paymentIdCtrl.text.trim(), monospace: true),
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
              child: Row(
                children: [
                  const Icon(Icons.warning_amber_outlined, color: kWarning, size: 16),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      tr?.transactionsIrreversible ?? 'Transactions are irreversible. Verify the address before confirming.',
                      style: const TextStyle(color: kWarning, fontSize: 12),
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
                    child: Text(tr?.back ?? 'Back'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  flex: 2,
                  child: FilledButton(
                    onPressed: _loading ? null : _send,
                    child: _loading
                        ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
                        : Text(tr?.confirmAndSend ?? 'Confirm & Send'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSuccess(S? tr) {
    final s = _sent!;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          children: [
            const Icon(Icons.check_circle_outline, color: kSuccess, size: 56),
            const SizedBox(height: 16),
            Text(tr?.transactionSent ?? 'Transaction Sent!', style: Theme.of(context).textTheme.headlineSmall),
            const SizedBox(height: 8),
            Text(tr?.transactionBroadcast ?? 'Your transaction has been broadcast to the network.', style: Theme.of(context).textTheme.bodyMedium, textAlign: TextAlign.center),
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
                        Text(tr?.transactionHash ?? 'Transaction Hash', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11)),
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
            FilledButton(onPressed: _reset, child: Text(tr?.sendAnother ?? 'Send Another')),
          ],
        ),
      ),
    );
  }

  void _showAddressBook(List<AddressBookEntry> entries) {
    final tr = S.of(context);
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr?.addressBookTitle ?? 'Address Book'),
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
        actions: [TextButton(onPressed: () => Navigator.of(ctx).pop(), child: Text(tr?.cancel ?? 'Cancel'))],
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
  final S? tr;
  const _StepIndicator({required this.current, required this.tr});

  @override
  Widget build(BuildContext context) {
    final steps = [
      tr?.stepFillDetails ?? 'Fill Details',
      tr?.stepReview ?? 'Review',
      tr?.stepDone ?? 'Done',
    ];
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
