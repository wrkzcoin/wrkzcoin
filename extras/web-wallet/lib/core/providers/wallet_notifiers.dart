import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../api/models/balance.dart';
import '../api/models/wallet_status.dart';
import '../api/models/transaction.dart';
import '../config/app_config.dart';
import '../ffi/wallet_web.dart';
import 'providers.dart';

/// Mixin providing the shared refresh discipline for the wallet pollers:
///
///  * refresh when the WASM module says something changed, instead of
///    re-fetching on a fixed timer and hoping;
///  * keep a slow timer as a safety net for anything the event stream misses;
///  * poll faster while syncing, when the data really is changing every block;
///  * skip work entirely while the tab is hidden or the wallet is locked;
///  * never publish a state object that compares equal to the current one,
///    so widgets do not rebuild four times a minute for identical data.
mixin _WalletPoller<T> on AsyncNotifier<T> {
  Timer? _timer;
  StreamSubscription<({WalletEvent type, Map<String, dynamic> data})>? _events;
  bool _inFlight = false;
  Duration _interval = const Duration(seconds: 30);

  /// Events that should trigger an immediate refresh of this provider.
  Set<WalletEvent> get refreshOn =>
      const {WalletEvent.synced, WalletEvent.transaction};

  void initPolling(Duration interval) {
    _interval = interval;
    final api = ref.read(walletCApiProvider);

    _events = api.events.listen((event) {
      if (refreshOn.contains(event.type)) unawaited(refresh());
    });

    _restartTimer(interval);

    ref.onDispose(() {
      _timer?.cancel();
      _timer = null;
      unawaited(_events?.cancel());
      _events = null;
    });
  }

  void _restartTimer(Duration interval) {
    if (_timer != null && _interval == interval && _timer!.isActive) return;
    _interval = interval;
    _timer?.cancel();
    _timer = Timer.periodic(interval, (_) => unawaited(refresh()));
  }

  /// Switches to the fast cadence while syncing and back to the slack one when
  /// the wallet has caught up.
  void tunePolling({required bool syncing, required Duration idleInterval}) {
    _restartTimer(syncing ? kSyncingStatusPollInterval : idleInterval);
  }

  Future<T> fetch();

  Future<void> refresh() async {
    // Overlapping fetches pile requests onto a worker that may already be busy
    // with a slow scan; one in flight at a time is enough.
    if (_inFlight) return;
    if (!ref.read(walletCApiProvider).isOpen) return;
    if (!ref.read(appActiveProvider)) return;

    _inFlight = true;
    try {
      final next = await AsyncValue.guard(fetch);
      if (!_isSameAs(next)) state = next;
    } finally {
      _inFlight = false;
    }
  }

  bool _isSameAs(AsyncValue<T> next) {
    final current = state;
    if (current is AsyncData<T> && next is AsyncData<T>) {
      return _valueEquals(current.value, next.value);
    }
    if (current is AsyncError && next is AsyncError) {
      return current.error.toString() == (next as AsyncError).error.toString();
    }
    return false;
  }

  /// Override where `==` is not meaningful (lists).
  bool _valueEquals(T a, T b) => a == b;
}

/// True while the app should be doing background work: the tab is visible and
/// the wallet is unlocked. Updated by the shell.
final appActiveProvider = StateProvider<bool>((_) => true);

// ── Status polling ────────────────────────────────────────────────────────────

class StatusNotifier extends AsyncNotifier<WalletStatus> with _WalletPoller<WalletStatus> {
  @override
  Future<WalletStatus> build() async {
    initPolling(kStatusPollInterval);
    return fetch();
  }

  @override
  Future<WalletStatus> fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) throw Exception('Wallet not connected');
    final status = WalletStatus.fromJson(await ffi.getStatusJson());
    // While the chain is still being scanned the height moves every few
    // seconds; once synced there is nothing to see between events.
    tunePolling(syncing: !status.isWalletSynced, idleInterval: kStatusPollInterval);
    return status;
  }
}

