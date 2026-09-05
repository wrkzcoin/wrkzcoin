import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../api/models/balance.dart';
import '../api/models/wallet_status.dart';
import '../api/models/transaction.dart';
import '../config/app_config.dart';
import 'providers.dart';

// ── Wallet session ────────────────────────────────────────────────────────────

/// Starts a new wallet session: drops every wallet-scoped cache so nothing
/// from the previous wallet can be shown against the new one.
///
/// Call after each successful open/create/restore, and after a close. The
/// providers below all watch [walletSessionProvider], so bumping it rebuilds
/// them — which also cancels and restarts their poll timers.
void beginWalletSession(WidgetRef ref, {String? walletName}) {
  ref.read(openWalletNameProvider.notifier).state = walletName;
  ref.read(walletSessionProvider.notifier).state++;
}

// ── Status polling ────────────────────────────────────────────────────────────

class StatusNotifier extends AsyncNotifier<WalletStatus> {
  Timer? _timer;

  @override
  Future<WalletStatus> build() async {
    ref.watch(walletSessionProvider);
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(kStatusPollInterval, (_) => _refresh());
    return _fetch();
  }

  Future<WalletStatus> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) throw Exception('Wallet not connected');
    final json = await ffi.getStatusJson();
    return WalletStatus.fromJson(json);
  }

  Future<void> _refresh() async {
    state = await AsyncValue.guard(_fetch);
  }

  Future<void> refresh() => _refresh();
}

final statusProvider =
    AsyncNotifierProvider<StatusNotifier, WalletStatus>(StatusNotifier.new);

// ── Balance polling ───────────────────────────────────────────────────────────

class BalanceNotifier extends AsyncNotifier<Balance> {
  Timer? _timer;

  @override
  Future<Balance> build() async {
    ref.watch(walletSessionProvider);
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(kBalancePollInterval, (_) => _refresh());
    return _fetch();
  }

  Future<Balance> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) throw Exception('Wallet not connected');
    final (:unlocked, :locked) = await ffi.getTotalBalance();
    return Balance(unlocked: unlocked, locked: locked);
  }

  Future<void> _refresh() async {
    state = await AsyncValue.guard(_fetch);
  }

  Future<void> refresh() => _refresh();
}

final balanceProvider =
    AsyncNotifierProvider<BalanceNotifier, Balance>(BalanceNotifier.new);

// ── Transaction list ──────────────────────────────────────────────────────────

class TransactionsNotifier extends AsyncNotifier<List<Transaction>> {
  Timer? _timer;

  @override
  Future<List<Transaction>> build() async {
    ref.watch(walletSessionProvider);
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(kTransactionPollInterval, (_) => _refresh());
    return _fetch();
  }

  Future<List<Transaction>> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) return [];
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

  Future<void> _refresh() async {
    state = await AsyncValue.guard(_fetch);
  }

  Future<void> refresh() => _refresh();
}

final transactionsProvider =
    AsyncNotifierProvider<TransactionsNotifier, List<Transaction>>(
        TransactionsNotifier.new);

// ── Node info ─────────────────────────────────────────────────────────────────

class NodeInfoNotifier extends AsyncNotifier<Map<String, dynamic>> {
  Timer? _timer;

  @override
  Future<Map<String, dynamic>> build() async {
    ref.watch(walletSessionProvider);
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(kStatusPollInterval, (_) => _refresh());
    return _fetch();
  }

  Future<Map<String, dynamic>> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    if (!ffi.isOpen) return {};
    return ffi.getNodeInfoJson();
  }

  Future<void> _refresh() async {
    state = await AsyncValue.guard(_fetch);
  }

  Future<void> refresh() => _refresh();
}

final nodeInfoProvider =
    AsyncNotifierProvider<NodeInfoNotifier, Map<String, dynamic>>(
        NodeInfoNotifier.new);
