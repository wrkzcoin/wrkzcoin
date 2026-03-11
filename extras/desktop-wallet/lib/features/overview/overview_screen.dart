import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../../core/api/models/transaction.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/widgets/sync_banner.dart';

class OverviewScreen extends ConsumerWidget {
  const OverviewScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final balanceAsync = ref.watch(balanceProvider);
    final statusAsync = ref.watch(statusProvider);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Sync banner (hidden when synced)
        statusAsync.whenOrNull(
          data: (s) => SyncBanner(status: s),
        ) ?? const SizedBox.shrink(),

        Expanded(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(28),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('Overview', style: Theme.of(context).textTheme.headlineMedium),
                const SizedBox(height: 24),

                // ── Balance card ──────────────────────────────────────────
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        balanceAsync.when(
                          data: (b) => Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              // Available (unlocked)
                              Text('Available Balance', style: Theme.of(context).textTheme.titleSmall),
                              const SizedBox(height: 6),
                              Row(
                                crossAxisAlignment: CrossAxisAlignment.baseline,
                                textBaseline: TextBaseline.alphabetic,
                                children: [
                                  Text(
                                    formatAmount(b.unlocked),
                                    style: TextStyle(
                                      fontSize: 36,
                                      fontWeight: FontWeight.bold,
                                      color: Theme.of(context).colorScheme.onSurface,
                                    ),
                                  ),
                                                  const SizedBox(width: 8),
                                  Text(kCoinTicker, style: TextStyle(fontSize: 18, color: Theme.of(context).colorScheme.onSurfaceVariant)),
                                ],
                              ),
                              // Locked (unconfirmed)
                              const SizedBox(height: 10),
                              Row(
                                children: [
                                  const Icon(Icons.lock_outline, size: 14, color: kTextSecondary),
                                  const SizedBox(width: 4),
                                  Text(
                                    'Locked (unconfirmed): ${formatAmount(b.locked)} $kCoinTicker',
                                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                      color: b.locked > 0 ? kWarning : kTextSecondary,
                                    ),
                                  ),
                                ],
                              ),
                              // Total
                              const SizedBox(height: 4),
                              Row(
                                children: [
                                  const Icon(Icons.account_balance_wallet_outlined, size: 14, color: kTextSecondary),
                                  const SizedBox(width: 4),
                                  Text(
                                    'Total: ${formatAmount(b.total)} $kCoinTicker',
                                    style: Theme.of(context).textTheme.bodyMedium,
                                  ),
                                ],
                              ),
                            ],
                          ),
                          loading: () => Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text('Available Balance', style: Theme.of(context).textTheme.titleSmall),
                              const SizedBox(height: 6),
                              const _SkeletonBox(width: 200, height: 40),
                              const SizedBox(height: 10),
                              const _SkeletonBox(width: 260, height: 16),
                              const SizedBox(height: 6),
                              const _SkeletonBox(width: 200, height: 16),
                            ],
                          ),
                          error: (e, _) => Text('Error: $e', style: const TextStyle(color: kError)),
                        ),
                        // Show a warning when balance may be incomplete due to sync
                        if (statusAsync.valueOrNull?.isWalletSynced == false) ...[
                          const SizedBox(height: 8),
                          Row(
                            children: [
                              const Icon(Icons.sync, size: 13, color: kWarning),
                              const SizedBox(width: 4),
                              Text(
                                'Balance may be incomplete while syncing',
                                style: Theme.of(context).textTheme.bodySmall?.copyWith(color: kWarning),
                              ),
                            ],
                          ),
                        ],
                        const SizedBox(height: 20),
                        Row(
                          children: [
                            FilledButton.icon(
                              icon: const Icon(Icons.send_outlined, size: 16),
                              label: const Text('Transfer'),
                              onPressed: () => context.go('/transfer'),
                            ),
                            const SizedBox(width: 12),
                            OutlinedButton.icon(
                              icon: const Icon(Icons.qr_code_outlined, size: 16),
                              label: const Text('Receive'),
                              onPressed: () => context.go('/receive'),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ),

                const SizedBox(height: 20),

                // ── Network status card ───────────────────────────────────
                statusAsync.when(
                  data: (s) => Card(
                    child: Padding(
                      padding: const EdgeInsets.all(20),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('Network', style: Theme.of(context).textTheme.titleSmall),
                          const SizedBox(height: 14),
                          _StatusRow(
                            label: 'Sync status',
                            value: s.isWalletSynced ? 'Synced' : 'Syncing…',
                            valueColor: s.isWalletSynced ? kSuccess : kWarning,
                          ),
                          _StatusRow(label: 'Wallet block', value: s.walletBlockCount.toString()),
                          _StatusRow(label: 'Network block', value: s.networkBlockCount.toString()),
                          _StatusRow(label: 'Peers', value: s.peerCount.toString()),
                          _StatusRow(label: 'Wallet type', value: s.isViewWallet ? 'View-only' : 'Full'),
                        ],
                      ),
                    ),
                  ),
                  loading: () => const Card(child: Padding(padding: EdgeInsets.all(20), child: _SkeletonBox(width: double.infinity, height: 120))),
                  error: (e, _) => _NodeErrorCard(error: e.toString()),
                ),

                const SizedBox(height: 20),

                // ── Recent transactions ───────────────────────────────────
                _RecentTransactions(),
              ],
            ),
          ),
        ),
      ],
    );
  }
}

