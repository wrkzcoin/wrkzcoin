/// wallet_web.dart
///
/// Web implementation of the wallet API using dart:js_interop to call
/// the WalletBridge JavaScript wrapper around the WASM module.
///
/// This file provides the same public API as wallet_ffi.dart (desktop/mobile)
/// so that the rest of the Flutter app (providers, screens) can use either
/// implementation transparently.
///
/// Usage:
///   final api = WalletCApi();
///   await api.create('my_wallet', 'pass', 'nodes.wrkz.work', 17856);
///   // ... use api ...
///   api.close();
library;

import 'dart:convert';
import 'dart:js_interop';



// ─── exception ────────────────────────────────────────────────────────────────

class WalletCApiException implements Exception {
  final int errorCode;
  final String message;
  WalletCApiException(this.errorCode, this.message);

  @override
  String toString() => 'WalletCApiException($errorCode): $message';
}

// ─── event types (match WALLET_EVENT_* in wallet_capi.h) ─────────────────────

enum WalletEvent {
  none(0),
  synced(1),
  transaction(2);

  final int value;
  const WalletEvent(this.value);

  static WalletEvent fromInt(int v) =>
      WalletEvent.values.firstWhere((e) => e.value == v,
          orElse: () => WalletEvent.none);
}

// ─── JS interop bindings ─────────────────────────────────────────────────────

/// Minimal binding to globalThis.JSON for parse/stringify.
@JS('JSON.parse')
external JSAny _jsonParse(JSString text);

@JS('JSON.stringify')
external JSString _jsonStringify(JSAny? value);

/// Binding for walletBridge.call(method, paramsObj).
@JS('walletBridge.call')
external JSAny? _bridgeCall(JSString method, JSAny params);

/// Call a method on the JS WalletBridge.call() synchronously.
/// The WASM module runs synchronously (not async) so this returns immediately.
JSAny? _jsCall(String method, [Map<String, dynamic>? params]) {
  final paramsStr = params != null ? jsonEncode(params) : '{}';
  final paramsObj = _jsonParse(paramsStr.toJS);
  return _bridgeCall(method.toJS, paramsObj);
}

/// Helper: call bridge and parse the result as a Dart object.
dynamic _call(String method, [Map<String, dynamic>? params]) {
  final result = _jsCall(method, params);
  if (result == null) return null;

  // Convert JS result to Dart via JSON round-trip
  final jsonStr = _jsonStringify(result);
  return jsonDecode(jsonStr.toDart);
}

/// Helper: call bridge and expect a string result.
String _callStr(String method, [Map<String, dynamic>? params]) {
  final result = _jsCall(method, params);
  if (result == null) return '';
  if (result.isA<JSString>()) return (result as JSString).toDart;
  // Might be returned as a JSON-encoded value
  return result.toString();
}

/// Helper: call bridge and expect a bool result.
bool _callBool(String method, [Map<String, dynamic>? params]) {
  final result = _jsCall(method, params);
  if (result == null) return false;
  if (result.isA<JSBoolean>()) return (result as JSBoolean).toDart;
  return false;
}

/// Helper: call bridge and expect an int result.
int _callInt(String method, [Map<String, dynamic>? params]) {
  final result = _jsCall(method, params);
  if (result == null) return 0;
  if (result.isA<JSNumber>()) return (result as JSNumber).toDartInt;
  return 0;
}

/// Helper: call bridge and expect a Map result.
Map<String, dynamic> _callMap(String method, [Map<String, dynamic>? params]) {
  final result = _call(method, params);
  if (result is Map<String, dynamic>) return result;
  if (result is Map) return Map<String, dynamic>.from(result);
  return {};
}

// ─── main binding class ───────────────────────────────────────────────────────

/// Web implementation of the wallet API.
/// Calls through to the WalletBridge JS wrapper which dispatches to WASM.
///
/// Same public API as the desktop/mobile WalletCApi (wallet_ffi.dart).
class WalletCApi {
  bool _open = false;

  WalletCApi();

  bool get isOpen => _open;

  // --- version ---

  int apiVersion() => _callInt('apiVersion');

  String versionString() => _callStr('versionString');

  // --- lifecycle ---

