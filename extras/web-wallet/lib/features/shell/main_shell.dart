import 'dart:async';
import 'dart:js_interop';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:web/web.dart' as web;
import '../../core/api/models/transaction.dart';
import '../../core/api/models/wallet_status.dart';
import '../../core/config/app_config.dart';
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

class _MainShellState extends ConsumerState<MainShell> with WidgetsBindingObserver {
  /// Hashes already seen, so a transaction only notifies once.
  ///
  /// Capped — this used to grow without bound for the life of the session on a
  /// wallet with a large history.
  final _knownTxHashes = <String>{};
  static const _maxKnownHashes = 2000;
  bool _firstTxLoad = true;

  // ── Autosave ────────────────────────────────────────────────────────────────
  Timer? _autosaveTimer;
  Duration? _autosaveInterval;

  // ── Idle auto-lock ──────────────────────────────────────────────────────────
  Timer? _idleTimer;

  // Browser event listeners, retained so they can be removed on dispose.
  JSFunction? _onPageHide;
  JSFunction? _onVisibilityChange;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _installBrowserHooks();
    _resetIdleTimer();
  }

  @override
  void dispose() {
    _autosaveTimer?.cancel();
    _idleTimer?.cancel();
    _removeBrowserHooks();
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  // ── Browser lifecycle ───────────────────────────────────────────────────────

  void _installBrowserHooks() {
    // Closing the tab used to discard everything since the last 5-minute
    // autosave. `pagehide` is the last reliable point to flush on the web.
    _onPageHide = ((JSAny _) {
      final api = ref.read(walletCApiProvider);
      if (api.isOpen) unawaited(api.save());
    }).toJS;
    web.window.addEventListener('pagehide', _onPageHide);

    // Stop polling the worker while the tab is in the background.
    _onVisibilityChange = ((JSAny _) {
      final visible = web.document.visibilityState == 'visible';
      ref.read(appActiveProvider.notifier).state =
          visible && !ref.read(walletLockedProvider);
      if (visible) {
        _resetIdleTimer();
        unawaited(refreshAllWalletData(ref));
      }
    }).toJS;
    web.document.addEventListener('visibilitychange', _onVisibilityChange);
  }

  void _removeBrowserHooks() {
    if (_onPageHide != null) {
      web.window.removeEventListener('pagehide', _onPageHide);
      _onPageHide = null;
    }
    if (_onVisibilityChange != null) {
      web.document.removeEventListener('visibilitychange', _onVisibilityChange);
      _onVisibilityChange = null;
    }
  }

  // ── Idle auto-lock ──────────────────────────────────────────────────────────

  void _resetIdleTimer() {
    _idleTimer?.cancel();
    final minutes = ref.read(autoLockMinutesProvider);
    if (minutes <= 0) return;
    _idleTimer = Timer(Duration(minutes: minutes), () {
      if (!mounted) return;
      if (ref.read(walletLockedProvider)) return;
      _lock();
    });
  }

  // ── Autosave logic ──────────────────────────────────────────────────────────

  void _onSyncStatusChange(
    AsyncValue<WalletStatus>? prev,
    AsyncValue<WalletStatus> next,
  ) {
    final status = next.valueOrNull;
    if (status == null) return;

    if (!ref.read(autosaveEnabledProvider)) {
      _autosaveTimer?.cancel();
      _autosaveTimer = null;
      _autosaveInterval = null;
      return;
    }

    // Save more often while the initial scan is running. Waiting for sync to
    // complete before the first save meant closing the tab mid-sync threw away
    // every block scanned so far.
    final wanted =
        status.isWalletSynced ? kAutosaveInterval : kSyncingAutosaveInterval;
    if (_autosaveInterval != wanted || _autosaveTimer == null) {
      _autosaveInterval = wanted;
      _autosaveTimer?.cancel();
      _autosaveTimer = Timer.periodic(wanted, (_) => unawaited(_doAutosave()));
    }

    // Flush once as soon as the wallet reports it has caught up.
    if (status.isWalletSynced && (prev?.valueOrNull?.isWalletSynced ?? false) == false) {
      unawaited(_doAutosave());
    }
  }

  Future<void> _doAutosave() async {
    if (!mounted) return;
    if (!ref.read(autosaveEnabledProvider)) return;
    final api = ref.read(walletCApiProvider);
    if (!api.isOpen) return;
    try {
      await api.save();
    } catch (e) {
      // A failure here means browser storage rejected the write (quota, or
      // eviction). Tell the user — silently continuing is how people lose a
      // wallet they believed was saved.
      if (!mounted) return;
      debugPrint('[autosave] failed: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          backgroundColor: kError,
          duration: const Duration(seconds: 8),
          content: Text(S.of(context)?.autosaveFailed(e.toString()) ??
              'Could not save the wallet to browser storage: $e'),
        ),
      );
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
      _rememberHashes(txs);
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
    _rememberHashes(txs);
  }

  void _rememberHashes(List<Transaction> txs) {
    _knownTxHashes.addAll(txs.map((t) => t.hash));
    if (_knownTxHashes.length > _maxKnownHashes) {
      // Keep the most recent window; anything older cannot arrive again.
      final keep = txs.take(_maxKnownHashes).map((t) => t.hash).toSet();
      _knownTxHashes
        ..clear()
        ..addAll(keep);
    }
  }

  void _showNotification(Transaction tx) {
    final tr = S.of(context);
    final title = tr?.wrkzReceived ?? 'WRKZ Received';
    final body = tr?.youReceivedAmount(formatAmount(tx.totalAmount.abs(), showTicker: true)) ??
        'You received ${formatAmount(tx.totalAmount.abs(), showTicker: true)}';

    // Only post when permission is already granted. Requesting it here — in
    // response to an incoming transaction rather than a user gesture — is
    // blocked or auto-denied by modern browsers. The Settings toggle asks.
    try {
      if (web.Notification.permission == 'granted') {
        web.Notification(title, web.NotificationOptions(body: body));
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

  void _lock() {
    // Locking must actually stop background work, not just swap the route —
    // otherwise the wallet keeps syncing and polling behind the lock screen.
    ref.read(appActiveProvider.notifier).state = false;
    ref.read(walletLockedProvider.notifier).state = true;
    final api = ref.read(walletCApiProvider);
    if (api.isOpen) unawaited(api.save());
  }

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
            const _NodeStatusFooter(),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    ref.listen(transactionsProvider, _onTxUpdate);
    ref.listen(statusProvider, _onSyncStatusChange);
    // Re-arm the idle timer whenever the user changes the setting.
    ref.listen<int>(autoLockMinutesProvider, (_, _) => _resetIdleTimer());

    final tr = S.of(context);
    final tabs = _localizedTabs(tr);
    final selected = _selectedIndex(context, tabs);
    final surface = context.surfaceColor;

    final isNarrow = MediaQuery.sizeOf(context).width < _mobileBreakpoint;

    // Any pointer or key activity postpones the auto-lock.
    return Listener(
      onPointerDown: (_) => _resetIdleTimer(),
      onPointerSignal: (_) => _resetIdleTimer(),
      // A pure listener: it must never take focus itself or appear in tab
      // order, or it would steal keystrokes from the form fields below it.
      child: Focus(
        canRequestFocus: false,
        skipTraversal: true,
        onKeyEvent: (_, _) {
          _resetIdleTimer();
          return KeyEventResult.ignored;
        },
        child: isNarrow
            ? _buildNarrow(context, tabs, selected, surface, tr)
            : _buildWide(context, tabs, selected, surface, tr),
      ),
    );
  }

  Widget _buildNarrow(BuildContext context, List<_TabItem> tabs, int selected,
      Color surface, S? tr) {
    return Scaffold(
      appBar: AppBar(
        title: Text(tabs[selected].label, style: const TextStyle(fontSize: 16)),
        backgroundColor: surface,
        leading: Builder(
          builder: (ctx) => IconButton(
            icon: const Icon(Icons.menu),
            tooltip: MaterialLocalizations.of(ctx).openAppDrawerTooltip,
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

  Widget _buildWide(BuildContext context, List<_TabItem> tabs, int selected,
      Color surface, S? tr) {
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
  const _NodeStatusFooter();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tr = S.of(context);
    final nodeInfoAsync = ref.watch(nodeInfoProvider);

    final isOnline = nodeInfoAsync.valueOrNull?['daemonOnline'] as bool? ?? false;
    final host = nodeInfoAsync.valueOrNull?['daemonHost'] as String? ?? '…';
    final port = nodeInfoAsync.valueOrNull?['daemonPort'];
    final nodeStr = port != null ? '$host:$port' : host;

    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 8, 8, 12),
      child: Row(
        children: [
          // Connection dot
          Tooltip(
            message: isOnline
                ? (tr?.synced ?? 'Connected')
                : (tr?.nodeConnectionIssue ?? 'Node unreachable'),
            child: Container(
              width: 8,
              height: 8,
              decoration: BoxDecoration(
                color: nodeInfoAsync.isLoading
                    ? Theme.of(context).colorScheme.onSurfaceVariant
                    : (isOnline ? kSuccess : kError),
                shape: BoxShape.circle,
              ),
            ),
          ),
          const SizedBox(width: 6),
          Expanded(
            child: Tooltip(
              message: nodeStr,
              child: Text(
                nodeStr,
                style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11),
                overflow: TextOverflow.ellipsis,
              ),
            ),
          ),
          // Language switcher
          const LanguageSelectorButton(),
          const SizedBox(width: 2),
          // Refresh button
          IconButton(
            icon: const Icon(Icons.refresh, size: 14),
            tooltip: tr?.refresh ?? 'Refresh',
            visualDensity: VisualDensity.compact,
            padding: const EdgeInsets.all(4),
            constraints: const BoxConstraints(minWidth: 28, minHeight: 28),
            color: Theme.of(context).colorScheme.onSurfaceVariant,
            onPressed: () => unawaited(refreshAllWalletData(ref)),
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
            Expanded(
              child: Text(
                label,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(
                  color: selected ? kPrimary : Theme.of(context).colorScheme.onSurfaceVariant,
                  fontWeight: selected ? FontWeight.w600 : FontWeight.normal,
                  fontSize: 14,
                ),
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
