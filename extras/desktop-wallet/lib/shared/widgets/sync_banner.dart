import 'package:flutter/material.dart';
import '../../core/api/models/wallet_status.dart';
import '../../l10n/generated/app_localizations.dart';
import '../theme/app_theme.dart';

/// Thin progress bar + label shown at the top of the overview while the wallet
/// is syncing. Tracks block-rate over the last 60 s to compute an ETA.
///
/// The lite-node notice is a separate widget shown by the shell, so it is up
/// on every screen and does not disappear when syncing finishes.
class SyncBanner extends StatefulWidget {
  final WalletStatus status;
  const SyncBanner({super.key, required this.status});

  @override
  State<SyncBanner> createState() => _SyncBannerState();
}

class _SyncBannerState extends State<SyncBanner> {
  // Ring of (timestamp, walletBlockCount) samples within the last 60 s
  final List<(DateTime, int)> _samples = [];

  @override
  void initState() {
    super.initState();
    _addSample();
  }

  @override
  void didUpdateWidget(SyncBanner old) {
    super.didUpdateWidget(old);
    if (old.status.walletBlockCount != widget.status.walletBlockCount) {
      _addSample();
    }
  }

  void _addSample() {
    _samples.add((DateTime.now(), widget.status.walletBlockCount));
    final cutoff = DateTime.now().subtract(const Duration(seconds: 60));
    _samples.removeWhere((s) => s.$1.isBefore(cutoff));
  }

  /// Returns a human-readable ETA string, or null if not enough data yet.
  String? _eta() {
    if (_samples.length < 2) return null;
    final oldest = _samples.first;
    final newest = _samples.last;
    final elapsedSec = newest.$1.difference(oldest.$1).inSeconds;
    if (elapsedSec == 0) return null;
    final blocksPerSec = (newest.$2 - oldest.$2) / elapsedSec;
    if (blocksPerSec <= 0) return null;
    final remaining =
        widget.status.networkBlockCount - widget.status.walletBlockCount;
    if (remaining <= 0) return null;
    final totalSec = remaining / blocksPerSec;
    if (totalSec < 90) return '< 2 min';
    final minutes = (totalSec / 60).round();
    if (minutes < 60) return '~$minutes min';
    final hours = minutes ~/ 60;
    final mins = minutes % 60;
    return mins > 0 ? '~${hours}h ${mins}m' : '~${hours}h';
  }

  Widget _progressRow(BuildContext context) {
    final tr = S.of(context);
    final pct = (widget.status.syncProgress * 100).toStringAsFixed(1);
    final eta = _eta();
    final label = StringBuffer(
        tr?.syncingProgress(pct, widget.status.walletBlockCount, widget.status.networkBlockCount)
        ?? 'Syncing $pct% (block ${widget.status.walletBlockCount} / ${widget.status.networkBlockCount})');
    if (eta != null) label.write(' ($eta)');

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
              value: widget.status.syncProgress,
              backgroundColor: kDivider,
              color: kWarning,
              minHeight: 4,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
          const SizedBox(width: 10),
          Text(
            label.toString(),
            style: const TextStyle(color: kWarning, fontSize: 12),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    // A stalled sync is not progress. The shell's LiteNodeBanner says why it
    // stopped; a bar creeping along underneath would contradict it.
    if (widget.status.isWalletSynced ||
        widget.status.hasReportableSyncGap) {
      return const SizedBox.shrink();
    }
    return _progressRow(context);
  }
}
