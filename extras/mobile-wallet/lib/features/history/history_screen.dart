import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/api/models/transaction.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/copy_button.dart';

enum _TxFilter { all, incoming, outgoing }

class HistoryScreen extends ConsumerStatefulWidget {
  const HistoryScreen({super.key});

  @override
  ConsumerState<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends ConsumerState<HistoryScreen> {
  _TxFilter _filter = _TxFilter.all;
  final _searchCtrl = TextEditingController();
  String _search = '';

  @override
  void dispose() {
    _searchCtrl.dispose();
    super.dispose();
  }

  List<Transaction> _applyFilters(List<Transaction> txs) {
    var filtered = txs;

    // Direction filter
    switch (_filter) {
      case _TxFilter.incoming:
        filtered = filtered.where((t) => t.isIncoming).toList();
        break;
      case _TxFilter.outgoing:
        filtered = filtered.where((t) => !t.isIncoming).toList();
        break;
      case _TxFilter.all:
        break;
    }

    // Search
    if (_search.isNotEmpty) {
      final q = _search.toLowerCase();
      filtered = filtered.where((t) {
        return t.hash.toLowerCase().contains(q) ||
            t.address.toLowerCase().contains(q) ||
            t.paymentID.toLowerCase().contains(q);
      }).toList();
    }

    return filtered;
  }

  @override
  Widget build(BuildContext context) {
    final txAsync = ref.watch(transactionsProvider);

    return RefreshIndicator(
      onRefresh: () async {
        await ref.read(transactionsProvider.notifier).refresh();
        hapticMedium();
      },
      child: Column(
        children: [
          // Search & filters
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
            child: TextField(
              controller: _searchCtrl,
              decoration: InputDecoration(
                hintText: 'Search by hash, address, payment ID...',
                prefixIcon: const Icon(Icons.search, size: 20),
                suffixIcon: _search.isNotEmpty
                    ? IconButton(
                        icon: const Icon(Icons.clear, size: 18),
                        onPressed: () {
                          _searchCtrl.clear();
                          setState(() => _search = '');
                        },
                      )
                    : null,
                contentPadding:
                    const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
              ),
              onChanged: (v) => setState(() => _search = v.trim()),
            ),
          ),
          const SizedBox(height: 8),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: Row(
              children: [
                _filterChip('All', _TxFilter.all),
                const SizedBox(width: 8),
                _filterChip('Received', _TxFilter.incoming),
                const SizedBox(width: 8),
                _filterChip('Sent', _TxFilter.outgoing),
              ],
            ),
          ),
          const SizedBox(height: 8),

          // Transaction list
          Expanded(
            child: txAsync.when(
              data: (txs) {
                final filtered = _applyFilters(txs);
                if (filtered.isEmpty) {
                  return Center(
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Icon(Icons.receipt_long,
                            size: 48,
                            color: Theme.of(context)
                                .textTheme
                                .bodySmall
                                ?.color),
                        const SizedBox(height: 12),
                        Text(
                          _search.isNotEmpty
                              ? 'No matching transactions'
                              : 'No transactions yet',
                          style: Theme.of(context).textTheme.bodyMedium,
                        ),
                      ],
                    ),
                  );
                }
                return ListView.builder(
                  padding: const EdgeInsets.symmetric(horizontal: 16),
                  itemCount: filtered.length,
                  itemBuilder: (_, i) =>
                      _ExpandableTxCard(tx: filtered[i]),
                );
              },
              loading: () => const Center(
                  child: CircularProgressIndicator(strokeWidth: 2)),
              error: (e, _) => Center(
                child: Text('Error: $e',
                    style: const TextStyle(color: kError)),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _filterChip(String label, _TxFilter filter) {
    final selected = _filter == filter;
    return ChoiceChip(
      label: Text(label),
      selected: selected,
      onSelected: (_) => setState(() => _filter = filter),
    );
  }
}

// ── expandable tx card ───────────────────────────────────────────────────────

class _ExpandableTxCard extends StatefulWidget {
  final Transaction tx;
  const _ExpandableTxCard({required this.tx});

  @override
  State<_ExpandableTxCard> createState() => _ExpandableTxCardState();
}

class _ExpandableTxCardState extends State<_ExpandableTxCard> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    final tx = widget.tx;
    final incoming = tx.isIncoming;
    final icon = incoming ? Icons.call_received : Icons.call_made;
    final color = incoming ? kSuccess : kError;
    final sign = incoming ? '+' : '-';
    final amount = tx.totalAmount.abs();

    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: InkWell(
        borderRadius: BorderRadius.circular(16),
        onTap: () => setState(() => _expanded = !_expanded),
        child: Padding(
          padding: const EdgeInsets.all(14),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Summary row
              Row(
                children: [
                  CircleAvatar(
                    radius: 18,
                    backgroundColor: color.withAlpha(25),
                    child: Icon(icon, color: color, size: 18),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          incoming ? 'Received' : 'Sent',
                          style:
                              Theme.of(context).textTheme.titleMedium,
                        ),
                        Text(
                          tx.isConfirmed
                              ? _formatDate(tx.dateTime)
                              : 'Pending...',
                          style:
                              Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.end,
                    children: [
                      Text(
                        '$sign${formatAmount(amount, showTicker: true)}',
                        style: Theme.of(context)
                            .textTheme
                            .titleMedium
                            ?.copyWith(color: color),
                      ),
                      if (!tx.isConfirmed)
                        Icon(Icons.schedule,
                            size: 14,
                            color: Theme.of(context)
                                .textTheme
                                .bodySmall
                                ?.color),
                    ],
                  ),
                  const SizedBox(width: 4),
                  Icon(
                    _expanded
                        ? Icons.expand_less
                        : Icons.expand_more,
                    size: 20,
                    color: Theme.of(context)
                        .textTheme
                        .bodySmall
                        ?.color,
                  ),
                ],
              ),

              // Expanded details
              if (_expanded) ...[
                const Divider(height: 20),
                _detailRow(context, 'Hash', tx.hash),
                if (tx.address.isNotEmpty)
                  _detailRow(context, 'Address', tx.address),
                _detailRow(
                    context, 'Block', '${tx.blockHeight}'),
                if (tx.fee > 0)
                  _detailRow(context, 'Fee',
                      formatAmount(tx.fee, showTicker: true)),
                if (tx.paymentID.isNotEmpty)
                  _detailRow(context, 'Payment ID', tx.paymentID),
                _detailRow(context, 'Status',
                    tx.isConfirmed ? 'Confirmed' : 'Pending'),
              ],
            ],
          ),
        ),
      ),
    );
  }

  Widget _detailRow(BuildContext context, String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 80,
            child: Text(label,
                style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(
            child: SelectableText(
              value,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    fontFamily: 'monospace',
                    fontSize: 11,
                  ),
            ),
          ),
          CopyButton(text: value, size: 16),
        ],
      ),
    );
  }

  String _formatDate(DateTime dt) {
    return '${dt.year}-${dt.month.toString().padLeft(2, '0')}-${dt.day.toString().padLeft(2, '0')} '
        '${dt.hour.toString().padLeft(2, '0')}:${dt.minute.toString().padLeft(2, '0')}';
  }
}
