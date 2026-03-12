import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../api/models/balance.dart';
import '../api/models/transaction.dart';
import '../api/models/wallet_status.dart';
import '../config/app_config.dart';
import 'providers.dart';

// ── status ───────────────────────────────────────────────────────────────────

class StatusNotifier extends AsyncNotifier<WalletStatus> {
  Timer? _timer;

  @override
  Future<WalletStatus> build() async {
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(
      AppConfig.statusPollInterval,
      (_) => refresh(),
    );
    return _fetch();
  }

  Future<WalletStatus> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    final json = await ffi.getStatusJson();
    return WalletStatus.fromJson(json);
  }

  Future<void> refresh() async {
    state = const AsyncValue.loading();
    state = await AsyncValue.guard(_fetch);
  }
}

final statusProvider =
    AsyncNotifierProvider<StatusNotifier, WalletStatus>(StatusNotifier.new);

// ── balance ──────────────────────────────────────────────────────────────────

class BalanceNotifier extends AsyncNotifier<Balance> {
  Timer? _timer;

  @override
  Future<Balance> build() async {
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(
      AppConfig.balancePollInterval,
      (_) => refresh(),
    );
    return _fetch();
  }

  Future<Balance> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    final bal = await ffi.getTotalBalance();
    return Balance(unlocked: bal.unlocked, locked: bal.locked);
  }

  Future<void> refresh() async {
    state = const AsyncValue.loading();
    state = await AsyncValue.guard(_fetch);
  }
}

final balanceProvider =
    AsyncNotifierProvider<BalanceNotifier, Balance>(BalanceNotifier.new);

// ── transactions ─────────────────────────────────────────────────────────────

class TransactionsNotifier extends AsyncNotifier<List<Transaction>> {
  Timer? _timer;

  @override
  Future<List<Transaction>> build() async {
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(
      AppConfig.transactionsPollInterval,
      (_) => refresh(),
    );
    return _fetch();
  }

  Future<List<Transaction>> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    final json = await ffi.getTransactionsJson();
    final confirmed = (json['transactions'] as List<dynamic>?)
            ?.map((t) => Transaction.fromJson(t as Map<String, dynamic>))
            .toList() ??
        [];
    final unconfirmed = (json['unconfirmedTransactions'] as List<dynamic>?)
            ?.map((t) => Transaction.fromJson(t as Map<String, dynamic>))
            .toList() ??
        [];
    final all = [...unconfirmed, ...confirmed];
    all.sort((a, b) => b.timestamp.compareTo(a.timestamp));
    return all;
  }

  Future<void> refresh() async {
    state = const AsyncValue.loading();
    state = await AsyncValue.guard(_fetch);
  }
}

final transactionsProvider =
    AsyncNotifierProvider<TransactionsNotifier, List<Transaction>>(
        TransactionsNotifier.new);

// ── node info ────────────────────────────────────────────────────────────────

class NodeInfoNotifier extends AsyncNotifier<Map<String, dynamic>> {
  Timer? _timer;

  @override
  Future<Map<String, dynamic>> build() async {
    ref.onDispose(() => _timer?.cancel());
    _timer = Timer.periodic(
      AppConfig.statusPollInterval,
      (_) => refresh(),
    );
    return _fetch();
  }

  Future<Map<String, dynamic>> _fetch() async {
    final ffi = ref.read(walletCApiProvider);
    return ffi.getNodeInfoJson();
  }

  Future<void> refresh() async {
    state = const AsyncValue.loading();
    state = await AsyncValue.guard(_fetch);
  }
}

final nodeInfoProvider =
    AsyncNotifierProvider<NodeInfoNotifier, Map<String, dynamic>>(
        NodeInfoNotifier.new);
