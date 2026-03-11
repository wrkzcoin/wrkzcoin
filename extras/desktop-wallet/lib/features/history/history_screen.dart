import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';
import '../../core/api/models/transaction.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/widgets/copy_button.dart';

// ── Filter model ──────────────────────────────────────────────────────────────

enum _TxFilter { all, incoming, outgoing }

class _HistoryFilter {
  final _TxFilter direction;
  final String search;

  const _HistoryFilter({this.direction = _TxFilter.all, this.search = ''});

  _HistoryFilter copyWith({_TxFilter? direction, String? search}) =>
      _HistoryFilter(
        direction: direction ?? this.direction,
        search: search ?? this.search,
      );
}

final _filterProvider = StateProvider<_HistoryFilter>((_) => const _HistoryFilter());
final _pageProvider = StateProvider<int>((_) => 0);
const _kPageSize = 25;

// ── Screen ────────────────────────────────────────────────────────────────────

class HistoryScreen extends ConsumerWidget {
  const HistoryScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final txAsync = ref.watch(transactionsProvider);
    final filter = ref.watch(_filterProvider);
    final page = ref.watch(_pageProvider);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // ── Header + filter bar ───────────────────────────────────────────
        Container(
          padding: const EdgeInsets.fromLTRB(28, 28, 28, 0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Transaction History', style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 16),
              Row(
                children: [
                  // Search
                  Expanded(
                    child: SizedBox(
                      height: 40,
                      child: TextField(
                        decoration: const InputDecoration(
                          hintText: 'Search by hash, address or payment ID…',
                          prefixIcon: Icon(Icons.search, size: 18),
                          contentPadding: EdgeInsets.symmetric(vertical: 0, horizontal: 12),
                        ),
                        onChanged: (v) {
                          ref.read(_filterProvider.notifier).state =
                              filter.copyWith(search: v.toLowerCase());
                          ref.read(_pageProvider.notifier).state = 0;
                        },
                      ),
                    ),
                  ),
                  const SizedBox(width: 12),
                  // Direction filter chips
                  _FilterChip(
                    label: 'All',
                    selected: filter.direction == _TxFilter.all,
                    onTap: () {
                      ref.read(_filterProvider.notifier).state = filter.copyWith(direction: _TxFilter.all);
                      ref.read(_pageProvider.notifier).state = 0;
                    },
                  ),
                  const SizedBox(width: 6),
                  _FilterChip(
                    label: 'Received',
                    selected: filter.direction == _TxFilter.incoming,
                    onTap: () {
                      ref.read(_filterProvider.notifier).state = filter.copyWith(direction: _TxFilter.incoming);
                      ref.read(_pageProvider.notifier).state = 0;
                    },
                  ),
                  const SizedBox(width: 6),
                  _FilterChip(
                    label: 'Sent',
                    selected: filter.direction == _TxFilter.outgoing,
                    onTap: () {
                      ref.read(_filterProvider.notifier).state = filter.copyWith(direction: _TxFilter.outgoing);
                      ref.read(_pageProvider.notifier).state = 0;
                    },
                  ),
                  const SizedBox(width: 12),
                  // Refresh
                  IconButton(
                    icon: const Icon(Icons.refresh, size: 18),
                    tooltip: 'Refresh',
                    onPressed: () => ref.read(transactionsProvider.notifier).refresh(),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              const Divider(height: 1),
            ],
          ),
        ),

        // ── Transaction list ──────────────────────────────────────────────
        Expanded(
          child: txAsync.when(
            data: (all) {
              final filtered = _applyFilter(all, filter);
              final pageCount = (filtered.length / _kPageSize).ceil().clamp(1, double.maxFinite.toInt());
              final safePage = page.clamp(0, pageCount - 1);
              final slice = filtered.skip(safePage * _kPageSize).take(_kPageSize).toList();

              if (filtered.isEmpty) {
                return const Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(Icons.receipt_long_outlined, size: 40, color: kTextDisabled),
                      SizedBox(height: 10),
                      Text('No transactions found', style: TextStyle(color: kTextDisabled)),
                    ],
                  ),
                );
              }

              return Column(
                children: [
                  // List
                  Expanded(
                    child: ListView.separated(
                      padding: const EdgeInsets.all(16),
                      itemCount: slice.length,
                      separatorBuilder: (_, _) => const SizedBox(height: 4),
                      itemBuilder: (ctx, i) => _TxCard(tx: slice[i]),
                    ),
                  ),
                  // Pagination bar
                  if (pageCount > 1)
                    _PaginationBar(
                      current: safePage,
                      total: pageCount,
                      totalItems: filtered.length,
                      onPage: (p) => ref.read(_pageProvider.notifier).state = p,
                    ),
                ],
              );
            },
            loading: () => const Center(child: CircularProgressIndicator()),
            error: (e, _) => Center(child: Text('Error: $e', style: const TextStyle(color: kError))),
          ),
        ),
      ],
    );
  }

  List<Transaction> _applyFilter(List<Transaction> all, _HistoryFilter f) {
    return all.where((tx) {
      if (f.direction == _TxFilter.incoming && !tx.isIncoming) return false;
      if (f.direction == _TxFilter.outgoing && tx.isIncoming) return false;
      if (f.search.isNotEmpty) {
        final q = f.search;
        if (!tx.hash.contains(q) &&
            !tx.address.toLowerCase().contains(q) &&
            !tx.paymentID.toLowerCase().contains(q)) {
          return false;
        }
      }
      return true;
    }).toList();
  }
}

