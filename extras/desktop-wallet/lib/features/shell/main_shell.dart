import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:local_notifier/local_notifier.dart';
import 'app_lifecycle.dart';
import '../../core/api/models/transaction.dart';
import '../../core/api/models/wallet_status.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/widgets/language_selector.dart';
import '../../shared/widgets/lite_node_banner.dart';
import '../../shared/widgets/pluton_logo.dart';

class MainShell extends ConsumerStatefulWidget {
  final Widget child;
  const MainShell({super.key, required this.child});

  @override
  ConsumerState<MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<MainShell> {
  final Set<String> _knownTxHashes = {};
  bool _firstTxLoad = true;

  /* The most toasts one transaction update may raise. Each show() is a
     blocking call out to the platform's notification service, so this is what
     stands between a burst of transactions and a frozen window. */
  static const _kMaxNotificationsPerUpdate = 5;

  // ── Autosave ────────────────────────────────────────────────────────────────
  static const _autosaveInterval = Duration(minutes: 5);
  Timer? _autosaveTimer;
  bool _savedAfterSync = false;

  /* Every toast we raise, so we can take them back down. Nothing here ever
     closed one, and on Windows a notification outlives the process that
     registered it - which is how a backlog of them carried on popping up in
     the tray after the app had already quit. */
  final List<LocalNotification> _liveNotifications = [];

  @override
  void initState() {
    super.initState();
    /* The app ends at exit() inside forceQuitNow, which runs no dispose(), so
       the quit path has to be able to reach these directly. */
    takeDownNotifications = _takeDownNotifications;
  }

  Future<void> _takeDownNotifications() async {
    for (final n in _liveNotifications) {
      try {
        await n.destroy();
      } catch (_) {
        // Already gone, or the shell is not answering.
      }
    }
    _liveNotifications.clear();
  }

  @override
  void dispose() {
    _autosaveTimer?.cancel();
    takeDownNotifications = () async {};
    unawaited(_takeDownNotifications());
    super.dispose();
  }

  // ── Autosave logic ──────────────────────────────────────────────────────────

  void _onSyncStatusChange(
    AsyncValue<WalletStatus>? prev,
    AsyncValue<WalletStatus> next,
  ) {
    final status = next.valueOrNull;
    if (status == null) return;
    final autosaveOn = ref.read(autosaveEnabledProvider);
    if (!autosaveOn) {
      _autosaveTimer?.cancel();
      _autosaveTimer = null;
      // Clear the latch too, so switching autosave back on restarts the timer
      // instead of leaving it off until the next app launch.
      _savedAfterSync = false;
      return;
    }

    if (status.isWalletSynced && !_savedAfterSync) {
      // First save right after sync completes
      _savedAfterSync = true;
      _doAutosave();
      // Start periodic timer
      _autosaveTimer?.cancel();
      _autosaveTimer = Timer.periodic(_autosaveInterval, (_) => _doAutosave());
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

  // ── Incoming transaction notifications ────────────────────────────────────────

  void _onTxUpdate(
    AsyncValue<List<Transaction>>? prev,
    AsyncValue<List<Transaction>> next,
  ) {
    final txs = next.valueOrNull;
    if (txs == null) return;

    if (_firstTxLoad) {
      // Seed known hashes on first load — don't notify for existing txs
      _knownTxHashes.addAll(txs.map((t) => t.hash));
      _firstTxLoad = false;
      return;
    }

    /* A syncing wallet meets its whole history a chunk at a time. Those are old
       transactions arriving late, not money coming in now, and raising a toast
       for each one puts one blocking platform-channel call per transaction on
       the UI isolate - a wallet with thousands of them freezes the app until
       the queue drains. Learn them silently and only speak up once the wallet
       has caught up and a new transaction really is new. */
    final synced = ref.read(statusProvider).valueOrNull?.isWalletSynced ?? false;
    final notificationsEnabled = ref.read(notificationsEnabledProvider);

    var shown = 0;

    for (final tx in txs) {
      if (_knownTxHashes.contains(tx.hash)) continue;
      _knownTxHashes.add(tx.hash);

      if (!synced || !tx.isIncoming || !notificationsEnabled) continue;

      /* Even a synced wallet can be handed a burst at once - a reconnect, a
         rescan, or a block that pays it many times over. Cap what a single
         update is allowed to raise so the UI thread survives any of them. */
      if (shown >= _kMaxNotificationsPerUpdate) continue;

      shown++;
      _showNotification(tx);
    }
  }

  void _showNotification(Transaction tx) {
    final tr = S.of(context);
    final notification = LocalNotification(
      title: tr?.wrkzReceived ?? 'WRKZ Received',
      body: tr?.youReceivedAmount(formatAmount(tx.totalAmount.abs(), showTicker: true)) ?? 'You received ${formatAmount(tx.totalAmount.abs(), showTicker: true)}',
    );
    /* Keep only a short tail. These are for tearing down what is still on
       screen at exit, not a log - an unbounded list would be one more thing
       growing with the session. */
    _liveNotifications.add(notification);
    if (_liveNotifications.length > _kMaxNotificationsPerUpdate) {
      _liveNotifications.removeAt(0).destroy();
    }

    notification.show();
  }

  // ── Helpers ───────────────────────────────────────────────────────────────────

  List<_TabItem> _localizedTabs(S? tr) => [
    _TabItem(icon: Icons.dashboard_outlined, activeIcon: Icons.dashboard, label: tr?.tabOverview ?? 'Overview', path: '/overview'),
    _TabItem(icon: Icons.qr_code_outlined, activeIcon: Icons.qr_code, label: tr?.tabReceive ?? 'Receive', path: '/receive'),
    _TabItem(icon: Icons.send_outlined, activeIcon: Icons.send, label: tr?.tabTransfer ?? 'Transfer', path: '/transfer'),
    _TabItem(icon: Icons.receipt_long_outlined, activeIcon: Icons.receipt_long, label: tr?.tabHistory ?? 'History', path: '/history'),
    _TabItem(icon: Icons.contacts_outlined, activeIcon: Icons.contacts, label: tr?.tabAddressBook ?? 'Address Book', path: '/addressbook'),
    _TabItem(icon: Icons.settings_outlined, activeIcon: Icons.settings, label: tr?.tabSettings ?? 'Settings', path: '/settings'),
    _TabItem(icon: Icons.info_outline, activeIcon: Icons.info, label: tr?.tabAbout ?? 'About', path: '/about'),
  ];

  int _selectedIndex(BuildContext context, List<_TabItem> tabs) {
    final location = GoRouterState.of(context).matchedLocation;
    final idx = tabs.indexWhere((t) => location.startsWith(t.path));
    return idx < 0 ? 0 : idx;
  }

  void _lock() => ref.read(walletLockedProvider.notifier).state = true;

  @override
  Widget build(BuildContext context) {
    ref.listen(transactionsProvider, _onTxUpdate);
    ref.listen(statusProvider, _onSyncStatusChange);

    final tr = S.of(context);
    final tabs = _localizedTabs(tr);
    final selected = _selectedIndex(context, tabs);
    final surface = Theme.of(context).brightness == Brightness.dark
        ? kSurface
        : kSurfaceLight;

    return Scaffold(
      body: Row(
        children: [
          // ── Navigation Rail ─────────────────────────────────────────────────
          Container(
            width: 200,
            color: surface,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Padding(
                  padding: EdgeInsets.fromLTRB(16, 24, 16, 20),
                  child: PlutonLogo(),
                ),
                const Divider(height: 1),
                const SizedBox(height: 8),
                ...List.generate(tabs.length, (i) {
                  final tab = tabs[i];
                  final active = selected == i;
                  return _NavItem(
                    icon: active ? tab.activeIcon : tab.icon,
                    label: tab.label,
                    selected: active,
                    onTap: () => context.go(tab.path),
                  );
                }),
                const Spacer(),
                // Lock button
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                  child: InkWell(
                    onTap: _lock,
                    borderRadius: BorderRadius.circular(8),
                    child: Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
                      child: Row(
                        children: [
                          Icon(Icons.lock_outline, size: 18, color: Theme.of(context).colorScheme.onSurfaceVariant),
                          const SizedBox(width: 12),
                          Text(tr?.lockWallet ?? 'Lock Wallet', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 14)),
                        ],
                      ),
                    ),
                  ),
                ),
                const Divider(height: 1),
                // ── Node status footer + language switcher ────────────────────
                _NodeStatusFooter(),
              ],
            ),
          ),
          const VerticalDivider(width: 1),
          Expanded(
            child: Column(
              children: [
                // Lite-node notice lives in the shell so it is on every
                // screen, and stays up once the wallet is synced — which is
                // exactly when a balance missing its older half looks most
                // trustworthy. See LITENODE.md.
                Consumer(
                  builder: (context, ref, _) {
                    final status = ref.watch(statusProvider).valueOrNull;
                    if (status == null) return const SizedBox.shrink();
                    return LiteNodeBanner(status: status);
                  },
                ),
                Expanded(child: widget.child),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

// ── Node status footer ─────────────────────────────────────────────────────────

class _NodeStatusFooter extends ConsumerWidget {
  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final nodeInfoAsync = ref.watch(nodeInfoProvider);

    final isOnline = nodeInfoAsync.valueOrNull?['daemonOnline'] as bool? ?? false;
    final host = nodeInfoAsync.valueOrNull?['daemonHost'] as String? ?? '…';
    final port = nodeInfoAsync.valueOrNull?['daemonPort'];

    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 8, 8, 12),
      child: Row(
        children: [
          // Connection dot
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              color: nodeInfoAsync.isLoading
                  ? Theme.of(context).colorScheme.onSurfaceVariant
                  : (isOnline ? kSuccess : kError),
              shape: BoxShape.circle,
            ),
          ),
          const SizedBox(width: 6),
          // The port is the half that matters when two nodes differ only by
          // it, and it is the half an ellipsis eats. Keep it, and let the
          // hostname be the part that gives way.
          Flexible(
            child: Text(
              host,
              style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11),
              overflow: TextOverflow.ellipsis,
              softWrap: false,
            ),
          ),
          if (port != null)
            Text(
              ':$port',
              style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11),
            ),
          const Spacer(),
          // Language switcher
          const LanguageSelectorButton(),
          const SizedBox(width: 2),
          // Refresh button
          InkWell(
            onTap: () {
              ref.invalidate(statusProvider);
              ref.invalidate(balanceProvider);
              ref.invalidate(transactionsProvider);
              ref.invalidate(nodeInfoProvider);
            },
            borderRadius: BorderRadius.circular(4),
            child: Padding(
              padding: const EdgeInsets.all(4),
              child: Icon(Icons.refresh, size: 14, color: Theme.of(context).colorScheme.onSurfaceVariant),
            ),
          ),
        ],
      ),
    );
  }
}

// ── Private widgets ────────────────────────────────────────────────────────────

class _NavItem extends StatelessWidget {
  final IconData icon;
  final String label;
  final bool selected;
  final VoidCallback onTap;

  const _NavItem({required this.icon, required this.label, required this.selected, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(8),
      child: Container(
        margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
        decoration: BoxDecoration(
          color: selected ? kPrimary.withAlpha(30) : Colors.transparent,
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          children: [
            Icon(icon, size: 20, color: selected ? kPrimary : Theme.of(context).colorScheme.onSurfaceVariant),
            const SizedBox(width: 12),
            Text(
              label,
              style: TextStyle(
                color: selected ? kPrimary : Theme.of(context).colorScheme.onSurfaceVariant,
                fontWeight: selected ? FontWeight.w600 : FontWeight.normal,
                fontSize: 14,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _TabItem {
  final IconData icon;
  final IconData activeIcon;
  final String label;
  final String path;
  const _TabItem({required this.icon, required this.activeIcon, required this.label, required this.path});
}