final statusProvider =
    AsyncNotifierProvider<StatusNotifier, WalletStatus>(StatusNotifier.new);

// ── Balance polling ───────────────────────────────────────────────────────────

class BalanceNotifier extends AsyncNotifier<Balance> with _WalletPoller<Balance> {
  @override
  Future<Balance> build() async {
    initPolling(kBalancePollInterval);
    return fetch();
  }

  @override
  Future<Balance> fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) throw Exception('Wallet not connected');
    final (:unlocked, :locked) = await ffi.getTotalBalance();
    return Balance(unlocked: unlocked, locked: locked);
  }
}

final balanceProvider =
    AsyncNotifierProvider<BalanceNotifier, Balance>(BalanceNotifier.new);

// ── Transaction list ──────────────────────────────────────────────────────────

class TransactionsNotifier extends AsyncNotifier<List<Transaction>>
    with _WalletPoller<List<Transaction>> {
  @override
  Future<List<Transaction>> build() async {
    initPolling(kTransactionPollInterval);
    return fetch();
  }

  /// A synced event does not change the transaction set on its own; only fetch
  /// the full history when the wallet says a transaction actually arrived.
  @override
  Set<WalletEvent> get refreshOn => const {WalletEvent.transaction};

  @override
  Future<List<Transaction>> fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) return const [];
    final json = await ffi.getTransactionsJson(includeUnconfirmed: true);
    final confirmed = (json['transactions'] as List<dynamic>? ?? [])
        .map((t) => Transaction.fromJson(t as Map<String, dynamic>))
        .toList();
    final unconfirmed =
        (json['unconfirmedTransactions'] as List<dynamic>? ?? [])
            .map((t) => Transaction.fromJson(t as Map<String, dynamic>))
            .toList();
    // Show unconfirmed (mempool) first, then confirmed most-recent-first
    return [...unconfirmed, ...confirmed.reversed];
  }

  /// Lists never compare equal by identity, so compare contents — otherwise
  /// every poll replaces the state and rebuilds the whole history list.
  @override
  bool _valueEquals(List<Transaction> a, List<Transaction> b) {
    if (identical(a, b)) return true;
    if (a.length != b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] != b[i]) return false;
    }
    return true;
  }
}

final transactionsProvider =
    AsyncNotifierProvider<TransactionsNotifier, List<Transaction>>(
        TransactionsNotifier.new);

// ── Node info ─────────────────────────────────────────────────────────────────

class NodeInfoNotifier extends AsyncNotifier<Map<String, dynamic>>
    with _WalletPoller<Map<String, dynamic>> {
  @override
  Future<Map<String, dynamic>> build() async {
    initPolling(kStatusPollInterval);
    return fetch();
  }

  @override
  Set<WalletEvent> get refreshOn => const {WalletEvent.synced};

  @override
  Future<Map<String, dynamic>> fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) return const {};
    return ffi.getNodeInfoJson();
  }

  @override
  bool _valueEquals(Map<String, dynamic> a, Map<String, dynamic> b) {
    if (identical(a, b)) return true;
    if (a.length != b.length) return false;
    for (final entry in a.entries) {
      if (!b.containsKey(entry.key) || b[entry.key] != entry.value) return false;
    }
    return true;
  }
}

final nodeInfoProvider =
    AsyncNotifierProvider<NodeInfoNotifier, Map<String, dynamic>>(
        NodeInfoNotifier.new);

// ── Manual refresh ───────────────────────────────────────────────────────────

/// Refreshes everything at once — wired to the refresh button and to
/// resuming from a hidden tab or the lock screen.
Future<void> refreshAllWalletData(WidgetRef ref) async {
  await Future.wait([
    ref.read(statusProvider.notifier).refresh(),
    ref.read(balanceProvider.notifier).refresh(),
    ref.read(transactionsProvider.notifier).refresh(),
    ref.read(nodeInfoProvider.notifier).refresh(),
  ]);
}
