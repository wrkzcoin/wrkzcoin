import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/api/models/transaction.dart';
import '../../core/api/models/wallet_status.dart';
import '../../core/config/app_config.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/utils/haptics.dart';
import '../../shared/widgets/language_selector.dart';

class MainShell extends ConsumerStatefulWidget {
  final Widget child;

  const MainShell({super.key, required this.child});

  @override
  ConsumerState<MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<MainShell>
    with WidgetsBindingObserver {
  // Auto-lock
  DateTime? _pausedAt;

  // Autosave
  bool _savedAfterSync = false;
  Timer? _autosaveTimer;

  // Tx notifications
  final Set<String> _knownTxHashes = {};
  bool _firstTxLoad = true;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _autosaveTimer?.cancel();
    super.dispose();
  }

  // ── app lifecycle (auto-lock) ──────────────────────────────────────────

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.paused ||
        state == AppLifecycleState.inactive) {
      _pausedAt = DateTime.now();
    } else if (state == AppLifecycleState.resumed) {
      _checkAutoLock();
    }
  }

  void _checkAutoLock() {
    if (_pausedAt == null) return;
    final autoLockIdx = ref.read(autoLockIndexProvider);
    final option = AppConfig.autoLockOptions[autoLockIdx];
    if (option.duration == null) return; // "Never"

    final elapsed = DateTime.now().difference(_pausedAt!);
    if (elapsed >= option.duration!) {
      ref.read(walletLockedProvider.notifier).state = true;
      if (mounted) context.go('/lock');
    }
    _pausedAt = null;
  }

  // ── sync status listener (autosave + notifications) ────────────────────

  void _onSyncStatusChange(
      AsyncValue<WalletStatus>? prev, AsyncValue<WalletStatus> next) {
    final status = next.valueOrNull;
    if (status == null) return;

    if (status.isWalletSynced && !_savedAfterSync) {
      _savedAfterSync = true;
      _doAutosave();
      _autosaveTimer?.cancel();
      _autosaveTimer = Timer.periodic(
        AppConfig.autosaveInterval,
        (_) => _doAutosave(),
      );
    }
  }

  Future<void> _doAutosave() async {
    if (!ref.read(autosaveEnabledProvider)) return;
    try {
      await ref.read(walletCApiProvider).save();
      debugPrint('[autosave] wallet saved');
    } catch (e) {
      debugPrint('[autosave] failed: $e');
    }
  }

  void _onTxUpdate(AsyncValue<List<Transaction>>? prev,
      AsyncValue<List<Transaction>> next) {
    final txs = next.valueOrNull;
    if (txs == null) return;

    if (_firstTxLoad) {
      _knownTxHashes.addAll(txs.map((t) => t.hash));
      _firstTxLoad = false;
      return;
    }

    final notificationsOn = ref.read(notificationsEnabledProvider);
    for (final tx in txs) {
      if (_knownTxHashes.contains(tx.hash)) continue;
      if (tx.isIncoming && tx.isConfirmed && notificationsOn) {
        hapticHeavy();
        // TODO: flutter_local_notifications for background notification
      }
    }
    _knownTxHashes.addAll(txs.map((t) => t.hash));
  }

  // ── build ──────────────────────────────────────────────────────────────

  int _currentIndex(BuildContext context) {
    final loc = GoRouterState.of(context).uri.path;
    final paths = ['/overview', '/receive', '/transfer', '/history', '/settings'];
    for (var i = 0; i < paths.length; i++) {
      if (loc == paths[i]) return i;
    }
    return 0;
  }

  @override
  Widget build(BuildContext context) {
    // Listen for sync status changes (autosave).
    ref.listen(statusProvider, _onSyncStatusChange);
    // Listen for new transactions (notifications).
    ref.listen(transactionsProvider, _onTxUpdate);

    final idx = _currentIndex(context);
    final tr = S.of(context)!;
    final tabLabels = [
      tr.tabOverview,
      tr.tabReceive,
      tr.tabSend,
      tr.tabHistory,
      tr.tabSettings,
    ];

    return Scaffold(
      appBar: AppBar(
        title: Text(tabLabels[idx]),
        actions: const [
          LanguageSelectorButton(),
        ],
      ),
      body: widget.child,
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: idx,
        onTap: (i) {
          if (i != idx) {
            hapticSelection();
            final paths = ['/overview', '/receive', '/transfer', '/history', '/settings'];
            context.go(paths[i]);
          }
        },
        items: [
          BottomNavigationBarItem(
            icon: const Icon(Icons.dashboard_outlined),
            activeIcon: const Icon(Icons.dashboard),
            label: tr.tabOverview,
          ),
          BottomNavigationBarItem(
            icon: const Icon(Icons.qr_code_outlined),
            activeIcon: const Icon(Icons.qr_code),
            label: tr.tabReceive,
          ),
          BottomNavigationBarItem(
            icon: const Icon(Icons.send_outlined),
            activeIcon: const Icon(Icons.send),
            label: tr.tabSend,
          ),
          BottomNavigationBarItem(
            icon: const Icon(Icons.history_outlined),
            activeIcon: const Icon(Icons.history),
            label: tr.tabHistory,
          ),
          BottomNavigationBarItem(
            icon: const Icon(Icons.settings_outlined),
            activeIcon: const Icon(Icons.settings),
            label: tr.tabSettings,
          ),
        ],
      ),
    );
  }
}
