/// wallet_web.dart
///
/// Web implementation of the wallet API using dart:js_interop to call
/// the WalletBridgeWorker JavaScript wrapper which delegates all WASM
/// calls to a dedicated Web Worker — keeping the main browser thread
/// responsive.
///
/// This file provides the same public API as wallet_ffi.dart (desktop/mobile)
/// so that the rest of the Flutter app (providers, screens) can use either
/// implementation transparently.
///
/// Usage:
///   final api = WalletCApi();
///   await api.init();  // spawns the web worker
///   await api.create('my_wallet', 'pass', 'node-fin.wrkz.work', 443, ssl: true);
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

/// The global walletBridgeWorker instance (WalletBridgeWorker from wallet_bridge.js).
/// Initialised by index.html before Flutter starts.
@JS('walletBridge')
external JSObject get _jsBridge;

/// True once walletBridge.init() has finished (WASM loaded inside the worker).
@JS('walletBridgeReady')
external JSBoolean? get _walletBridgeReadyFlag;

/// Extract a readable message from a JS exception (Error or plain value).
@JS('_extractJsError')
external JSString _extractJsError(JSAny? err);

/// Throws [WalletCApiException] with a readable message extracted from [e],
/// which may be a raw JS Error thrown by a rejected Promise.
Never _throwJsError(Object e) {
  String msg;
  try {
    msg = _extractJsError((e as JSAny?)).toDart;
  } catch (_) {
    msg = e.toString();
  }
  throw WalletCApiException(-1, msg);
}

/// Waits until the WASM wallet module is fully loaded in the worker.
/// Resolves immediately if already ready; polls at 100 ms intervals otherwise.
Future<void> _waitForBridge() async {
  while (_walletBridgeReadyFlag == null || !_walletBridgeReadyFlag!.toDart) {
    await Future<void>.delayed(const Duration(milliseconds: 100));
  }
}

/// Call a method on the bridge that returns a JS Promise.
/// Every method on WalletBridgeWorker is async.
Future<JSAny?> _callAsync(String method, [Map<String, dynamic>? params]) async {
  await _waitForBridge();
  final paramsStr = params != null ? jsonEncode(params) : '{}';
  final paramsObj = _jsonParse(paramsStr.toJS);
  // bridge.call(method, params) returns a Promise
  final promise = _jsBridge.callMethod('call'.toJS, method.toJS, paramsObj);
  try {
    return await (promise as JSPromise).toDart;
  } catch (e) {
    _throwJsError(e);
  }
}

/// Helper: call bridge and parse the Promise result as a Dart object.
Future<dynamic> _call(String method, [Map<String, dynamic>? params]) async {
  final result = await _callAsync(method, params);
  if (result == null) return null;
  final jsonStr = _jsonStringify(result);
  return jsonDecode(jsonStr.toDart);
}

/// Helper: call bridge and expect a string result.
Future<String> _callStr(String method, [Map<String, dynamic>? params]) async {
  final result = await _callAsync(method, params);
  if (result == null) return '';
  if (result.isA<JSString>()) return (result as JSString).toDart;
  return _jsonStringify(result).toDart;
}

/// Helper: call bridge and expect a bool result.
Future<bool> _callBool(String method, [Map<String, dynamic>? params]) async {
  final result = await _callAsync(method, params);
  if (result == null) return false;
  if (result.isA<JSBoolean>()) return (result as JSBoolean).toDart;
  return false;
}

/// Helper: call bridge and expect an int result.
Future<int> _callInt(String method, [Map<String, dynamic>? params]) async {
  final result = await _callAsync(method, params);
  if (result == null) return 0;
  if (result.isA<JSNumber>()) return (result as JSNumber).toDartInt;
  return 0;
}

/// Helper: call bridge and expect a Map result.
Future<Map<String, dynamic>> _callMap(String method, [Map<String, dynamic>? params]) async {
  final result = await _call(method, params);
  if (result is Map<String, dynamic>) return result;
  if (result is Map) return Map<String, dynamic>.from(result);
  return {};
}

/// For lifecycle methods that need the bridge's async JS methods directly
/// (create, open, etc. involve IndexedDB).
Future<dynamic> _callLifecycle(String method, Map<String, dynamic> params) async {
  await _waitForBridge();
  final paramsStr = jsonEncode(params);
  final paramsObj = _jsonParse(paramsStr.toJS);
  // Use the specific lifecycle method on the bridge (e.g. bridge.create(opts))
  final promise = _jsBridge.callMethod(method.toJS, paramsObj);
  try {
    final result = await (promise as JSPromise).toDart;
    if (result == null) return null;
    final jsonStr = _jsonStringify(result);
    return jsonDecode(jsonStr.toDart);
  } catch (e) {
    _throwJsError(e);
  }
}

