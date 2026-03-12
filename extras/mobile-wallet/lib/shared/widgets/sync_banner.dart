import 'dart:collection';

import 'package:flutter/material.dart';

import '../../core/api/models/wallet_status.dart';
import '../theme/app_theme.dart';

class SyncBanner extends StatefulWidget {
  final WalletStatus status;

  const SyncBanner({super.key, required this.status});

  @override
  State<SyncBanner> createState() => _SyncBannerState();
}

class _SyncBannerState extends State<SyncBanner> {
  final _samples = Queue<_Sample>();
  static const _windowSecs = 60;

  @override
  void didUpdateWidget(covariant SyncBanner old) {
    super.didUpdateWidget(old);
    if (widget.status.walletBlockCount != old.status.walletBlockCount) {
      _samples.addLast(_Sample(
        DateTime.now(),
        widget.status.walletBlockCount,
      ));
      while (_samples.length > 120) {
        _samples.removeFirst();
      }
    }
  }

  String _eta() {
    if (_samples.length < 2) return '';
    final now = DateTime.now();
    final cutoff = now.subtract(const Duration(seconds: _windowSecs));
    while (_samples.isNotEmpty && _samples.first.time.isBefore(cutoff)) {
      _samples.removeFirst();
    }
    if (_samples.length < 2) return '';
    final oldest = _samples.first;
    final newest = _samples.last;
    final elapsed = newest.time.difference(oldest.time).inSeconds;
    if (elapsed <= 0) return '';
    final blocksDone = newest.height - oldest.height;
    if (blocksDone <= 0) return '';
    final bps = blocksDone / elapsed;
    final remaining =
        widget.status.networkBlockCount - widget.status.walletBlockCount;
    if (remaining <= 0) return '';
    final secsLeft = (remaining / bps).round();
    if (secsLeft < 60) return '~${secsLeft}s';
    if (secsLeft < 3600) return '~${(secsLeft / 60).round()} min';
    final h = secsLeft ~/ 3600;
    final m = (secsLeft % 3600) ~/ 60;
    return '~${h}h ${m}m';
  }

  @override
  Widget build(BuildContext context) {
    final progress = widget.status.syncProgress;
    final pct = (progress * 100).toStringAsFixed(1);
    final eta = _eta();

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      decoration: BoxDecoration(
        color: kPrimary.withAlpha(25),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(
            children: [
              const SizedBox(
                width: 16,
                height: 16,
                child: CircularProgressIndicator(strokeWidth: 2),
              ),
              const SizedBox(width: 10),
              Text(
                'Syncing $pct%',
                style: Theme.of(context).textTheme.titleMedium,
              ),
              const Spacer(),
              if (eta.isNotEmpty)
                Text(eta, style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
          const SizedBox(height: 8),
          ClipRRect(
            borderRadius: BorderRadius.circular(4),
            child: LinearProgressIndicator(
              value: progress,
              minHeight: 6,
              backgroundColor: kPrimary.withAlpha(40),
              valueColor: const AlwaysStoppedAnimation<Color>(kPrimary),
            ),
          ),
          const SizedBox(height: 4),
          Text(
            '${widget.status.walletBlockCount} / ${widget.status.networkBlockCount}',
            style: Theme.of(context).textTheme.labelSmall,
          ),
        ],
      ),
    );
  }
}

class _Sample {
  final DateTime time;
  final int height;
  _Sample(this.time, this.height);
}
