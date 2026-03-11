import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../core/providers/providers.dart';
import '../core/providers/app_providers.dart';
import '../features/setup/setup_screen.dart';
import '../features/lock/lock_screen.dart';
import '../features/shell/main_shell.dart';
import '../features/overview/overview_screen.dart';
import '../features/receive/receive_screen.dart';
import '../features/transfer/transfer_screen.dart';
import '../features/history/history_screen.dart';
import '../features/settings/settings_screen.dart';
import '../features/addressbook/address_book_screen.dart';
import '../features/about/about_screen.dart';

// Thin ChangeNotifier that holds auth state and triggers GoRouter's
// refreshListenable when either walletOpen or walletLocked changes.
// Using ref.listen (not ref.watch) keeps the routerProvider from being
// recreated — which would cause "ref used after dependency changed" errors.
class _AuthNotifier extends ChangeNotifier {
  bool walletOpen = false;
  bool walletLocked = false;
}

final routerProvider = Provider<GoRouter>((ref) {
  final auth = _AuthNotifier()
    ..walletOpen = ref.read(walletOpenProvider)
    ..walletLocked = ref.read(walletLockedProvider);

  ref.listen<bool>(walletOpenProvider, (_, next) {
    auth.walletOpen = next;
    auth.notifyListeners();
  });
  ref.listen<bool>(walletLockedProvider, (_, next) {
    auth.walletLocked = next;
    auth.notifyListeners();
  });

  ref.onDispose(auth.dispose);

  return GoRouter(
    initialLocation: auth.walletOpen ? '/overview' : '/setup',
    refreshListenable: auth,
    redirect: (context, state) {
      final open = auth.walletOpen;
      final locked = auth.walletLocked;
      final loc = state.matchedLocation;
      if (!open && loc != '/setup') return '/setup';
      if (open && locked && loc != '/lock') return '/lock';
      if (open && !locked && (loc == '/setup' || loc == '/lock')) return '/overview';
      return null;
    },
    routes: [
      GoRoute(
        path: '/setup',
        builder: (_, _) => const SetupScreen(),
      ),
      GoRoute(
        path: '/lock',
        builder: (_, _) => const LockScreen(),
      ),
      ShellRoute(
        builder: (context, state, child) => MainShell(child: child),
        routes: [
          GoRoute(
            path: '/overview',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: OverviewScreen()),
          ),
          GoRoute(
            path: '/receive',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: ReceiveScreen()),
          ),
          GoRoute(
            path: '/transfer',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: TransferScreen()),
          ),
          GoRoute(
            path: '/history',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: HistoryScreen()),
          ),
          GoRoute(
            path: '/addressbook',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: AddressBookScreen()),
          ),
          GoRoute(
            path: '/settings',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: SettingsScreen()),
          ),
          GoRoute(
            path: '/about',
            pageBuilder: (_, _) =>
                const NoTransitionPage(child: AboutScreen()),
          ),
        ],
      ),
    ],
  );
});
