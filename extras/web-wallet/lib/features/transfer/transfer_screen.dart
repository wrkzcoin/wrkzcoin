import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';
import '../../core/api/models/transaction.dart';
import '../../core/config/app_config.dart';
import '../../core/ffi/wallet_web.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/address_validator.dart';
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
  FeeMode _feeMode = FeeMode.fast;

  final _formKey = GlobalKey<FormState>();
  final _addressCtrl = TextEditingController();
  final _amountCtrl = TextEditingController();
  final _paymentIdCtrl = TextEditingController();

  bool _loading = false;
  String? _error;
  SendResult? _prepared;
  SendResult? _sent;

  /// Exact values the prepared transaction was built from.
  ///
  /// The review screen renders these rather than re-reading the text fields,
  /// so what the user confirms is provably what was prepared.
  String? _preparedAddress;
  int? _preparedAmount;
  String? _preparedPaymentId;

  /// Fingerprint of the inputs behind [_prepared].
  ///
  /// `wallet_capi` exposes no way to release a prepared transaction, so each
  /// prepare that is abandoned keeps its inputs locked. Reusing the existing
  /// one when nothing changed keeps "Back, then Send anyway" from stacking up
  /// prepared transactions against the same UTXOs.
  String? _preparedFingerprint;

  String? _powLabel;

  @override
  void dispose() {
    _addressCtrl.dispose();
    _amountCtrl.dispose();
    _paymentIdCtrl.dispose();
    super.dispose();
  }

  // ── Validation ──────────────────────────────────────────────────────────────

  String? _validateAddress(S? tr, String? value) {
    switch (validateAddress(value ?? '')) {
      case null:
        return null;
      case AddressError.empty:
        return tr?.enterDestinationAddress ?? 'Enter a destination address';
      case AddressError.badPrefix:
        return tr?.addressWrongPrefix ??
            'A WRKZ address starts with "$kAddressPrefix"';
      case AddressError.badLength:
        return tr?.addressWrongLength ??
            'Wrong length — a WRKZ address is $kStandardAddressLength characters '
                '($kIntegratedAddressLength or $kIntegratedAddressLengthLong if integrated)';
      case AddressError.badCharacters:
        return tr?.addressBadCharacters ??
            'Contains characters that are not valid in an address (0, O, I and l are never used)';
    }
  }

  String? _validateAmount(S? tr, String? value) {
    final result = parseAmountChecked(value ?? '');
    if (result.error != null) {
      return switch (result.error!) {
        AmountParseError.empty => tr?.enterValidAmount ?? 'Enter a valid amount',
        AmountParseError.invalidCharacters =>
          tr?.enterValidAmount ?? 'Enter a valid amount',
        AmountParseError.negative =>
          tr?.amountMustBePositive ?? 'Amount must be greater than zero',
        AmountParseError.tooManyDecimals => tr?.amountTooManyDecimals(kCoinDecimalPlaces) ??
            'At most $kCoinDecimalPlaces decimal places',
        AmountParseError.tooLarge => tr?.amountTooLarge ?? 'Amount is too large',
      };
    }
    final atomic = result.atomic!;
    if (atomic <= 0) {
      return tr?.amountMustBePositive ?? 'Amount must be greater than zero';
    }

    // Check it against the spendable balance *before* spending seconds in the
    // WASM module only to be told there are insufficient funds.
    final unlocked = ref.read(balanceProvider).valueOrNull?.unlocked;
    if (unlocked != null) {
      final fee = _feeMode == FeeMode.fast ? kPowExemptFee : 0;
      if (atomic + fee > unlocked) {
        return tr?.amountExceedsBalance(formatAmount(unlocked), kCoinTicker) ??
            'More than your available balance (${formatAmount(unlocked)} $kCoinTicker, plus fee)';
      }
    }
    return null;
  }

  String? _validatePaymentId(S? tr, String? value) {
    if (isValidPaymentId(value ?? '')) return null;
    return tr?.paymentIdInvalidError ??
        'Payment ID must be 16 or 64 hex characters';
  }

  // ── Actions ─────────────────────────────────────────────────────────────────

  void _clearError() {
    if (_error != null) setState(() => _error = null);
  }

  String _fingerprint(String dest, int amount, String paymentId) =>
      '$dest|$amount|$paymentId|${_feeMode.name}';

  /// Prepare a transaction (no broadcast). Uses sendAdvanced so we get
  /// the fee back for the review screen.
  Future<void> _prepare() async {
    final tr = S.of(context);
    if (!(_formKey.currentState?.validate() ?? false)) return;

    final dest = _addressCtrl.text.trim();
    final amount = parseAmount(_amountCtrl.text.trim())!;
    final paymentId = _paymentIdCtrl.text.trim();
    final fingerprint = _fingerprint(dest, amount, paymentId);

    // Nothing changed since the last prepare — reuse it instead of locking a
    // second set of inputs against the same spend.
    if (_prepared != null && _preparedFingerprint == fingerprint) {
      setState(() => _step = _TransferStep.review);
      return;
    }

    // An integrated address already carries its payment ID; supplying a second
    // one is rejected deep inside the library with an opaque message.
    if (paymentId.isNotEmpty && isIntegratedAddress(dest)) {
      setState(() => _error = tr?.paymentIdWithIntegrated ??
          'This address already contains a payment ID — leave the payment ID field empty.');
      return;
    }

    setState(() {
      _loading = true;
      _error = null;
      _powLabel = _feeMode == FeeMode.economy
          ? (tr?.computingPow(0) ?? 'Computing proof of work — this can take several minutes…')
          : null;
    });

    try {
      final ffi = ref.read(walletCApiProvider);
      final requestJson = jsonEncode({
        'destinations': [{'address': dest, 'amount': amount}],
        // Fast mode pays the PoW-exemption fee; economy omits `fee` entirely so
        // the library charges the network minimum and computes the PoW.
        if (_feeMode == FeeMode.fast) 'fee': kPowExemptFee,
        if (paymentId.isNotEmpty) 'paymentID': paymentId,
      });
      final result = await ffi.sendAdvanced(requestJson, broadcast: false);
      if (!mounted) return;
      setState(() {
        _prepared = SendResult.fromJson(result);
        _preparedAddress = dest;
        _preparedAmount = amount;
        _preparedPaymentId = paymentId;
        _preparedFingerprint = fingerprint;
        _step = _TransferStep.review;
      });
    } on WalletCApiException catch (e) {
      if (mounted) setState(() => _error = e.message);
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() { _loading = false; _powLabel = null; });
    }
  }

  /// Broadcast the previously prepared transaction.
  Future<void> _send() async {
    if (_prepared == null) return;
    setState(() { _loading = true; _error = null; });
    try {
      final ffi = ref.read(walletCApiProvider);
      final txHash = await ffi.sendPrepared(_prepared!.transactionHash);
      // The bridge persists to IndexedDB as part of the send, so a tab closed
      // right after this cannot resurrect the spent outputs.
      await Future.wait([
        ref.read(balanceProvider.notifier).refresh(),
        ref.read(transactionsProvider.notifier).refresh(),
      ]);
      if (!mounted) return;
      setState(() {
        _sent = SendResult(
          transactionHash: txHash,
          fee: _prepared!.fee,
          relayedToNetwork: true,
        );
        _prepared = null;
        _preparedFingerprint = null;
        _step = _TransferStep.success;
      });
    } on WalletCApiException catch (e) {
      if (mounted) setState(() => _error = e.message);
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _loading = false);
    }
  }

  /// Sweep all funds to an address.
  Future<void> _sweep() async {
    final tr = S.of(context);
    final dest = _addressCtrl.text.trim();
    final addrError = _validateAddress(tr, dest);
    if (addrError != null) {
      setState(() => _error = addrError);
      return;
    }

    final confirmed = await _confirmSweep(tr, dest);
    if (confirmed != true || !mounted) return;

    setState(() {
      _loading = true;
      _error = null;
      _powLabel = tr?.computingPow(0) ?? 'Preparing transactions…';
    });
    try {
      final ffi = ref.read(walletCApiProvider);
      final result = await ffi.sweepToAddress(dest);
      final results = (result['results'] as List<dynamic>?) ?? const [];
      final successes = results
          .whereType<Map>()
          .where((r) => r.containsKey('txHash'))
          .toList();
      if (successes.isEmpty) {
        // `results.first` on an empty list threw "Bad state: No element",
        // which surfaced to the user instead of the real failure.
        final first = results.whereType<Map>().firstOrNull;
        throw WalletCApiException(
          (first?['error'] as num?)?.toInt() ?? -1,
          (first?['errorMessage'] as String?) ?? (tr?.sweepFailed ?? 'Sweep failed'),
        );
      }
      final hashes = successes.map((r) => r['txHash'] as String).join(', ');
      await Future.wait([
        ref.read(balanceProvider.notifier).refresh(),
        ref.read(transactionsProvider.notifier).refresh(),
      ]);
      if (!mounted) return;
      setState(() {
        _sent = SendResult(transactionHash: hashes, fee: 0, relayedToNetwork: true);
        _step = _TransferStep.success;
      });
    } on WalletCApiException catch (e) {
      if (mounted) setState(() => _error = e.message);
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() { _loading = false; _powLabel = null; });
    }
  }

  Future<bool?> _confirmSweep(S? tr, String dest) {
    return showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr?.sweepAllFunds ?? 'Sweep All Funds'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(tr?.sweepConfirmBody ??
                'This sends your entire balance, minus fees, to:'),
            const SizedBox(height: 12),
            _AddressBlock(address: dest),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(tr?.cancel ?? 'Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(tr?.confirmAndSend ?? 'Confirm & Send'),
          ),
        ],
      ),
    );
  }

  void _setMaxAmount() {
    final unlocked = ref.read(balanceProvider).valueOrNull?.unlocked;
    if (unlocked == null) return;
    final fee = _feeMode == FeeMode.fast ? kPowExemptFee : 0;
    final spendable = unlocked - fee;
    _amountCtrl.text = formatAmountPlain(spendable > 0 ? spendable : 0);
    _formKey.currentState?.validate();
  }

  /// Go back from review to form. The prepared transaction is kept so an
  /// unchanged re-submit reuses it.
  void _backToForm() {
    setState(() { _step = _TransferStep.form; _error = null; });
  }

  void _reset() {
    _addressCtrl.clear();
    _amountCtrl.clear();
    _paymentIdCtrl.clear();
    setState(() {
      _step = _TransferStep.form;
      _prepared = null;
      _preparedFingerprint = null;
      _preparedAddress = null;
      _preparedAmount = null;
      _preparedPaymentId = null;
      _sent = null;
      _error = null;
    });
  }

  Future<void> _openInExplorer(String hash) async {
    final url = Uri.parse(kExplorerTxUrl.replaceAll('{hash}', hash));
    if (!await launchUrl(url, mode: LaunchMode.externalApplication)) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('$url')),
      );
    }
  }

  // ── Build ───────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final narrow = MediaQuery.sizeOf(context).width < 600;

    return SingleChildScrollView(
      padding: EdgeInsets.all(narrow ? 16 : 28),
      child: Center(
        child: SizedBox(
          width: narrow ? double.infinity : 560,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _buildHeader(tr, narrow),
              const SizedBox(height: 24),
              if (!(_sweepMode && _step == _TransferStep.form)) ...[
                _StepIndicator(current: _step, tr: tr, narrow: narrow),
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

  Widget _buildHeader(S? tr, bool narrow) {
    final title = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(tr?.transfer ?? 'Transfer',
            style: Theme.of(context).textTheme.headlineMedium),
        const SizedBox(height: 4),
        Text(
          _sweepMode
              ? (tr?.sweepAllDescription ?? 'Send all funds to an address (consolidates UTXOs)')
              : (tr?.sendWrkzToAny ?? 'Send WRKZ to any address'),
          style: Theme.of(context).textTheme.bodyMedium,
        ),
      ],
    );

    if (_step != _TransferStep.form) return title;

    final toggle = Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(tr?.sweepAll ?? 'Sweep all',
            style: TextStyle(fontSize: 13, color: context.textSecondary)),
        const SizedBox(width: 6),
        Switch(
          value: _sweepMode,
          onChanged: (v) => setState(() { _sweepMode = v; _error = null; }),
        ),
      ],
    );

    // Stack on narrow screens — side by side, the title and the switch
    // overflowed below about 400px.
    if (narrow) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [title, const SizedBox(height: 8), toggle],
      );
    }
    return Row(children: [Expanded(child: title), toggle]);
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
            _Callout(
              icon: Icons.info_outline,
              color: kWarning,
              text: tr?.sweepWarning ??
                  'Sweep consolidates all UTXOs into one output. Use this when transactions fail due to too many inputs.',
            ),
            const SizedBox(height: 16),
            if (balance != null) ...[
              Text(
                tr?.sweepAvailableBalance(formatAmount(balance), kCoinTicker) ??
                    'Available: ${formatAmount(balance)} $kCoinTicker (entire balance will be sent minus fee)',
                style: TextStyle(color: context.textSecondary, fontSize: 12),
              ),
              const SizedBox(height: 14),
            ],
            _AddressField(
              controller: _addressCtrl,
              label: tr?.destinationAddress ?? 'Destination address',
              entries: bookEntries,
              onPickFromBook: _showAddressBook,
              onChanged: (_) => _clearError(),
            ),
            const SizedBox(height: 20),
            SizedBox(
              width: double.infinity,
              child: FilledButton(
                onPressed: _loading ? null : _sweep,
                child: _buildButtonChild(tr?.sweepAllFunds ?? 'Sweep All Funds'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildForm(S? tr) {
    final bookEntries = ref.watch(addressBookProvider);
    final balance = ref.watch(balanceProvider).whenOrNull(data: (b) => b.unlocked);

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Form(
          key: _formKey,
          autovalidateMode: AutovalidateMode.onUserInteraction,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _AddressField(
                controller: _addressCtrl,
                label: tr?.recipientAddress ?? 'Recipient address',
                entries: bookEntries,
                onPickFromBook: _showAddressBook,
                validator: (v) => _validateAddress(tr, v),
                onChanged: (_) => _clearError(),
              ),
              const SizedBox(height: 14),
              TextFormField(
                controller: _amountCtrl,
                keyboardType: const TextInputType.numberWithOptions(decimal: true),
                // Keep the field to digits and separators so a stray letter can
                // never reach the parser.
                inputFormatters: [
                  FilteringTextInputFormatter.allow(RegExp(r'[0-9., ]')),
                ],
                validator: (v) => _validateAmount(tr, v),
                onChanged: (_) => _clearError(),
                decoration: InputDecoration(
                  labelText: tr?.amount ?? 'Amount',
                  suffixText: kCoinTicker,
                  helperText:
                      '${tr?.available ?? 'Available'}: ${balance == null ? '…' : formatAmount(balance)} $kCoinTicker',
                  prefixIcon: TextButton(
                    onPressed: balance == null ? null : _setMaxAmount,
                    style: TextButton.styleFrom(
                      minimumSize: const Size(48, 36),
                      padding: const EdgeInsets.symmetric(horizontal: 8),
                    ),
                    child: Text(tr?.max ?? 'MAX',
                        style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w700)),
                  ),
                  prefixIconConstraints: const BoxConstraints(minWidth: 56),
                ),
              ),
              const SizedBox(height: 14),
              TextFormField(
                controller: _paymentIdCtrl,
                validator: (v) => _validatePaymentId(tr, v),
                onChanged: (_) => _clearError(),
                inputFormatters: [
                  FilteringTextInputFormatter.allow(RegExp(r'[0-9a-fA-F]')),
                  LengthLimitingTextInputFormatter(64),
                ],
                decoration: InputDecoration(
                  labelText: tr?.paymentIdOptional ?? 'Payment ID (optional)',
                  hintText: tr?.hexCharacters ?? '16 or 64 hex characters',
                ),
              ),
              const SizedBox(height: 18),
              _FeeSelector(
                value: _feeMode,
                tr: tr,
                onChanged: (mode) {
                  setState(() { _feeMode = mode; _error = null; });
                  _formKey.currentState?.validate();
                },
              ),
              const SizedBox(height: 20),
              SizedBox(
                width: double.infinity,
                child: FilledButton(
                  onPressed: _loading ? null : _prepare,
                  child: _buildButtonChild(tr?.reviewTransaction ?? 'Review Transaction'),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildButtonChild(String label) {
    if (!_loading) return Text(label);
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        const SizedBox(
            width: 18, height: 18,
            child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2)),
        if (_powLabel != null) ...[
          const SizedBox(width: 10),
          Flexible(
            child: Text(_powLabel!,
                style: const TextStyle(fontSize: 12),
                overflow: TextOverflow.ellipsis),
          ),
        ],
      ],
    );
  }

  Widget _buildReview(S? tr) {
    final p = _prepared!;
    // Rendered from the snapshot taken at prepare time, not from the live text
    // fields — the review must describe the transaction that was actually built.
    final dest = _preparedAddress ?? '';
    final amount = _preparedAmount ?? 0;
    final paymentId = _preparedPaymentId ?? '';

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(tr?.reviewAndConfirm ?? 'Review & Confirm',
                style: Theme.of(context).textTheme.headlineSmall),
            const SizedBox(height: 20),
            Text(tr?.to ?? 'To',
                style: TextStyle(color: context.textSecondary, fontSize: 13)),
            const SizedBox(height: 6),
            _AddressBlock(address: dest),
            const SizedBox(height: 16),
            _ReviewRow(label: tr?.amount ?? 'Amount', value: '${formatAmount(amount)} $kCoinTicker'),
            _ReviewRow(label: tr?.fee ?? 'Fee', value: '${formatAmount(p.fee)} $kCoinTicker'),
            _ReviewRow(
              label: tr?.totalDeducted ?? 'Total deducted',
              value: '${formatAmount(amount + p.fee)} $kCoinTicker',
              bold: true,
            ),
            if (paymentId.isNotEmpty)
              _ReviewRow(label: tr?.paymentId ?? 'Payment ID', value: paymentId, monospace: true),
            const SizedBox(height: 8),
            const Divider(),
            const SizedBox(height: 12),
            _Callout(
              icon: Icons.warning_amber_outlined,
              color: kWarning,
              text: tr?.transactionsIrreversible ??
                  'Transactions are irreversible. Verify the address before confirming.',
            ),
            const SizedBox(height: 20),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton(
                    onPressed: _loading ? null : _backToForm,
                    child: Text(tr?.back ?? 'Back'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  flex: 2,
                  child: FilledButton(
                    onPressed: _loading ? null : _send,
                    child: _loading
                        ? const SizedBox(width: 18, height: 18,
                            child: CircularProgressIndicator(color: Colors.white, strokeWidth: 2))
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
    final singleHash = !s.transactionHash.contains(',');
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          children: [
            const Icon(Icons.check_circle_outline, color: kSuccess, size: 56),
            const SizedBox(height: 16),
            Text(tr?.transactionSent ?? 'Transaction Sent!',
                style: Theme.of(context).textTheme.headlineSmall),
            const SizedBox(height: 8),
            Text(tr?.transactionBroadcast ?? 'Your transaction has been broadcast to the network.',
                style: Theme.of(context).textTheme.bodyMedium, textAlign: TextAlign.center),
            const SizedBox(height: 20),
            Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: Theme.of(context).colorScheme.surfaceContainerHighest,
                borderRadius: BorderRadius.circular(8),
              ),
              child: Row(
                children: [
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(tr?.transactionHash ?? 'Transaction Hash',
                            style: TextStyle(color: context.textSecondary, fontSize: 11)),
                        const SizedBox(height: 4),
                        SelectableText(s.transactionHash,
                            style: TextStyle(
                                fontSize: 11,
                                fontFamily: 'monospace',
                                color: Theme.of(context).colorScheme.onSurface)),
                      ],
                    ),
                  ),
                  CopyButton(text: s.transactionHash),
                ],
              ),
            ),
            const SizedBox(height: 20),
            Wrap(
              spacing: 12,
              runSpacing: 8,
              alignment: WrapAlignment.center,
              children: [
                if (singleHash)
                  OutlinedButton.icon(
                    icon: const Icon(Icons.open_in_new, size: 16),
                    label: Text(tr?.viewInExplorer ?? 'View in explorer'),
                    onPressed: () => _openInExplorer(s.transactionHash),
                  ),
                FilledButton(
                  onPressed: _reset,
                  child: Text(tr?.sendAnother ?? 'Send Another'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  void _showAddressBook(List<AddressBookEntry> entries) {
    final tr = S.of(context);
    showDialog<void>(
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
                subtitle: Text(shortenAddress(e.address, head: 16, tail: 10),
                    style: const TextStyle(fontSize: 11, fontFamily: 'monospace')),
                onTap: () {
                  _addressCtrl.text = e.address;
                  _formKey.currentState?.validate();
                  Navigator.of(ctx).pop();
                },
              );
            },
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: Text(tr?.cancel ?? 'Cancel'),
          ),
        ],
      ),
    );
  }
}

// ── Local widgets ─────────────────────────────────────────────────────────────

/// Address input with an address-book picker. Autocorrect and suggestions are
/// off — browsers otherwise try to autofill and "helpfully" capitalise a
/// case-sensitive base58 string.
class _AddressField extends StatelessWidget {
  final TextEditingController controller;
  final String label;
  final List<AddressBookEntry> entries;
  final void Function(List<AddressBookEntry>) onPickFromBook;
  final String? Function(String?)? validator;
  final ValueChanged<String>? onChanged;

  const _AddressField({
    required this.controller,
    required this.label,
    required this.entries,
    required this.onPickFromBook,
    this.validator,
    this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Expanded(
          child: TextFormField(
            controller: controller,
            validator: validator,
            onChanged: onChanged,
            autocorrect: false,
            enableSuggestions: false,
            textCapitalization: TextCapitalization.none,
            decoration: InputDecoration(labelText: label),
          ),
        ),
        if (entries.isNotEmpty) ...[
          const SizedBox(width: 8),
          Padding(
            padding: const EdgeInsets.only(top: 4),
            child: IconButton(
              icon: const Icon(Icons.contacts_outlined),
              tooltip: tr?.addressBook ?? 'Address book',
              onPressed: () => onPickFromBook(entries),
            ),
          ),
        ],
      ],
    );
  }
}

