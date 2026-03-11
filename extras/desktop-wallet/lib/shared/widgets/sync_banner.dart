import 'package:flutter/material.dart';
import '../../core/api/models/wallet_status.dart';
import '../theme/app_theme.dart';

/// Thin progress bar + label shown at the top of screens while wallet is syncing.
class SyncBanner extends StatelessWidget {
  final WalletStatus status;
  const SyncBanner({super.key, required this.status});

  @override
  Widget build(BuildContext context) {
    if (status.isWalletSynced) return const SizedBox.shrink();

    final pct = (status.syncProgress * 100).toStringAsFixed(1);
    return Container(
      width: double.infinity,
      color: kWarning.withAlpha(25),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
      child: Row(
        children: [
          const SizedBox(
            width: 14,
            height: 14,
            child: CircularProgressIndicator(strokeWidth: 2, color: kWarning),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: LinearProgressIndicator(
              value: status.syncProgress,
              backgroundColor: kDivider,
              color: kWarning,
              minHeight: 4,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
          const SizedBox(width: 10),
          Text(
            'Syncing $pct% (block ${status.walletBlockCount} / ${status.networkBlockCount})',
            style: const TextStyle(color: kWarning, fontSize: 12),
          ),
        ],
      ),
    );
  }
}