class _NodeErrorCard extends StatelessWidget {
  final String error;
  const _NodeErrorCard({required this.error});

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Row(
          children: [
            const Icon(Icons.cloud_off_outlined, color: kError),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text('Node connection issue', style: TextStyle(color: kError, fontWeight: FontWeight.w600)),
                  const SizedBox(height: 4),
                  Text(error, style: const TextStyle(color: kTextSecondary, fontSize: 12)),
                  const SizedBox(height: 8),
                  TextButton(
                    onPressed: () => context.go('/settings'),
                    child: const Text('Switch node in Settings →'),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _RecentTransactions extends ConsumerWidget {
  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final txAsync = ref.watch(transactionsProvider);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text('Recent Transactions', style: Theme.of(context).textTheme.titleSmall),
            TextButton(onPressed: () => context.go('/history'), child: const Text('View all →')),
          ],
        ),
        const SizedBox(height: 8),
        txAsync.when(
          data: (txs) {
            final recent = txs.take(5).toList();
            if (recent.isEmpty) {
              return const _EmptyTxPlaceholder();
            }
            return Column(
              children: recent.map((tx) => _TxRow(tx: tx)).toList(),
            );
          },
          loading: () => Column(
            children: List.generate(3, (_) => Padding(
              padding: const EdgeInsets.only(bottom: 8),
              child: const _SkeletonBox(width: double.infinity, height: 52),
            )),
          ),
          error: (_, _) => const _EmptyTxPlaceholder(),
        ),
      ],
    );
  }
}

class _TxRow extends StatelessWidget {
  final Transaction tx;
  const _TxRow({required this.tx});

  @override
  Widget build(BuildContext context) {
    final incoming = tx.isIncoming;
    final amount = tx.totalAmount;
    return Card(
      margin: const EdgeInsets.only(bottom: 6),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Row(
          children: [
            Container(
              width: 36, height: 36,
              decoration: BoxDecoration(
                color: (incoming ? kSuccess : kError).withAlpha(25),
                borderRadius: BorderRadius.circular(8),
              ),
              child: Icon(
                incoming ? Icons.arrow_downward : Icons.arrow_upward,
                size: 16,
                color: incoming ? kSuccess : kError,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    incoming ? 'Received' : 'Sent',
                    style: Theme.of(context).textTheme.bodyLarge,
                  ),
                  Text(
                    tx.dateTime.toString().substring(0, 16),
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ],
              ),
            ),
            Text(
              '${incoming ? '+' : '-'}${formatAmount(amount.abs())} $kCoinTicker',
              style: TextStyle(
                color: incoming ? kSuccess : kError,
                fontWeight: FontWeight.w600,
                fontSize: 13,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _EmptyTxPlaceholder extends StatelessWidget {
  const _EmptyTxPlaceholder();

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(28),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceContainerLow,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
      ),
      child: const Column(
        children: [
          Icon(Icons.receipt_long_outlined, size: 32, color: kTextDisabled),
          SizedBox(height: 8),
          Text('No transactions yet', style: TextStyle(color: kTextDisabled)),
        ],
      ),
    );
  }
}

class _StatusRow extends StatelessWidget {
  final String label;
  final String value;
  final Color? valueColor;
  const _StatusRow({required this.label, required this.value, this.valueColor});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 13)),
          Text(value, style: TextStyle(color: valueColor ?? Theme.of(context).colorScheme.onSurface, fontSize: 13, fontWeight: FontWeight.w500)),
        ],
      ),
    );
  }
}

class _SkeletonBox extends StatelessWidget {
  final double width;
  final double height;
  const _SkeletonBox({required this.width, required this.height});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: width,
      height: height,
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceContainerHighest,
        borderRadius: BorderRadius.circular(6),
      ),
    );
  }
}