  Future<void> open(
      String filename, String password,
      String daemonHost, int daemonPort,
      {bool ssl = false, int syncThreads = 0}) async {
    // On web, the JS bridge handles IndexedDB → WASM in-memory store loading
    _call('open', {
      'filename': filename,
      'password': password,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
  }

  Future<void> create(
      String filename, String password,
      String daemonHost, int daemonPort,
      {bool ssl = false, int syncThreads = 0}) async {
    _call('create', {
      'filename': filename,
      'password': password,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
  }

  Future<void> restoreFromSeed(
      String mnemonicSeed,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    _call('restoreFromSeed', {
      'mnemonicSeed': mnemonicSeed,
      'filename': filename,
      'password': password,
      'scanHeight': scanHeight,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
  }

  Future<void> restoreFromKeys(
      String privateSpendKey, String privateViewKey,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    _call('restoreFromKeys', {
      'privateSpendKey': privateSpendKey,
      'privateViewKey': privateViewKey,
      'filename': filename,
      'password': password,
      'scanHeight': scanHeight,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
  }

  Future<void> restoreViewWallet(
      String privateViewKey, String address,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    _call('restoreViewWallet', {
      'privateViewKey': privateViewKey,
      'address': address,
      'filename': filename,
      'password': password,
      'scanHeight': scanHeight,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
  }

  void close() {
    if (_open) {
      _call('close');
      _open = false;
    }
  }

  Future<void> save() async {
    _call('save');
  }

  Future<void> changePassword(String newPassword) async {
    _call('changePassword', {'newPassword': newPassword});
  }

  Future<String> exportJson() async {
    final result = _call('exportJson');
    if (result is String) return result;
    return jsonEncode(result);
  }

  Future<void> deleteFile(String filename) async {
    _call('deleteFile', {'filename': filename});
  }

  // --- sync / node ---

  Future<Map<String, int>> getSyncStatus() async {
    final m = _callMap('getSyncStatus');
    return {
      'walletHeight': (m['walletHeight'] as num?)?.toInt() ?? 0,
      'localDaemonHeight': (m['localDaemonHeight'] as num?)?.toInt() ?? 0,
      'networkHeight': (m['networkHeight'] as num?)?.toInt() ?? 0,
    };
  }

  Future<bool> isDaemonOnline() async {
    return _callBool('isDaemonOnline');
  }

  Future<Map<String, dynamic>> getStatusJson() async {
    return _callMap('getStatusJson');
  }

  Future<Map<String, dynamic>> getNodeInfoJson() async {
    return _callMap('getNodeInfoJson');
  }

  Future<void> swapNode(String host, int port, {bool ssl = false}) async {
    _call('swapNode', {
      'daemonHost': host,
      'daemonPort': port,
      'daemonSsl': ssl,
    });
  }

  Future<void> reset({int scanHeight = 0, int timestamp = 0}) async {
    _call('reset', {
      'scanHeight': scanHeight,
      'timestamp': timestamp,
    });
  }

  // --- balances ---

  Future<({int unlocked, int locked})> getTotalBalance() async {
    final m = _callMap('getTotalBalance');
    return (
      unlocked: (m['unlocked'] as num?)?.toInt() ?? 0,
      locked: (m['locked'] as num?)?.toInt() ?? 0,
    );
  }

  Future<({int unlocked, int locked})> getBalanceForAddress(
      String address) async {
    final m = _callMap('getBalanceForAddress', {'address': address});
    return (
      unlocked: (m['unlocked'] as num?)?.toInt() ?? 0,
      locked: (m['locked'] as num?)?.toInt() ?? 0,
    );
  }

  Future<Map<String, dynamic>> getBalancesJson() async {
    return _callMap('getBalancesJson');
  }

  // --- addresses ---

  Future<String> getPrimaryAddress() async {
    return _callStr('getPrimaryAddress');
  }

  Future<Map<String, dynamic>> getAddressesJson() async {
    return _callMap('getAddressesJson');
  }

  // --- transactions ---

  Future<Map<String, dynamic>> getTransactionsJson({
    int startHeight = 0,
    int endHeight = 0,
    bool includeUnconfirmed = true,
  }) async {
    return _callMap('getTransactionsJson', {
      'startHeight': startHeight,
      'endHeight': endHeight,
      'includeUnconfirmed': includeUnconfirmed,
    });
  }

  Future<String> sendBasic(String destination, int amount,
      {String paymentId = '', bool sendAll = false,
      bool broadcast = true}) async {
    return _callStr('sendBasic', {
      'destination': destination,
      'amount': amount,
      'paymentId': paymentId,
      'sendAll': sendAll,
      'broadcast': broadcast,
    });
  }

  Future<String> sendPrepared(String preparedTxHash) async {
    return _callStr('sendPrepared', {
      'preparedTxHash': preparedTxHash,
    });
  }

  Future<Map<String, dynamic>> sendAdvanced(String requestJson,
      {bool broadcast = true}) async {
    return _callMap('sendAdvancedJson', {
      'requestJson': requestJson,
      'broadcast': broadcast,
    });
  }

  Future<Map<String, dynamic>> getTransactionsStatus(
      String requestJson) async {
    return _callMap('getTransactionsStatusJson', {
      'requestJson': requestJson,
    });
  }

  Future<String> getTxPrivateKey(String txHash) async {
    return _callStr('getTxPrivateKey', {'txHash': txHash});
  }

  // --- sweep ---

  Future<Map<String, dynamic>> sweepToAddress(String destination,
      {String paymentId = '', int amountToSweep = 0}) async {
    return _callMap('sweepToAddress', {
      'destination': destination,
      'paymentId': paymentId,
      'amountToSweep': amountToSweep,
    });
  }

  Future<({int txCount, int totalFee})> estimateSweep(
      {int amountToSweep = 0}) async {
    final m = _callMap('estimateSweep', {
      'amountToSweep': amountToSweep,
    });
    return (
      txCount: (m['txCount'] as num?)?.toInt() ?? 0,
      totalFee: (m['totalFee'] as num?)?.toInt() ?? 0,
    );
  }

  // --- keys / seeds ---

  Future<String> getPrivateViewKey() async {
    return _callStr('getPrivateViewKey');
  }

  Future<Map<String, dynamic>> getSpendKeysJson(String address) async {
    return _callMap('getSpendKeysJson', {'address': address});
  }

  Future<String> getMnemonicSeed() async {
    return _callStr('getMnemonicSeed');
  }

  Future<String> getMnemonicSeedForAddress(String address) async {
    return _callStr('getMnemonicSeedForAddress', {'address': address});
  }

  Future<bool> isViewWallet() async {
    return _callBool('isViewWallet');
  }

  // --- subwallets ---

  Future<Map<String, dynamic>> addSubwallet() async {
    return _callMap('addSubwallet');
  }

  Future<String> importSubwalletFromKey(String privateSpendKeyHex,
      {int scanHeight = 0}) async {
    return _callStr('importSubwalletFromKey', {
      'privateSpendKey': privateSpendKeyHex,
      'scanHeight': scanHeight,
    });
  }

  Future<String> importSubwalletFromIndex(int walletIndex,
      {int scanHeight = 0}) async {
    return _callStr('importSubwalletFromIndex', {
      'walletIndex': walletIndex,
      'scanHeight': scanHeight,
    });
  }

  Future<void> deleteSubwallet(String address) async {
    _call('deleteSubwallet', {'address': address});
  }

  // --- integrated address ---

  Future<String> createIntegratedAddress(
      String address, String paymentId) async {
    return _callStr('createIntegratedAddress', {
      'address': address,
      'paymentId': paymentId,
    });
  }

  // --- events ---

  ({WalletEvent type, Map<String, dynamic> data})? pollEvent(
      {int timeoutMs = 0}) {
    final m = _callMap('pollEvent', {'timeoutMs': timeoutMs});
    final evType = (m['eventType'] as num?)?.toInt() ?? 0;
    final t = WalletEvent.fromInt(evType);
    if (t == WalletEvent.none) return null;
    final data = m['eventData'];
    return (
      type: t,
      data: data is Map<String, dynamic> ? data : <String, dynamic>{},
    );
  }

  // --- logging ---

  void setLogLevel(String level) {
    _call('setLogLevel', {'level': level});
  }

  Map<String, dynamic> takeLogs() => _callMap('takeLogsJson');

  void clearLogs() => _call('clearLogs');

  // --- TX PoW progress ---

  ({bool active, int elapsedMs, int nonces}) getPowStatus() {
    final m = _callMap('getPowStatus');
    return (
      active: m['active'] == true,
      elapsedMs: (m['elapsedMs'] as num?)?.toInt() ?? 0,
      nonces: (m['nonces'] as num?)?.toInt() ?? 0,
    );
  }

  // --- scan coinbase ---

  void setScanCoinbase(bool scan) {
    _call('setScanCoinbase', {'scan': scan});
  }

  // --- error helpers ---

  String errorCodeToString(int code) {
    // Not directly available via WASM bridge — provide a basic fallback
    return 'Error code: $code';
  }
}
