/// app_lifecycle.dart
///
/// Owns the three things that have to outlive any route: the tray icon, the
/// window close/minimise interception, and the shutdown sequence.
///
/// They used to live in MainShell, which a ShellRoute builds. Locking the
/// wallet routes to `/lock`, which sits outside that shell, so the state was
/// disposed while the tray icon stayed on screen with an Exit item still bound
/// to it — and riverpod throws on `ref` after dispose, so Exit did nothing at
/// all until the wallet was unlocked again. Nothing here is tied to a route.
library;

import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:local_notifier/local_notifier.dart';
import 'package:path/path.dart' as p;
import 'package:system_tray/system_tray.dart';
import 'package:window_manager/window_manager.dart';

import '../../core/node/local_node.dart';
import '../../core/node/local_node_controller.dart';
import '../../core/providers/app_providers.dart';
import '../../core/providers/providers.dart';
import '../../l10n/generated/app_localizations.dart';

/// How far along the shutdown is, for the overlay that covers the window while
/// the app closes down.
enum ShutdownPhase {
  /// Not quitting.
  none,

  /// Waiting for the user to say what should happen to the running node.
  askingNode,

  /// Shutting the local node down cleanly.
  stoppingNode,

  /// Closing the wallet, which saves it.
  saving,

  /// Still going well past the point where it should have finished, so the
  /// user is offered a way out that is not Task Manager.
  slow,
}

/// What the user said about the node, and whether to stop asking.
typedef NodeExitChoice = ({bool keepRunning, bool remember});

class ShutdownState {
  final ShutdownPhase phase;

  /// Which answer the question opens on. A first sync runs for hours and is
  /// worth keeping; a node that is already level with the network is not worth
  /// burning CPU, bandwidth and disk on while the wallet is closed.
  final bool suggestKeepRunning;

  const ShutdownState({
    this.phase = ShutdownPhase.none,
    this.suggestKeepRunning = true,
  });
}

/// Drives the shutdown overlay, and carries the answer back from it.
///
/// The overlay lives inside MaterialApp — it needs a Theme and Localizations —
/// while the shutdown itself is run from [AppLifecycle], above the router. This
/// is the seam between them.
class ShutdownController extends Notifier<ShutdownState> {
  Completer<NodeExitChoice>? _answer;

  @override
  ShutdownState build() => const ShutdownState();

  void setPhase(ShutdownPhase phase) => state = ShutdownState(
        phase: phase,
        suggestKeepRunning: state.suggestKeepRunning,
      );

  Future<NodeExitChoice> askAboutNode({required bool suggestKeep}) {
    final answer = Completer<NodeExitChoice>();
    _answer = answer;
    state = ShutdownState(
      phase: ShutdownPhase.askingNode,
      suggestKeepRunning: suggestKeep,
    );
    return answer.future;
  }

  void answer(NodeExitChoice choice) {
    final pending = _answer;
    _answer = null;
    if (pending != null && !pending.isCompleted) pending.complete(choice);
  }
}

final shutdownProvider =
    NotifierProvider<ShutdownController, ShutdownState>(ShutdownController.new);

/// The live tray, so [forceQuitNow] can take its icon down without building a
/// second [SystemTray]: the constructor claims the plugin's method channel, and
/// a throwaway instance would silently steal tray events from the real one.
SystemTray? _liveTray;

/// Ends the process.
///
/// The tray icon is taken down first — the shell only reaps one whose owner it
/// has noticed is gone, so skipping this leaves a ghost in the notification
/// area until the user happens to hover over it. Then the process exits
/// outright.
///
/// `windowManager.destroy()` is deliberately not used, and this is the whole
/// reason the app was unclosable. On Windows it is `PostQuitMessage(0)`, and
/// Windows hands `WM_QUIT` to `GetMessage` only once the message queue is
/// empty. An animation — the shutdown overlay's own spinner will do — posts a
/// frame every vsync, so the queue never empties and the loop never ends.
/// Worse, the attempt starves the Dart event loop, so anything scheduled after
/// it is stranded too. Measured on this build: awaiting `destroy()` never
/// returned on any of five launches; not awaiting it still pushed a 500 ms
/// fallback timer out to about fifteen seconds. `exit()` needs none of that
/// machinery.
///
/// Exiting like this is safe: on the ordinary path the wallet has already been
/// closed and written out, and on the "quit anyway" path the worst case is
/// losing that save. It cannot damage the wallet file — the save writes a
/// temporary and renames it over, so the file on disk is only ever the old
/// wallet or the new one.
Future<void> forceQuitNow() async {
  try {
    await _liveTray?.destroy();
  } catch (_) {
    // Nothing installed, or the shell is not answering. Quit regardless.
  }
  /* Take our notifications with us. A toast is registered with the shell, not
     with us, so any still outstanding here goes on popping up in the tray
     after the process is gone - which is what the user sees, and there is no
     app left to attribute it to. This is the last point anything of ours runs:
     exit() below means no widget's dispose() is ever called. */
  try {
    await takeDownNotifications();
  } catch (_) {
    // Best effort. Never stand between the user and a quit.
  }
  exit(0);
}

