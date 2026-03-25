import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/api/models/balance.dart';
import '../../core/api/models/transaction.dart';
import '../../core/api/models/wallet_status.dart';
import '../../core/auth/wallet_auth.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/offline_banner.dart';
import '../../shared/widgets/sync_banner.dart';

class OverviewScreen extends ConsumerWidget {
  const OverviewScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final statusAsync = ref.watch(statusProvider);
    final balanceAsync = ref.watch(balanceProvider);
    final txAsync = ref.watch(transactionsProvider);
    final nodeAsync = ref.watch(nodeInfoProvider);
    final tr = S.of(context)!;

    return RefreshIndicator(
      onRefresh: () async {
        await Future.wait([
          ref.read(statusProvider.notifier).refresh(),
          ref.read(balanceProvider.notifier).refresh(),
          ref.read(transactionsProvider.notifier).refresh(),
        ]);
        hapticMedium();
      },
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Seed backup warning
          _SeedBackupWarning(ref: ref),

          // Offline banner
          statusAsync.whenOrNull(
                data: (status) {
                  final daemonOnline =
                      nodeAsync.valueOrNull?['daemonOnline'] as bool? ?? true;
                  if (!daemonOnline) {
                    return const Padding(
                      padding: EdgeInsets.only(bottom: 12),
                      child: OfflineBanner(),
                    );
                  }
                  return null;
                },
              ) ??
              const SizedBox.shrink(),

          // Sync banner
          statusAsync.whenOrNull(
                data: (status) {
                  if (!status.isWalletSynced) {
                    return Padding(
                      padding: const EdgeInsets.only(bottom: 12),
                      child: SyncBanner(status: status),
                    );
                  }
                  return null;
                },
              ) ??
              const SizedBox.shrink(),

          // Balance card
          _BalanceCard(balanceAsync: balanceAsync),

          const SizedBox(height: 16),

          // Quick actions
          Row(
            children: [
              Expanded(
                child: FilledButton.icon(
                  onPressed: () => context.go('/transfer'),
                  icon: const Icon(Icons.send, size: 18),
                  label: Text(tr.send),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: () => context.go('/receive'),
                  icon: const Icon(Icons.qr_code, size: 18),
                  label: Text(tr.receive),
                ),
              ),
            ],
          ),

          const SizedBox(height: 24),

          // Network status
          _NetworkStatusCard(statusAsync: statusAsync, nodeAsync: nodeAsync),

          const SizedBox(height: 24),

          // Recent transactions
          Row(
            children: [
              Text(tr.recentTransactions,
                  style: Theme.of(context).textTheme.titleLarge),
              const Spacer(),
              TextButton(
                onPressed: () => context.go('/history'),
                child: Text(tr.viewAll),
              ),
            ],
          ),
          const SizedBox(height: 8),
          _RecentTransactions(txAsync: txAsync),
        ],
      ),
    );
  }
}

// ── seed backup warning ──────────────────────────────────────────────────────

class _SeedBackupWarning extends StatefulWidget {
  final WidgetRef ref;
  const _SeedBackupWarning({required this.ref});

  @override
  State<_SeedBackupWarning> createState() => _SeedBackupWarningState();
}

class _SeedBackupWarningState extends State<_SeedBackupWarning> {
  bool _show = false;

  @override
  void initState() {
    super.initState();
    _check();
  }

  Future<void> _check() async {
    final filename = widget.ref.read(activeWalletFilenameProvider);
    if (filename == null) return;
    final confirmed = await isSeedBackupConfirmed(filename);
    if (mounted && !confirmed) setState(() => _show = true);
  }

  @override
  Widget build(BuildContext context) {
    if (!_show) return const SizedBox.shrink();
    final tr = S.of(context)!;
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Container(
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: kWarning.withAlpha(25),
          borderRadius: BorderRadius.circular(12),
        ),
        child: Row(
          children: [
            const Icon(Icons.warning_amber, color: kWarning, size: 20),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                tr.seedBackupWarning,
                style: Theme.of(context)
                    .textTheme
                    .bodySmall
                    ?.copyWith(color: kWarning),
              ),
            ),
            IconButton(
              icon: const Icon(Icons.close, size: 18),
              onPressed: () => setState(() => _show = false),
              visualDensity: VisualDensity.compact,
            ),
          ],
        ),
      ),
    );
  }
}

// ── balance card ─────────────────────────────────────────────────────────────