// ─── JS interop extension for callMethod ─────────────────────────────────────

extension _JSObjectCallMethod on JSObject {
  /// Call a method on a JS object by name.
  /// Works with 0–2 arguments.
  JSAny? callMethod(JSString name, [JSAny? arg1, JSAny? arg2]) {
    return _jsCallMethod(this, name, arg1, arg2);
  }
}

@JS('_dartCallMethod')
external JSAny? _jsCallMethod(JSObject obj, JSString name, JSAny? arg1, JSAny? arg2);

// ─── main binding class ───────────────────────────────────────────────────────

/// Web implementation of the wallet API.
/// Calls through to the WalletBridgeWorker JS wrapper which delegates
/// all WASM calls to a dedicated Web Worker.
///
/// Same public API as the desktop/mobile WalletCApi (wallet_ffi.dart).
class WalletCApi {
  bool _open = false;

  WalletCApi();

  bool get isOpen => _open;

  // --- version ---

  Future<int> apiVersionAsync() => _callInt('apiVersion');
  int apiVersion() => 0; // synchronous fallback; use apiVersionAsync
  String versionString() => ''; // synchronous fallback; use versionStringAsync
  Future<String> versionStringAsync() => _callStr('versionString');

  // --- lifecycle ---

  Future<void> open(
      String filename, String password,
      String daemonHost, int daemonPort,
      {bool ssl = false, int syncThreads = 0}) async {
    await _callLifecycle('open', {
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
    await _callLifecycle('create', {
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
    await _callLifecycle('restoreFromSeed', {
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
    await _callLifecycle('restoreFromKeys', {
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
    await _callLifecycle('restoreViewWallet', {
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

  Future<void> close() async {
    if (_open) {
      await _callLifecycle('close', {});
      _open = false;
    }
  }

  Future<void> save() async {
    await _callLifecycle('save', {});
  }

  Future<void> changePassword(String newPassword) async {
    await _call('changePassword', {'newPassword': newPassword});
  }

  Future<String> exportJson() async {
    final result = await _call('exportJson');
    if (result is String) return result;
    return jsonEncode(result);
  }

  Future<void> deleteFile(String filename) async {
    await _callLifecycle('deleteFile', {'filename': filename});
  }

  // --- sync / node ---

  Future<Map<String, int>> getSyncStatus() async {
    final m = await _callMap('getSyncStatus');
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
    await _call('swapNode', {
      'daemonHost': host,
      'daemonPort': port,
      'daemonSsl': ssl,
    });
  }

  Future<void> reset({int scanHeight = 0, int timestamp = 0}) async {
    await _call('reset', {
      'scanHeight': scanHeight,
      'timestamp': timestamp,
    });
  }

  // --- balances ---

  Future<({int unlocked, int locked})> getTotalBalance() async {
    final m = await _callMap('getTotalBalance');
    return (
      unlocked: (m['unlocked'] as num?)?.toInt() ?? 0,
      locked: (m['locked'] as num?)?.toInt() ?? 0,
    );
  }

  Future<({int unlocked, int locked})> getBalanceForAddress(
      String address) async {
    final m = await _callMap('getBalanceForAddress', {'address': address});
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
    final m = await _callMap('estimateSweep', {
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
    await _call('deleteSubwallet', {'address': address});
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
    // pollEvent is still synchronous for compatibility with the polling timer.
    // In worker mode, events arrive via callbacks instead.
    return null;
  }

  // --- logging ---

  void setLogLevel(String level) {
    _call('setLogLevel', {'level': level});
  }

  Future<Map<String, dynamic>> takeLogsAsync() => _callMap('takeLogsJson');
  Map<String, dynamic> takeLogs() => {}; // sync fallback

  void clearLogs() {
    _call('clearLogs');
  }

  // --- TX PoW progress ---

  Future<({bool active, int elapsedMs, int nonces})> getPowStatusAsync() async {
    final m = await _callMap('getPowStatus');
    return (
      active: m['active'] == true,
      elapsedMs: (m['elapsedMs'] as num?)?.toInt() ?? 0,
      nonces: (m['nonces'] as num?)?.toInt() ?? 0,
    );
  }

  ({bool active, int elapsedMs, int nonces}) getPowStatus() {
    // Sync fallback for UI polling — returns inactive
    return (active: false, elapsedMs: 0, nonces: 0);
  }

  // --- scan coinbase ---

  void setScanCoinbase(bool scan) {
    _call('setScanCoinbase', {'scan': scan});
  }

  // --- error helpers ---

  String errorCodeToString(int code) {
    return 'Error code: $code';
  }
}
