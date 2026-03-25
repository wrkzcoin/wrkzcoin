import 'dart:async';
import 'dart:js_interop';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:web/web.dart' as web;
import '../../core/api/models/transaction.dart';
import '../../core/api/models/wallet_status.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/widgets/language_selector.dart';
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

  // ── Autosave ────────────────────────────────────────────────────────────────
  static const _autosaveInterval = Duration(minutes: 5);
  Timer? _autosaveTimer;
  bool _savedAfterSync = false;

  @override
  void initState() {
    super.initState();
  }

  @override
  void dispose() {
    _autosaveTimer?.cancel();
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

  // ── Incoming transaction notifications (browser Notification API) ──────────

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

    final notificationsEnabled = ref.read(notificationsEnabledProvider);
    for (final tx in txs) {
      if (_knownTxHashes.contains(tx.hash)) continue;
      if (tx.isIncoming && tx.isConfirmed && notificationsEnabled) {
        _showNotification(tx);
      }
    }
    _knownTxHashes.addAll(txs.map((t) => t.hash));
  }

  void _showNotification(Transaction tx) {
    final tr = S.of(context);
    final title = tr?.wrkzReceived ?? 'WRKZ Received';
    final body = tr?.youReceivedAmount(formatAmount(tx.totalAmount.abs(), showTicker: true)) ??
        'You received ${formatAmount(tx.totalAmount.abs(), showTicker: true)}';

    // Use the browser Notification API
    try {
      if (web.Notification.permission == 'granted') {
        web.Notification(
          title,
          web.NotificationOptions(body: body),
        );
      } else if (web.Notification.permission != 'denied') {
        web.Notification.requestPermission().toDart.then((permission) {
          if ((permission as JSString).toDart == 'granted') {
            web.Notification(
              title,
              web.NotificationOptions(body: body),
            );
          }
        });
      }
    } catch (_) {
      // Notification API not available — silently ignore
    }
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

  /// Width below which the sidebar collapses into a drawer.
  static const double _mobileBreakpoint = 600;

  Widget _buildSidebarContent(BuildContext context, List<_TabItem> tabs, int selected, Color surface, S? tr, {bool isDrawer = false}) {
    return Container(
      width: 200,
      color: surface,
      child: SafeArea(
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
                onTap: () {
                  if (isDrawer) Navigator.of(context).pop();
                  context.go(tab.path);
                },
              );
            }),
            const Spacer(),
            // Lock button
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
              child: InkWell(
                onTap: () {
                  if (isDrawer) Navigator.of(context).pop();
                  _lock();
                },
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
    );
  }

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

    final isNarrow = MediaQuery.sizeOf(context).width < _mobileBreakpoint;

    if (isNarrow) {
      // ── Mobile: drawer-based navigation ──────────────────────────────────
      return Scaffold(
        appBar: AppBar(
          title: Text(tabs[selected].label, style: const TextStyle(fontSize: 16)),
          backgroundColor: surface,
          leading: Builder(
            builder: (ctx) => IconButton(
              icon: const Icon(Icons.menu),
              onPressed: () => Scaffold.of(ctx).openDrawer(),
            ),
          ),
        ),
        drawer: Drawer(
          backgroundColor: surface,
          child: _buildSidebarContent(context, tabs, selected, surface, tr, isDrawer: true),
        ),
        body: widget.child,
      );
    }

    // ── Desktop: fixed sidebar ───────────────────────────────────────────────
    return Scaffold(
      body: Row(
        children: [
          _buildSidebarContent(context, tabs, selected, surface, tr),
          const VerticalDivider(width: 1),
          Expanded(child: widget.child),
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
    final host = nodeInfoAsync.valueOrNull?['daemonHost'] as String? ?? '...';
    final port = nodeInfoAsync.valueOrNull?['daemonPort'];
    final nodeStr = port != null ? '$host:$port' : host;

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
          Expanded(
            child: Text(
              nodeStr,
              style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11),
              overflow: TextOverflow.ellipsis,
            ),
          ),
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
