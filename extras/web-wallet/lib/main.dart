import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'app/app.dart';
import 'core/auth/wallet_auth.dart';
import 'core/ffi/wallet_web.dart';
import 'core/providers/app_providers.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Read every stored preference before the first frame so the app paints in
  // the right theme and language immediately, instead of rendering defaults
  // and snapping to the saved values a frame later.
  final prefs = await loadPreferences();

  // Older builds persisted the wallet password in recoverable form. Drop it on
  // startup so upgrading is enough to stop carrying it around.
  unawaitedPurge();

  runApp(
    ProviderScope(
      overrides: [preloadedPrefsProvider.overrideWithValue(prefs)],
      child: const PlutonWebApp(),
    ),
  );

  // Take down the HTML boot splash once Flutter has actually painted.
  WidgetsBinding.instance.addPostFrameCallback((_) => dismissBootSplash());
}

/// Fire-and-forget legacy credential cleanup — never worth delaying startup for.
void unawaitedPurge() {
  purgeLegacyCredentials().catchError((Object _) {});
}