class _BalanceCard extends StatelessWidget {
  final AsyncValue<Balance> balanceAsync;
  const _BalanceCard({required this.balanceAsync});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: balanceAsync.when(
          data: (balance) => Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(tr.available,
                  style: Theme.of(context).textTheme.bodySmall),
              const SizedBox(height: 4),
              Text(
                formatAmount(balance.unlocked, showTicker: true),
                style: Theme.of(context)
                    .textTheme
                    .headlineLarge
                    ?.copyWith(color: kPrimary),
              ),
              if (balance.locked > 0) ...[
                const SizedBox(height: 8),
                Row(
                  children: [
                    Icon(Icons.lock_outline,
                        size: 14,
                        color: Theme.of(context).textTheme.bodySmall?.color),
                    const SizedBox(width: 4),
                    Text(
                      tr.lockedAmount(formatAmount(balance.locked, showTicker: true)),
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ],
                ),
                const SizedBox(height: 4),
                Text(
                  tr.totalAmount(formatAmount(balance.total, showTicker: true)),
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ],
          ),
          loading: () => const SizedBox(
            height: 80,
            child: Center(child: CircularProgressIndicator(strokeWidth: 2)),
          ),
          error: (e, _) => Text(tr.errorPrefix(e.toString()),
              style: const TextStyle(color: kError, fontSize: 13)),
        ),
      ),
    );
  }
}

// ── network status card ──────────────────────────────────────────────────────

class _NetworkStatusCard extends StatelessWidget {
  final AsyncValue<WalletStatus> statusAsync;
  final AsyncValue<Map<String, dynamic>> nodeAsync;
  const _NetworkStatusCard(
      {required this.statusAsync, required this.nodeAsync});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: statusAsync.when(
          data: (status) {
            final daemonHost =
                nodeAsync.valueOrNull?['daemonHost'] as String? ?? '—';
            final daemonPort =
                nodeAsync.valueOrNull?['daemonPort'] as int? ?? 0;
            final online =
                nodeAsync.valueOrNull?['daemonOnline'] as bool? ?? false;

            return Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(tr.networkStatus,
                    style: Theme.of(context).textTheme.titleMedium),
                const SizedBox(height: 12),
                _statusRow(context, tr.node, '$daemonHost:$daemonPort'),
                _statusRow(context, tr.status,
                    online ? tr.connected : tr.disconnected),
                _statusRow(context, tr.walletHeight,
                    '${status.walletBlockCount}'),
                _statusRow(context, tr.networkHeight,
                    '${status.networkBlockCount}'),
                _statusRow(context, tr.peers, '${status.peerCount}'),
                if (status.isViewWallet)
                  _statusRow(context, tr.type, tr.viewOnly),
              ],
            );
          },
          loading: () => const SizedBox(
            height: 60,
            child: Center(child: CircularProgressIndicator(strokeWidth: 2)),
          ),
          error: (e, _) => Text(
            tr.couldNotFetchStatus,
            style: TextStyle(color: kError, fontSize: 13),
          ),
        ),
      ),
    );
  }

  Widget _statusRow(BuildContext context, String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: Theme.of(context).textTheme.bodySmall),
          Text(value, style: Theme.of(context).textTheme.bodyMedium),
        ],
      ),
    );
  }
}

// ── recent transactions ──────────────────────────────────────────────────────

class _RecentTransactions extends StatelessWidget {
  final AsyncValue<List<Transaction>> txAsync;
  const _RecentTransactions({required this.txAsync});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    return txAsync.when(
      data: (txs) {
        if (txs.isEmpty) {
          return Card(
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: Center(
                child: Text(tr.noTransactionsYet,
                    style: Theme.of(context).textTheme.bodySmall),
              ),
            ),
          );
        }
        final recent = txs.take(5).toList();
        return Column(
          children: recent.map((tx) => _TxCard(tx: tx)).toList(),
        );
      },
      loading: () => const SizedBox(
        height: 80,
        child: Center(child: CircularProgressIndicator(strokeWidth: 2)),
      ),
      error: (e, _) => Text(tr.errorPrefix(e.toString()),
          style: const TextStyle(color: kError, fontSize: 13)),
    );
  }
}

class _TxCard extends StatelessWidget {
  final Transaction tx;
  const _TxCard({required this.tx});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context)!;
    final incoming = tx.isIncoming;
    final icon = incoming ? Icons.call_received : Icons.call_made;
    final color = incoming ? kSuccess : kError;
    final sign = incoming ? '+' : '-';
    final amount = tx.totalAmount.abs();

    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: color.withAlpha(25),
          child: Icon(icon, color: color, size: 20),
        ),
        title: Text(
          '$sign${formatAmount(amount, showTicker: true)}',
          style: Theme.of(context)
              .textTheme
              .titleMedium
              ?.copyWith(color: color),
        ),
        subtitle: Text(
          tx.isConfirmed
              ? _formatDate(context, tx.dateTime)
              : tr.pending,
          style: Theme.of(context).textTheme.bodySmall,
        ),
        trailing: tx.isConfirmed
            ? null
            : Icon(Icons.schedule,
                size: 16,
                color: Theme.of(context).textTheme.bodySmall?.color),
      ),
    );
  }

  String _formatDate(BuildContext context, DateTime dt) {
    final tr = S.of(context)!;
    final now = DateTime.now();
    final diff = now.difference(dt);
    if (diff.inMinutes < 1) return tr.justNow;
    if (diff.inHours < 1) return tr.minutesAgo(diff.inMinutes);
    if (diff.inDays < 1) return tr.hoursAgo(diff.inHours);
    if (diff.inDays < 7) return tr.daysAgo(diff.inDays);
    return '${dt.month}/${dt.day}/${dt.year}';
  }
}