// ── Transaction card ──────────────────────────────────────────────────────────

class _TxCard extends StatefulWidget {
  final Transaction tx;
  const _TxCard({required this.tx});

  @override
  State<_TxCard> createState() => _TxCardState();
}

class _TxCardState extends State<_TxCard> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    final tx = widget.tx;
    final incoming = tx.isIncoming;
    final color = incoming ? kSuccess : kError;
    final fmt = DateFormat('dd MMM yyyy  HH:mm');

    return Card(
      child: InkWell(
        onTap: () => setState(() => _expanded = !_expanded),
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Summary row
              Row(
                children: [
                  Container(
                    width: 32, height: 32,
                    decoration: BoxDecoration(
                      color: color.withAlpha(25),
                      borderRadius: BorderRadius.circular(6),
                    ),
                    child: Icon(
                      incoming ? Icons.arrow_downward : Icons.arrow_upward,
                      size: 14, color: color,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(incoming ? 'Received' : 'Sent',
                            style: const TextStyle(color: kTextPrimary, fontSize: 13, fontWeight: FontWeight.w500)),
                        Text(fmt.format(tx.dateTime),
                            style: const TextStyle(color: kTextSecondary, fontSize: 11)),
                      ],
                    ),
                  ),
                  Column(
                    crossAxisAlignment: CrossAxisAlignment.end,
                    children: [
                      Text(
                        '${incoming ? '+' : '-'}${formatAmount(tx.totalAmount.abs())} $kCoinTicker',
                        style: TextStyle(color: color, fontWeight: FontWeight.w600, fontSize: 13),
                      ),
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                        decoration: BoxDecoration(
                          color: tx.isConfirmed ? kSuccess.withAlpha(25) : kWarning.withAlpha(25),
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Text(
                          tx.isConfirmed ? 'Confirmed' : 'Pending',
                          style: TextStyle(
                            fontSize: 10,
                            color: tx.isConfirmed ? kSuccess : kWarning,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(width: 8),
                  Icon(_expanded ? Icons.expand_less : Icons.expand_more, size: 16, color: kTextSecondary),
                ],
              ),

              // Expanded details
              if (_expanded) ...[
                const SizedBox(height: 12),
                const Divider(height: 1),
                const SizedBox(height: 12),
                _DetailRow(label: 'Hash', value: tx.hash, mono: true, copyable: true),
                if (tx.address.isNotEmpty)
                  _DetailRow(label: 'Address', value: tx.address, mono: true),
                _DetailRow(label: 'Block', value: tx.blockHeight.toString()),
                _DetailRow(label: 'Fee', value: '${formatAmount(tx.fee)} $kCoinTicker'),
                if (tx.paymentID.isNotEmpty)
                  _DetailRow(label: 'Payment ID', value: tx.paymentID, mono: true, copyable: true),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _DetailRow extends StatelessWidget {
  final String label;
  final String value;
  final bool mono;
  final bool copyable;
  const _DetailRow({required this.label, required this.value, this.mono = false, this.copyable = false});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(width: 90, child: Text(label, style: const TextStyle(color: kTextSecondary, fontSize: 12))),
          Expanded(
            child: Text(
              value,
              style: TextStyle(
                fontSize: 12,
                fontFamily: mono ? 'monospace' : null,
                color: kTextPrimary,
              ),
            ),
          ),
          if (copyable) CopyButton(text: value, size: 14),
        ],
      ),
    );
  }
}

// ── Pagination bar ────────────────────────────────────────────────────────────

class _PaginationBar extends StatelessWidget {
  final int current;
  final int total;
  final int totalItems;
  final void Function(int) onPage;

  const _PaginationBar({
    required this.current,
    required this.total,
    required this.totalItems,
    required this.onPage,
  });

  @override
  Widget build(BuildContext context) {
    final start = current * _kPageSize + 1;
    final end = ((current + 1) * _kPageSize).clamp(0, totalItems);
    return Container(
      height: 48,
      decoration: const BoxDecoration(
        border: Border(top: BorderSide(color: kDivider)),
        color: kSurface,
      ),
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Row(
        children: [
          Text(
            'Showing $start–$end of $totalItems',
            style: const TextStyle(color: kTextSecondary, fontSize: 12),
          ),
          const Spacer(),
          IconButton(
            icon: const Icon(Icons.chevron_left, size: 18),
            onPressed: current > 0 ? () => onPage(current - 1) : null,
            tooltip: 'Previous',
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 8),
            child: Text('${current + 1} / $total', style: const TextStyle(fontSize: 12, color: kTextPrimary)),
          ),
          IconButton(
            icon: const Icon(Icons.chevron_right, size: 18),
            onPressed: current < total - 1 ? () => onPage(current + 1) : null,
            tooltip: 'Next',
          ),
        ],
      ),
    );
  }
}

class _FilterChip extends StatelessWidget {
  final String label;
  final bool selected;
  final VoidCallback onTap;
  const _FilterChip({required this.label, required this.selected, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
        decoration: BoxDecoration(
          color: selected ? kPrimary.withAlpha(30) : kSurfaceVariant,
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: selected ? kPrimary : kDivider),
        ),
        child: Text(
          label,
          style: TextStyle(
            fontSize: 12,
            color: selected ? kPrimary : kTextSecondary,
            fontWeight: selected ? FontWeight.w600 : FontWeight.normal,
          ),
        ),
      ),
    );
  }
}
