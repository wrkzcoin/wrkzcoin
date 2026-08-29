import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:intl/intl.dart';
import 'package:url_launcher/url_launcher.dart';
import '../../core/api/models/transaction.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
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

class HistoryScreen extends ConsumerStatefulWidget {
  const HistoryScreen({super.key});

  @override
  ConsumerState<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends ConsumerState<HistoryScreen> {
  final _searchCtrl = TextEditingController();
  Timer? _debounce;

  @override
  void initState() {
    super.initState();
    _searchCtrl.text = ref.read(_filterProvider).search;
  }

  @override
  void dispose() {
    _debounce?.cancel();
    _searchCtrl.dispose();
    super.dispose();
  }

  /// Every keystroke used to re-filter the entire history synchronously.
  void _onSearchChanged(String value) {
    _debounce?.cancel();
    _debounce = Timer(const Duration(milliseconds: 250), () {
      if (!mounted) return;
      ref.read(_filterProvider.notifier).state =
          ref.read(_filterProvider).copyWith(search: value.trim().toLowerCase());
      ref.read(_pageProvider.notifier).state = 0;
    });
  }

  void _setDirection(_TxFilter direction) {
    ref.read(_filterProvider.notifier).state =
        ref.read(_filterProvider).copyWith(direction: direction);
    ref.read(_pageProvider.notifier).state = 0;
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final txAsync = ref.watch(transactionsProvider);
    final filter = ref.watch(_filterProvider);
    final page = ref.watch(_pageProvider);
    final narrow = MediaQuery.sizeOf(context).width < 600;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // ── Header + filter bar ───────────────────────────────────────────
        Container(
          padding: EdgeInsets.fromLTRB(narrow ? 16 : 28, narrow ? 16 : 28, narrow ? 16 : 28, 0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(tr?.transactionHistory ?? 'Transaction History', style: Theme.of(context).textTheme.headlineMedium),
              const SizedBox(height: 16),
              // Wrap, not Row: on a phone the search field plus three chips
              // plus the refresh button overflowed the viewport.
              Wrap(
                spacing: 8,
                runSpacing: 8,
                crossAxisAlignment: WrapCrossAlignment.center,
                children: [
                  SizedBox(
                    height: 40,
                    width: narrow ? double.infinity : 320,
                    child: TextField(
                      controller: _searchCtrl,
                      decoration: InputDecoration(
                        hintText: tr?.searchByHash ?? 'Search by hash, address or payment ID\u2026',
                        prefixIcon: const Icon(Icons.search, size: 18),
                        suffixIcon: filter.search.isEmpty
                            ? null
                            : IconButton(
                                icon: const Icon(Icons.clear, size: 16),
                                tooltip: tr?.clear ?? 'Clear',
                                onPressed: () {
                                  _searchCtrl.clear();
                                  _onSearchChanged('');
                                },
                              ),
                        contentPadding: const EdgeInsets.symmetric(vertical: 0, horizontal: 12),
                      ),
                      onChanged: _onSearchChanged,
                    ),
                  ),
                  _FilterChip(
                    label: tr?.all ?? 'All',
                    selected: filter.direction == _TxFilter.all,
                    onTap: () => _setDirection(_TxFilter.all),
                  ),
                  _FilterChip(
                    label: tr?.filterReceived ?? 'Received',
                    selected: filter.direction == _TxFilter.incoming,
                    onTap: () => _setDirection(_TxFilter.incoming),
                  ),
                  _FilterChip(
                    label: tr?.filterSent ?? 'Sent',
                    selected: filter.direction == _TxFilter.outgoing,
                    onTap: () => _setDirection(_TxFilter.outgoing),
                  ),
                  IconButton(
                    icon: const Icon(Icons.refresh, size: 18),
                    tooltip: tr?.refresh ?? 'Refresh',
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
                return Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(Icons.receipt_long_outlined, size: 40, color: context.textDisabled),
                      const SizedBox(height: 10),
                      Text(tr?.noTransactionsFound ?? 'No transactions found', style: TextStyle(color: context.textDisabled)),
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
            error: (e, _) => Center(child: Text(tr?.errorPrefix(e.toString()) ?? 'Error: $e', style: const TextStyle(color: kError))),
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
        // The query is lower-cased; the hash was compared raw, so any
        // upper-case character in a hash made it unsearchable.
        final q = f.search;
        if (!tx.hash.toLowerCase().contains(q) &&
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
    final tr = S.of(context);
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
                        Text(incoming ? (tr?.received ?? 'Received') : (tr?.sent ?? 'Sent'),
                            style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontSize: 13, fontWeight: FontWeight.w500)),
                        Text(
                            // Pending transactions have no block timestamp yet;
                            // formatting 0 rendered every one as 1 Jan 1970.
                            tx.dateTime == null
                                ? (tr?.pendingConfirmation ?? 'Pending confirmation')
                                : fmt.format(tx.dateTime!),
                            style: TextStyle(color: context.textSecondary, fontSize: 11)),
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
                          tx.isConfirmed ? (tr?.confirmed ?? 'Confirmed') : (tr?.pending ?? 'Pending'),
                          style: TextStyle(
                            fontSize: 10,
                            color: tx.isConfirmed ? kSuccess : kWarning,
                          ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(width: 8),
                  Icon(_expanded ? Icons.expand_less : Icons.expand_more, size: 16, color: context.textSecondary),
                ],
              ),

              // Expanded details
              if (_expanded) ...[
                const SizedBox(height: 12),
                const Divider(height: 1),
                const SizedBox(height: 12),
                _DetailRow(label: tr?.hash ?? 'Hash', value: tx.hash, mono: true, copyable: true),
                if (tx.address.isNotEmpty)
                  _DetailRow(label: tr?.address ?? 'Address', value: tx.address, mono: true),
                _DetailRow(label: tr?.block ?? 'Block', value: tx.blockHeight.toString()),
                _DetailRow(label: tr?.fee ?? 'Fee', value: '${formatAmount(tx.fee)} $kCoinTicker'),
                if (tx.paymentID.isNotEmpty)
                  _DetailRow(label: tr?.paymentId ?? 'Payment ID', value: tx.paymentID, mono: true, copyable: true),
                const SizedBox(height: 4),
                Align(
                  alignment: Alignment.centerLeft,
                  child: TextButton.icon(
                    icon: const Icon(Icons.open_in_new, size: 14),
                    label: Text(tr?.viewInExplorer ?? 'View in explorer',
                        style: const TextStyle(fontSize: 12)),
                    style: TextButton.styleFrom(
                      padding: const EdgeInsets.symmetric(horizontal: 8),
                      minimumSize: const Size(0, 32),
                    ),
                    onPressed: () => launchUrl(
                      Uri.parse(kExplorerTxUrl.replaceAll('{hash}', tx.hash)),
                      mode: LaunchMode.externalApplication,
                    ),
                  ),
                ),
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
          SizedBox(width: 90, child: Text(label, style: TextStyle(color: context.textSecondary, fontSize: 12))),
          Expanded(
            child: Text(
              value,
              style: TextStyle(
                fontSize: 12,
                fontFamily: mono ? 'monospace' : null,
                color: Theme.of(context).colorScheme.onSurface,
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
    final tr = S.of(context);
    final start = current * _kPageSize + 1;
    final end = ((current + 1) * _kPageSize).clamp(0, totalItems);
    return Container(
      height: 48,
      decoration: BoxDecoration(
        border: Border(top: BorderSide(color: Theme.of(context).colorScheme.outlineVariant)),
        color: Theme.of(context).colorScheme.surfaceContainer,
      ),
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Row(
        children: [
          Text(
            tr?.showingRange(start, end, totalItems) ?? 'Showing $start\u2013$end of $totalItems',
            style: TextStyle(color: context.textSecondary, fontSize: 12),
          ),
          const Spacer(),
          IconButton(
            icon: const Icon(Icons.chevron_left, size: 18),
            onPressed: current > 0 ? () => onPage(current - 1) : null,
            tooltip: tr?.previous ?? 'Previous',
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 8),
            child: Text('${current + 1} / $total', style: TextStyle(fontSize: 12, color: Theme.of(context).colorScheme.onSurface)),
          ),
          IconButton(
            icon: const Icon(Icons.chevron_right, size: 18),
            onPressed: current < total - 1 ? () => onPage(current + 1) : null,
            tooltip: tr?.next ?? 'Next',
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
          color: selected ? kPrimary.withAlpha(30) : context.surfaceVariantColor,
          borderRadius: BorderRadius.circular(20),
          border: Border.all(color: selected ? kPrimary : context.dividerColor),
        ),
        child: Text(
          label,
          style: TextStyle(
            fontSize: 12,
            color: selected ? kPrimary : context.textSecondary,
            fontWeight: selected ? FontWeight.w600 : FontWeight.normal,
          ),
        ),
      ),
    );
  }
}
