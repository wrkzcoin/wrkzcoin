import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import '../core/providers/providers.dart';
import '../core/providers/app_providers.dart';
import '../features/language_picker/language_picker_screen.dart';
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
import '../l10n/generated/app_localizations.dart';

// Thin ChangeNotifier that holds auth state and triggers GoRouter's
// refreshListenable when either walletOpen or walletLocked changes.
// Using ref.listen (not ref.watch) keeps the routerProvider from being
// recreated — which would cause "ref used after dependency changed" errors.
class _AuthNotifier extends ChangeNotifier {
  bool walletOpen = false;
  bool walletLocked = false;
  bool firstLaunchDone = true;

  /// notifyListeners is protected on ChangeNotifier; expose an explicit
  /// mutator so the provider does not have to reach through the guard.
  void update({bool? open, bool? locked, bool? firstLaunch}) {
    var changed = false;
    if (open != null && open != walletOpen) {
      walletOpen = open;
      changed = true;
    }
    if (locked != null && locked != walletLocked) {
      walletLocked = locked;
      changed = true;
    }
    if (firstLaunch != null && firstLaunch != firstLaunchDone) {
      firstLaunchDone = firstLaunch;
      changed = true;
    }
    if (changed) notifyListeners();
  }
}

final routerProvider = Provider<GoRouter>((ref) {
  // Preferences are preloaded, so this is a plain bool now — no more guessing
  // an initial route from an unresolved future and correcting it a frame later.
  final firstLaunchDone = ref.read(firstLaunchDoneProvider);

  final auth = _AuthNotifier()
    ..walletOpen = ref.read(walletOpenProvider)
    ..walletLocked = ref.read(walletLockedProvider)
    ..firstLaunchDone = firstLaunchDone;

  ref.listen<bool>(walletOpenProvider, (_, next) => auth.update(open: next));
  ref.listen<bool>(walletLockedProvider, (_, next) => auth.update(locked: next));
  ref.listen<bool>(firstLaunchDoneProvider, (_, next) => auth.update(firstLaunch: next));

  ref.onDispose(auth.dispose);

  return GoRouter(
    initialLocation: firstLaunchDone
        ? (auth.walletOpen ? '/overview' : '/setup')
        : '/language',
    refreshListenable: auth,
    redirect: (context, state) {
      final open = auth.walletOpen;
      final locked = auth.walletLocked;
      final loc = state.matchedLocation;

      // First launch → language picker
      if (!auth.firstLaunchDone && loc != '/language') return '/language';
      if (auth.firstLaunchDone && loc == '/language') return '/setup';

      if (!open && loc != '/setup' && loc != '/language') return '/setup';
      if (open && locked && loc != '/lock') return '/lock';
      if (open && !locked && (loc == '/setup' || loc == '/lock')) return '/overview';
      return null;
    },
    errorBuilder: (context, state) => _RouteNotFound(location: state.uri.toString()),
    routes: [
      GoRoute(
        path: '/language',
        builder: (_, _) => const LanguagePickerScreen(),
      ),
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

/// Shown for an unknown URL instead of a bare grey GoRouter error page.
class _RouteNotFound extends StatelessWidget {
  final String location;
  const _RouteNotFound({required this.location});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final cs = Theme.of(context).colorScheme;
    return Scaffold(
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.explore_off_outlined, size: 44, color: cs.onSurfaceVariant),
              const SizedBox(height: 16),
              Text(tr?.pageNotFound ?? 'Page not found',
                  style: Theme.of(context).textTheme.headlineSmall),
              const SizedBox(height: 8),
              Text(location,
                  style: TextStyle(fontSize: 12, color: cs.onSurfaceVariant),
                  textAlign: TextAlign.center),
              const SizedBox(height: 20),
              FilledButton(
                onPressed: () => context.go('/overview'),
                child: Text(tr?.backToWallet ?? 'Back to wallet'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