/// Closes every notification this session raised, set by the shell that owns
/// them. Called from [forceQuitNow], which is the only way this app exits.
Future<void> Function() takeDownNotifications = () async {};

class AppLifecycle extends ConsumerStatefulWidget {
  final Widget child;

  const AppLifecycle({super.key, required this.child});

  @override
  ConsumerState<AppLifecycle> createState() => _AppLifecycleState();
}

class _AppLifecycleState extends ConsumerState<AppLifecycle>
    with WindowListener {
  final _systemTray = SystemTray();

  Timer? _trayClickTimer;

  /// The icon is really on screen. Only then may closing the window hide it
  /// instead of quitting — otherwise the app vanishes with nothing left to
  /// bring it back.
  bool _trayReady = false;

  /// The one shutdown in flight. Every later Exit joins it rather than starting
  /// a competing close, and — unlike the flag this replaced — it always
  /// completes, so a slow save can no longer leave the window unclosable.
  Future<void>? _shutdown;

  bool _explainedTray = false;

  /// How long the wallet close is given before the overlay offers a way out.
  static const _slowShutdownAfter = Duration(seconds: 20);

  @override
  void initState() {
    super.initState();
    windowManager.addListener(this);
    // Every close is intercepted, on the setup and lock screens too: whether it
    // hides or quits is decided in onWindowClose, and quitting has to run
    // through _quit() so the wallet is written out first.
    unawaited(windowManager.setPreventClose(true));
    unawaited(_initSystemTray());
  }

  @override
  void dispose() {
    _trayClickTimer?.cancel();
    windowManager.removeListener(this);
    super.dispose();
  }

  // ── strings ────────────────────────────────────────────────────────────────

  /// Tray labels are built outside the widget tree, above MaterialApp, so they
  /// cannot come from `S.of(context)`. Look them up by locale instead.
  S get _tr {
    final locale = ref.read(localeProvider);
    try {
      if (locale != null && supportedLocales.contains(locale)) {
        return lookupS(locale);
      }
    } catch (_) {
      // An unsupported locale slipped through; English is always there.
    }
    return lookupS(const Locale('en'));
  }

  // ── system tray ────────────────────────────────────────────────────────────

  /// Flutter copies `assets/` into `data/flutter_assets/` next to the
  /// executable, so the tray icon has to be addressed there — a bare
  /// `assets/...` path only resolves when running from the project root.
  String get _trayIconPath {
    final exeDir = File(Platform.resolvedExecutable).parent.path;
    final name = Platform.isWindows ? 'app_icon.ico' : 'app_icon.png';
    final bundled =
        p.join(exeDir, 'data', 'flutter_assets', 'assets', 'images', name);
    if (File(bundled).existsSync()) return bundled;
    return p.join('assets', 'images', name); // `flutter run` from the repo
  }

  Future<void> _initSystemTray() async {
    try {
      // The return value matters. A shell that refuses the icon reports it here
      // rather than throwing, and taking that for success is what left the app
      // hiding itself into a tray that was not there.
      final installed = await _systemTray.initSystemTray(
        title: _tr.plutonWallet,
        iconPath: _trayIconPath,
        toolTip: _tr.plutonWallet,
      );
      if (!installed) {
        debugPrint('[tray] the shell refused the icon');
        return;
      }

      await _buildTrayMenu();
      _liveTray = _systemTray;
      // Only now is hiding the window safe — there is something to restore it.
      _trayReady = true;

      _systemTray.registerSystemTrayEventHandler(_onTrayEvent);
    } catch (e) {
      // No tray: leave close and minimise behaving normally so the window stays
      // reachable.
      _trayReady = false;
      debugPrint('[tray] init failed: $e');
    }
  }

  Future<void> _buildTrayMenu() async {
    final tr = _tr;
    final menu = Menu();
    await menu.buildFrom([
      MenuItemLabel(label: tr.show, onClicked: (_) => unawaited(_showWindow())),
      MenuSeparator(),
      MenuItemLabel(label: tr.exit, onClicked: (_) => unawaited(_quit())),
    ]);
    await _systemTray.setContextMenu(menu);
  }

  void _onTrayEvent(String eventName) {
    if (eventName == kSystemTrayEventClick) {
      // Single left click → show window
      if (_trayClickTimer?.isActive ?? false) {
        // Second click within threshold → double-click: maximize
        _trayClickTimer!.cancel();
        _trayClickTimer = null;
        unawaited(_showWindow(maximize: true));
      } else {
        _trayClickTimer = Timer(const Duration(milliseconds: 350), () {
          unawaited(_showWindow());
          _trayClickTimer = null;
        });
      }
    } else if (eventName == kSystemTrayEventRightClick) {
      unawaited(_systemTray.popUpContextMenu());
    }
  }

  // ── window ─────────────────────────────────────────────────────────────────

  Future<void> _showWindow({bool maximize = false}) async {
    await windowManager.show();
    if (maximize) await windowManager.maximize();
    // Windows restricts SetForegroundWindow from background processes.
    // Briefly setting alwaysOnTop forces the window to the front reliably.
    await windowManager.setAlwaysOnTop(true);
    await windowManager.focus();
    await windowManager.setAlwaysOnTop(false);
  }

  /// Hides the window, and says so the first time.
  ///
  /// On Windows a new app's tray icon goes into the overflow flyout, so a
  /// window that disappears with no icon in sight reads as a crash — or as an
  /// app that cannot be quit, which is how this arrived as a bug report.
  Future<void> _hideToTray() async {
    await windowManager.hide();
    if (_explainedTray) return;
    _explainedTray = true;
    try {
      await LocalNotification(
        title: _tr.plutonWallet,
        body: _tr.stillRunningInTray,
      ).show();
    } catch (e) {
      debugPrint('[tray] notification failed: $e');
    }
  }

  /// Whether the window should park in the tray rather than close or minimise
  /// normally.
  ///
  /// Only once a wallet is open. Before that there is nothing to keep running
  /// for, and an app the user has not even finished setting up should not
  /// survive its own window.
  bool get _canHideToTray => _trayReady && ref.read(walletOpenProvider);

  @override
  Future<void> onWindowMinimize() async {
    if (_canHideToTray) await _hideToTray();
  }

  @override
  Future<void> onWindowClose() async {
    if (_canHideToTray && _shutdown == null) {
      await _hideToTray();
      return;
    }
    await _quit();
  }

  // ── shutdown ───────────────────────────────────────────────────────────────

  /// Writes the wallet out and quits.
  ///
  /// Returns the shutdown already in flight if there is one, so a second Exit
  /// is harmless rather than starting a competing close — or, as the flag this
  /// replaced did, latching the app into a state where neither the tray nor the
  /// window's own close button could ever quit it again.
  Future<void> _quit() => _shutdown ??= _runShutdown();

  Future<void> _runShutdown() async {
    // The node question comes first: it is the only part that waits on a
    // person, and asking it after a thirty-second wallet save would be rude.
    await _settleLocalNode();

    _setPhase(ShutdownPhase.saving);
    final slow = Timer(_slowShutdownAfter, () => _setPhase(ShutdownPhase.slow));
    try {
      final ffi = ref.read(walletCApiProvider);
      // close() is the whole save: ~WalletBackend writes the wallet on its way
      // out. The explicit save() that used to run first meant one quit did the
      // entire pause-synchroniser, PBKDF2 and AES pass twice over.
      if (ffi.isOpen) await ffi.close();
    } catch (e) {
      debugPrint('[shutdown] close failed: $e');
    } finally {
      slow.cancel();
    }
    await forceQuitNow();
  }

  /// Decides what happens to a running local node, and does it.
  ///
  /// The daemon is a child process that outlives this one unless it is told
  /// otherwise, and until now nothing ever told it otherwise — quitting the
  /// wallet left a node syncing forever with nothing on screen having mentioned
  /// it.
  Future<void> _settleLocalNode() async {
    if (!mounted) return;

    final node = ref.read(localNodeProvider);
    if (!node.isRunning) return;

    var keepRunning = true;
    switch (await readNodeExitPolicy()) {
      case NodeExitPolicy.keep:
        keepRunning = true;
      case NodeExitPolicy.stop:
        keepRunning = false;
      case NodeExitPolicy.ask:
        if (!mounted) return;
        // Exit from the tray arrives with the window hidden, and a question
        // nobody can see is the unclosable app all over again.
        await _showWindow();
        if (!mounted) return;
        // An unfinished first sync is hours of work; a synced node catches up
        // in minutes. That is the whole basis for the preselected answer.
        final choice = await ref
            .read(shutdownProvider.notifier)
            .askAboutNode(suggestKeep: !node.synced);
        keepRunning = choice.keepRunning;
        if (choice.remember) {
          await writeNodeExitPolicy(
              keepRunning ? NodeExitPolicy.keep : NodeExitPolicy.stop);
        }
    }

    if (keepRunning) {
      await _sayNodeIsStillRunning();
      return;
    }

    _setPhase(ShutdownPhase.stoppingNode);
    try {
      // keepAutoStart: this is the app closing, not the user switching the node
      // off, so it comes back on the next launch.
      if (!mounted) return;
      await ref.read(localNodeProvider.notifier).stop(keepAutoStart: true);
    } catch (e) {
      debugPrint('[shutdown] stopping the node failed: $e');
    }
  }

  /// Says the daemon is still there, because nothing else will.
  ///
  /// Once this process is gone there is no window, no tray icon and no other
  /// sign of it — just a Wrkzd in Task Manager using the disk and the network.
  Future<void> _sayNodeIsStillRunning() async {
    try {
      await LocalNotification(
        title: _tr.plutonWallet,
        body: _tr.localNodeStillRunningBody,
      ).show();
    } catch (e) {
      debugPrint('[shutdown] notification failed: $e');
    }
  }

  void _setPhase(ShutdownPhase phase) {
    if (!mounted) return;
    ref.read(shutdownProvider.notifier).setPhase(phase);
  }

  @override
  Widget build(BuildContext context) {
    // Tray labels are baked into the native menu, so switching language has to
    // rebuild it.
    ref.listen<Locale?>(localeProvider, (_, _) {
      if (_trayReady) unawaited(_buildTrayMenu());
    });
    return widget.child;
  }
}

