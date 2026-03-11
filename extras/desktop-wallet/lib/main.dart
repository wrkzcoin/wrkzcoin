import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:local_notifier/local_notifier.dart';
import 'package:tray_manager/tray_manager.dart';
import 'package:window_manager/window_manager.dart';
import 'app/app.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await windowManager.ensureInitialized();
  await windowManager.setMinimumSize(const Size(900, 600));
  await windowManager.setTitle('PLUTON v2');

  await localNotifier.setup(appName: 'PLUTON Wallet');

  // System tray
  try {
    await trayManager.setIcon('assets/images/app_icon.ico');
    await trayManager.setToolTip('PLUTON Wallet');
    await trayManager.setContextMenu(Menu(items: [
      MenuItem(key: 'show', label: 'Show'),
      MenuItem.separator(),
      MenuItem(key: 'exit', label: 'Exit'),
    ]));
  } catch (_) {
    // Tray icon optional — may fail if asset not present
  }

  runApp(const ProviderScope(child: PlutonApp()));
}
