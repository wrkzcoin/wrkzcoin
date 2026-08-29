import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../core/providers/providers.dart';
import '../features/history/history_screen.dart';
import '../features/language_picker/language_picker_screen.dart';
import '../features/lock/lock_screen.dart';
import '../features/overview/overview_screen.dart';
import '../features/receive/receive_screen.dart';
import '../features/settings/settings_screen.dart';
import '../features/setup/setup_screen.dart';
import '../features/shell/main_shell.dart';
import '../features/splash/splash_screen.dart';
import '../features/transfer/transfer_screen.dart';
import '../features/wallet_picker/wallet_picker_screen.dart';

/// Route guard — reacts to wallet open/locked state changes.
class _AuthNotifier extends ChangeNotifier {
  bool _walletOpen = false;
  bool _walletLocked = false;

  set walletOpen(bool v) {
    _walletOpen = v;
    notifyListeners();
  }

  set walletLocked(bool v) {
    _walletLocked = v;
    notifyListeners();
  }
}

final routerProvider = Provider<GoRouter>((ref) {
  final auth = _AuthNotifier()
    .._walletOpen = ref.read(walletOpenProvider)
    .._walletLocked = ref.read(walletLockedProvider);

  ref.listen<bool>(walletOpenProvider, (_, next) {
    auth.walletOpen = next;
  });
  ref.listen<bool>(walletLockedProvider, (_, next) {
    auth.walletLocked = next;
  });

  ref.onDispose(auth.dispose);

  return GoRouter(
    initialLocation: '/splash',
    refreshListenable: auth,
    redirect: (context, state) {
      final loc = state.uri.path;
      final open = auth._walletOpen;
      final locked = auth._walletLocked;

      // Allow splash, picker, setup, and language picker without redirect.
      if (loc == '/splash' || loc == '/language') return null;
      if (loc == '/picker' || loc == '/setup') {
        if (open && !locked) return '/overview';
        return null;
      }

      // Must have wallet open for main routes (lock is allowed for password entry).
      if (!open && loc != '/picker' && loc != '/setup' && loc != '/lock') {
        return '/picker';
      }

      // If locked, force lock screen.
      if (open && locked && loc != '/lock') return '/lock';

      // If unlocked and on lock/picker/setup, go to overview.
      if (open && !locked && (loc == '/lock')) return '/overview';

      return null;
    },
    routes: [
      GoRoute(
        path: '/splash',
        pageBuilder: (_, state) => const NoTransitionPage(
          child: SplashScreen(),
        ),
      ),
      GoRoute(
        path: '/language',
        pageBuilder: (_, state) => const NoTransitionPage(
          child: LanguagePickerScreen(),
        ),
      ),
      GoRoute(
        path: '/picker',
        pageBuilder: (_, state) => const NoTransitionPage(
          child: WalletPickerScreen(),
        ),
      ),
      GoRoute(
        path: '/setup',
        pageBuilder: (_, state) => const NoTransitionPage(
          child: SetupScreen(),
        ),
      ),
      GoRoute(
        path: '/lock',
        pageBuilder: (_, state) => const NoTransitionPage(
          child: LockScreen(),
        ),
      ),
      // Main app — shell with bottom navigation.
      ShellRoute(
        builder: (_, __, child) => MainShell(child: child),
        routes: [
          GoRoute(
            path: '/overview',
            pageBuilder: (_, state) => const NoTransitionPage(
              child: OverviewScreen(),
            ),
          ),
          GoRoute(
            path: '/receive',
            pageBuilder: (_, state) => const NoTransitionPage(
              child: ReceiveScreen(),
            ),
          ),
          GoRoute(
            path: '/transfer',
            pageBuilder: (_, state) => const NoTransitionPage(
              child: TransferScreen(),
            ),
          ),
          GoRoute(
            path: '/history',
            pageBuilder: (_, state) => const NoTransitionPage(
              child: HistoryScreen(),
            ),
          ),
          GoRoute(
            path: '/settings',
            pageBuilder: (_, state) => const NoTransitionPage(
              child: SettingsScreen(),
            ),
          ),
        ],
      ),
    ],
  );
});
