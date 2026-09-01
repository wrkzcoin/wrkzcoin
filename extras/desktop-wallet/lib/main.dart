import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:local_notifier/local_notifier.dart';
import 'package:path_provider/path_provider.dart';
import 'package:window_manager/window_manager.dart';
import 'app/app.dart';
import 'core/node/local_node.dart';
import 'core/node/local_node_controller.dart';
import 'features/shell/app_lifecycle.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await windowManager.ensureInitialized();
  await windowManager.setMinimumSize(const Size(900, 600));
  await windowManager.setTitle('PLUTON v2');

  await localNotifier.setup(appName: 'PLUTON Wallet');

  // Where an optional local lite node keeps its chain. Resolved before the
  // first frame so the settings screen can read the node's state
  // synchronously; the directory itself is only created if a node is set up.
  //
  // A remembered choice wins, then a node already sitting in the old fixed
  // location, then a folder beside the executable for a portable copy. See
  // resolveNodeDataDir.
  final support = await getApplicationSupportDirectory();
  LocalNodePaths.bindDataDirectory(await resolveNodeDataDir(support.path));

  // Read once at launch so a configured node resumes syncing — or an existing
  // one left behind by a previous run is adopted — without waiting for the
  // settings screen to be opened.
  final container = ProviderContainer();
  container.read(localNodeProvider);

  // AppLifecycle sits above the router on purpose: it owns the tray icon, the
  // window close interception and the shutdown, none of which may be torn down
  // by a route change. Inside the router they were, and locking the wallet left
  // a tray whose Exit item did nothing.
  runApp(UncontrolledProviderScope(
    container: container,
    child: const AppLifecycle(child: PlutonApp()),
  ));
}