/// Renders an address in fixed-width groups so it can be compared against
/// another copy without losing your place in 98 unbroken characters.
class _AddressBlock extends StatelessWidget {
  final String address;
  const _AddressBlock({required this.address});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: context.dividerColor),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(
            child: SelectableText(
              chunkAddress(address).join(' '),
              style: TextStyle(
                fontSize: 12,
                height: 1.7,
                letterSpacing: 0.4,
                fontFamily: 'monospace',
                color: Theme.of(context).colorScheme.onSurface,
              ),
            ),
          ),
          CopyButton(text: address, size: 16),
        ],
      ),
    );
  }
}

class _FeeSelector extends StatelessWidget {
  final FeeMode value;
  final ValueChanged<FeeMode> onChanged;
  final S? tr;

  const _FeeSelector({required this.value, required this.onChanged, required this.tr});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(tr?.networkFee ?? 'Network fee',
            style: TextStyle(color: context.textSecondary, fontSize: 13)),
        const SizedBox(height: 8),
        SegmentedButton<FeeMode>(
          segments: [
            ButtonSegment(
              value: FeeMode.fast,
              icon: const Icon(Icons.bolt, size: 15),
              label: Text(tr?.feeFast ?? 'Fast'),
            ),
            ButtonSegment(
              value: FeeMode.economy,
              icon: const Icon(Icons.savings_outlined, size: 15),
              label: Text(tr?.feeEconomy ?? 'Economy'),
            ),
          ],
          selected: {value},
          onSelectionChanged: (s) => onChanged(s.first),
          style: const ButtonStyle(visualDensity: VisualDensity.compact),
        ),
        const SizedBox(height: 6),
        Text(
          value == FeeMode.fast
              ? (tr?.feeFastHint(formatAmount(kPowExemptFee), kCoinTicker) ??
                  'Pays ${formatAmount(kPowExemptFee)} $kCoinTicker to skip the transaction proof of work. Sends in seconds.')
              : (tr?.feeEconomyHint ??
                  'Pays the network minimum, but your browser must compute the transaction proof of work — this can take several minutes.'),
          style: TextStyle(color: context.textSecondary, fontSize: 11, height: 1.4),
        ),
      ],
    );
  }
}