/// Covers the window while the app closes down.
///
/// Three jobs: put the "the node is still running" question somewhere it can be
/// answered, show that the node is being stopped, and show that the wallet is
/// being written out. It lives inside MaterialApp because it needs a Theme and
/// Localizations; [ShutdownController] is the seam back to [AppLifecycle].
class ShutdownOverlay extends ConsumerWidget {
  const ShutdownOverlay({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final shutdown = ref.watch(shutdownProvider);
    if (shutdown.phase == ShutdownPhase.none) return const SizedBox.shrink();

    return Positioned.fill(
      child: ColoredBox(
        color: Colors.black.withAlpha(160),
        child: Center(
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 420),
            child: Card(
              margin: const EdgeInsets.all(24),
              child: Padding(
                padding: const EdgeInsets.all(24),
                child: shutdown.phase == ShutdownPhase.askingNode
                    ? _NodeExitQuestion(
                        suggestKeep: shutdown.suggestKeepRunning)
                    : _ShutdownProgress(phase: shutdown.phase),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _ShutdownProgress extends StatelessWidget {
  final ShutdownPhase phase;

  const _ShutdownProgress({required this.phase});

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final theme = Theme.of(context);
    final stopping = phase == ShutdownPhase.stoppingNode;

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        const CircularProgressIndicator(),
        const SizedBox(height: 20),
        Text(
          stopping
              ? (tr?.shutdownStoppingNode ?? 'Stopping the local node…')
              : (tr?.savingWallet ?? 'Saving your wallet…'),
          textAlign: TextAlign.center,
          style: theme.textTheme.titleMedium,
        ),
        const SizedBox(height: 8),
        Text(
          stopping
              ? (tr?.shutdownStoppingNodeBody ??
                  'Letting it flush its database, so the next start does not '
                      'have to replay the write-ahead log.')
              : (tr?.savingWalletBody ??
                  'Do not power off. This can take a moment on a large '
                      'wallet.'),
          textAlign: TextAlign.center,
          style: theme.textTheme.bodySmall,
        ),
        if (phase == ShutdownPhase.slow) ...[
          const SizedBox(height: 20),
          Text(
            tr?.shutdownTakingLong ??
                'This is taking longer than expected. Quitting now loses this '
                    'save — the wallet on disk stays as it was, so nothing '
                    'is damaged, but anything since the last save is gone.',
            textAlign: TextAlign.center,
            style: theme.textTheme.bodySmall
                ?.copyWith(color: theme.colorScheme.error),
          ),
          const SizedBox(height: 12),
          TextButton(
            onPressed: () => unawaited(forceQuitNow()),
            child: Text(tr?.quitAnyway ?? 'Quit anyway'),
          ),
        ],
      ],
    );
  }
}

/// "The local node is still running" — asked once, then remembered.
class _NodeExitQuestion extends ConsumerStatefulWidget {
  final bool suggestKeep;

  const _NodeExitQuestion({required this.suggestKeep});

  @override
  ConsumerState<_NodeExitQuestion> createState() => _NodeExitQuestionState();
}

class _NodeExitQuestionState extends ConsumerState<_NodeExitQuestion> {
  bool _remember = false;

  /// The buttons do nothing for a moment after the question appears.
  ///
  /// Quitting yanks the window to the front — show, alwaysOnTop, focus — and a
  /// tap then lands on whatever is under the cursor. Measured: with the cursor
  /// parked in a screen corner and nobody touching the mouse, a real
  /// `TapGestureRecognizer` fired on the first button within 1.7–3.6 seconds on
  /// every launch, and silently stopped the node. Whatever synthesises it, a
  /// dialog that appears under the pointer must not accept a click it did not
  /// wait for. Long enough to swallow that; short enough that nobody aiming at
  /// a button notices.
  static const _settleDelay = Duration(milliseconds: 800);

  bool _armed = false;
  Timer? _arm;

  @override
  void initState() {
    super.initState();
    _arm = Timer(_settleDelay, () {
      if (mounted) setState(() => _armed = true);
    });
  }

  @override
  void dispose() {
    _arm?.cancel();
    super.dispose();
  }

  void _answer(bool keepRunning) {
    if (!_armed) return;
    ref
        .read(shutdownProvider.notifier)
        .answer((keepRunning: keepRunning, remember: _remember));
  }

  @override
  Widget build(BuildContext context) {
    final tr = S.of(context);
    final theme = Theme.of(context);

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          tr?.nodeExitTitle ?? 'The local node is still running',
          style: theme.textTheme.titleMedium,
        ),
        const SizedBox(height: 10),
        Text(
          widget.suggestKeep
              ? (tr?.nodeExitBodySyncing ??
                  'Its first sync has not finished. That takes hours, and it '
                      'only makes progress while the node is running — but '
                      'a node left running keeps using the disk and the '
                      'network after PLUTON has closed.')
              : (tr?.nodeExitBodySynced ??
                  'It is level with the network. Leaving it running keeps it '
                      'that way and keeps using CPU, bandwidth and the disk '
                      'while PLUTON is closed; stopping it costs a short '
                      'catch-up next time.'),
          style: theme.textTheme.bodySmall?.copyWith(height: 1.5),
        ),
        const SizedBox(height: 6),
        CheckboxListTile(
          value: _remember,
          onChanged: (v) => setState(() => _remember = v ?? false),
          dense: true,
          contentPadding: EdgeInsets.zero,
          controlAffinity: ListTileControlAffinity.leading,
          title: Text(
            tr?.rememberMyChoice ?? 'Remember my choice',
            style: theme.textTheme.bodySmall,
          ),
        ),
        const SizedBox(height: 4),
        Text(
          tr?.nodeExitChangeLater ??
              'Changeable later in Settings, under Local Lite Node.',
          style: theme.textTheme.bodySmall
              ?.copyWith(color: theme.colorScheme.onSurfaceVariant),
        ),
        const SizedBox(height: 16),
        Row(
          mainAxisAlignment: MainAxisAlignment.end,
          children: [
            // The preselected answer is the filled one, so the obvious click is
            // the right one for the state the node is actually in.
            if (widget.suggestKeep) ...[
              TextButton(
                onPressed: _armed ? () => _answer(false) : null,
                child: Text(tr?.nodeExitStop ?? 'Stop it'),
              ),
              const SizedBox(width: 8),
              FilledButton(
                onPressed: _armed ? () => _answer(true) : null,
                child: Text(tr?.nodeExitKeep ?? 'Leave it running'),
              ),
            ] else ...[
              TextButton(
                onPressed: _armed ? () => _answer(true) : null,
                child: Text(tr?.nodeExitKeep ?? 'Leave it running'),
              ),
              const SizedBox(width: 8),
              FilledButton(
                onPressed: _armed ? () => _answer(false) : null,
                child: Text(tr?.nodeExitStop ?? 'Stop it'),
              ),
            ],
          ],
        ),
      ],
    );
  }
}
