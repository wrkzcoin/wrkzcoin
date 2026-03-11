import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:local_notifier/local_notifier.dart';
import 'package:system_tray/system_tray.dart';
import 'package:window_manager/window_manager.dart';
import '../../core/api/models/transaction.dart';
import '../../core/api/models/wallet_status.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../core/providers/wallet_notifiers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/amount_formatter.dart';
import '../../shared/widgets/pluton_logo.dart';

class MainShell extends ConsumerStatefulWidget {
  final Widget child;
  const MainShell({super.key, required this.child});

  @override
  ConsumerState<MainShell> createState() => _MainShellState();
}

class _MainShellState extends ConsumerState<MainShell>
    with WindowListener {
  static const _tabs = [
    _TabItem(icon: Icons.dashboard_outlined, activeIcon: Icons.dashboard, label: 'Overview', path: '/overview'),
    _TabItem(icon: Icons.qr_code_outlined, activeIcon: Icons.qr_code, label: 'Receive', path: '/receive'),
    _TabItem(icon: Icons.send_outlined, activeIcon: Icons.send, label: 'Transfer', path: '/transfer'),
    _TabItem(icon: Icons.receipt_long_outlined, activeIcon: Icons.receipt_long, label: 'History', path: '/history'),
    _TabItem(icon: Icons.contacts_outlined, activeIcon: Icons.contacts, label: 'Address Book', path: '/addressbook'),
    _TabItem(icon: Icons.settings_outlined, activeIcon: Icons.settings, label: 'Settings', path: '/settings'),
    _TabItem(icon: Icons.info_outline, activeIcon: Icons.info, label: 'About', path: '/about'),
  ];

  final Set<String> _knownTxHashes = {};
  bool _firstTxLoad = true;
  Timer? _trayClickTimer;

  // ── System tray ──────────────────────────────────────────────────────────────
  final _systemTray = SystemTray();

  // ── Autosave ────────────────────────────────────────────────────────────────
  static const _autosaveInterval = Duration(minutes: 5);
  Timer? _autosaveTimer;
  bool _savedAfterSync = false;

  @override
  void initState() {
    super.initState();
    windowManager.addListener(this);
    windowManager.setPreventClose(true);
    _initSystemTray();
  }

  @override
  void dispose() {
    _trayClickTimer?.cancel();
    _autosaveTimer?.cancel();
    windowManager.setPreventClose(false);
    windowManager.removeListener(this);
    super.dispose();
  }

  Future<void> _initSystemTray() async {
    try {
      await _systemTray.initSystemTray(
        title: 'PLUTON Wallet',
        iconPath: 'assets/images/app_icon.ico',
        toolTip: 'PLUTON Wallet',
      );

      final menu = Menu();
      await menu.buildFrom([
        MenuItemLabel(label: 'Show', onClicked: (_) => _showWindow()),
        MenuSeparator(),
        MenuItemLabel(label: 'Exit', onClicked: (_) {
          windowManager.setPreventClose(false);
          windowManager.close();
        }),
      ]);
      await _systemTray.setContextMenu(menu);

      _systemTray.registerSystemTrayEventHandler((eventName) {
        if (eventName == kSystemTrayEventClick) {
          // Single left click → show window
          if (_trayClickTimer?.isActive ?? false) {
            // Second click within threshold → double-click: maximize
            _trayClickTimer!.cancel();
            _trayClickTimer = null;
            _showWindow(maximize: true);
          } else {
            _trayClickTimer = Timer(const Duration(milliseconds: 350), () {
              _showWindow();
              _trayClickTimer = null;
            });
          }
        } else if (eventName == kSystemTrayEventRightClick) {
          _systemTray.popUpContextMenu();
        }
      });
    } catch (e) {
      debugPrint('[tray] init failed: $e');
    }
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

  // ── Window helpers ─────────────────────────────────────────────────────────

  Future<void> _showWindow({bool maximize = false}) async {
    await windowManager.show();
    if (maximize) await windowManager.maximize();
    // Windows restricts SetForegroundWindow from background processes.
    // Briefly setting alwaysOnTop forces the window to the front reliably.
    await windowManager.setAlwaysOnTop(true);
    await windowManager.focus();
    await windowManager.setAlwaysOnTop(false);
  }

  // ── Window events ─────────────────────────────────────────────────────────────

  @override
  Future<void> onWindowMinimize() async => windowManager.hide();

  @override
  Future<void> onWindowClose() async => windowManager.hide();

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
    final notification = LocalNotification(
      title: 'WRKZ Received',
      body: 'You received ${formatAmount(tx.totalAmount.abs(), showTicker: true)}',
    );
    notification.show();
  }

  // ── Helpers ───────────────────────────────────────────────────────────────────

  int _selectedIndex(BuildContext context) {
    final location = GoRouterState.of(context).matchedLocation;
    final idx = _tabs.indexWhere((t) => location.startsWith(t.path));
    return idx < 0 ? 0 : idx;
  }

  void _lock() => ref.read(walletLockedProvider.notifier).state = true;

  @override
  Widget build(BuildContext context) {
    ref.listen(transactionsProvider, _onTxUpdate);
    ref.listen(statusProvider, _onSyncStatusChange);

    final selected = _selectedIndex(context);
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
                ...List.generate(_tabs.length, (i) {
                  final tab = _tabs[i];
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
                          Text('Lock Wallet', style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 14)),
                        ],
                      ),
                    ),
                  ),
                ),
                const Divider(height: 1),
                // ── Node status footer ────────────────────────────────────────
                _NodeStatusFooter(),
              ],
            ),
          ),
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
    final host = nodeInfoAsync.valueOrNull?['daemonHost'] as String? ?? '…';
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