class _Callout extends StatelessWidget {
  final IconData icon;
  final Color color;
  final String text;
  const _Callout({required this.icon, required this.color, required this.text});

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: color.withAlpha(20),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: color.withAlpha(80)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: color, size: 16),
          const SizedBox(width: 8),
          Expanded(
            child: Text(text, style: TextStyle(color: color, fontSize: 12, height: 1.4)),
          ),
        ],
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
          SizedBox(
            width: 120,
            child: Text(label, style: TextStyle(color: context.textSecondary, fontSize: 13)),
          ),
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
  final bool narrow;
  const _StepIndicator({required this.current, required this.tr, this.narrow = false});

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
              if (i > 0)
                Expanded(
                  child: Container(height: 1, color: done ? kPrimary : context.dividerColor),
                ),
              Container(
                width: 24, height: 24,
                decoration: BoxDecoration(
                  color: active || done ? kPrimary : context.surfaceVariantColor,
                  shape: BoxShape.circle,
                  border: Border.all(color: active || done ? kPrimary : context.dividerColor),
                ),
                child: Center(
                  child: done
                      ? const Icon(Icons.check, size: 12, color: Colors.white)
                      : Text('${i + 1}',
                          style: TextStyle(
                              fontSize: 11,
                              color: active ? Colors.white : context.textDisabled)),
                ),
              ),
              // Labels are dropped on narrow screens; unconstrained Text inside
              // a Row overflowed below ~380px.
              if (!narrow) ...[
                const SizedBox(width: 6),
                Flexible(
                  child: Text(
                    steps[i],
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                        fontSize: 11, color: active ? kPrimary : context.textSecondary),
                  ),
                ),
              ],
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
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Icon(Icons.error_outline, color: kError, size: 16),
          const SizedBox(width: 8),
          Expanded(child: SelectableText(message, style: const TextStyle(color: kError, fontSize: 13))),
        ],
      ),
    );
  }
}
