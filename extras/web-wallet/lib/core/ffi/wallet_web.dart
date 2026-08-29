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

import 'dart:async';
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

/// Thrown when the WASM module never becomes usable — a missing/failed
/// `wallet_wasm.wasm`, a worker that crashed, or missing COOP/COEP headers.
class WalletEngineUnavailableException extends WalletCApiException {
  WalletEngineUnavailableException(String message) : super(-2, message);
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

/// Non-null when the bridge failed to initialise; holds the reason.
@JS('walletBridgeError')
external JSString? get _walletBridgeErrorFlag;

/// Dismisses the boot splash in index.html.
@JS('_bootDone')
external void _bootDone();

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

/// How long to wait for the WASM module before giving up.
///
/// The previous implementation polled forever, so a missing `wallet_wasm.wasm`
/// or a server without COOP/COEP headers showed as a spinner that never
/// resolved and no error anywhere in the UI.
const Duration kBridgeReadyTimeout = Duration(seconds: 90);

/// Dismisses the HTML boot splash. Safe to call more than once.
void dismissBootSplash() {
  try {
    _bootDone();
  } catch (_) {
    // Splash already removed, or index.html predates the helper.
  }
}

/// Waits until the WASM wallet module is fully loaded in the worker.
/// Resolves immediately if already ready; polls at 100 ms intervals otherwise.
///
/// Throws [WalletEngineUnavailableException] if the bridge reports a failure or
/// does not come up within [kBridgeReadyTimeout].
Future<void> _waitForBridge() async {
  final deadline = DateTime.now().add(kBridgeReadyTimeout);

  for (;;) {
    final ready = _walletBridgeReadyFlag;
    if (ready != null && ready.toDart) return;

    final error = _walletBridgeErrorFlag;
    if (error != null) {
      throw WalletEngineUnavailableException(
        'The wallet engine failed to load: ${error.toDart}',
      );
    }

    if (DateTime.now().isAfter(deadline)) {
      throw WalletEngineUnavailableException(
        'The wallet engine did not start within '
        '${kBridgeReadyTimeout.inSeconds}s. Check that wallet_wasm.js and '
        'wallet_wasm.wasm are served next to index.html, and that the server '
        'sends the Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy '
        'headers.',
      );
    }

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

/// Invokes a named method on the JS bridge with up to two positional
/// arguments. Reserved for the handful of entry points that genuinely need a
/// non-object argument (the DOM download, event subscription).
Future<dynamic> _callNamed(String method, [JSAny? arg1, JSAny? arg2]) async {
  await _waitForBridge();
  final promise = _jsBridge.callMethod(method.toJS, arg1, arg2);
  try {
    if (promise == null || !promise.isA<JSPromise>()) return null;
    final result = await (promise as JSPromise).toDart;
    if (result == null) return null;
    final jsonStr = _jsonStringify(result);
    return jsonDecode(jsonStr.toDart);
  } catch (e) {
    _throwJsError(e);
  }
}

/// Runs a stateful (IndexedDB-touching) operation on the worker.
///
/// Everything goes through the bridge's uniform `request(method, params)`
/// entry point. Calling the bridge's named methods instead meant matching each
/// one's arity by hand, and a single mismatch — an options object passed where
/// a bare string was expected — was enough to silently break wallet deletion.
Future<dynamic> _request(String method, [Map<String, dynamic>? params]) async {
  await _waitForBridge();
  final paramsObj = _jsonParse(jsonEncode(params ?? const {}).toJS);
  final promise = _jsBridge.callMethod('request'.toJS, method.toJS, paramsObj);
  try {
    final result = await (promise as JSPromise).toDart;
    if (result == null) return null;
    final jsonStr = _jsonStringify(result);
    return jsonDecode(jsonStr.toDart);
  } catch (e) {
    _throwJsError(e);
  }
}

/// [_request] coercing the result to a map.
Future<Map<String, dynamic>> _requestMap(String method,
    [Map<String, dynamic>? params]) async {
  final result = await _request(method, params);
  if (result is Map<String, dynamic>) return result;
  if (result is Map) return Map<String, dynamic>.from(result);
  return {};
}

/// [_request] coercing the result to a string.
Future<String> _requestStr(String method, [Map<String, dynamic>? params]) async {
  final result = await _request(method, params);
  if (result == null) return '';
  return result is String ? result : jsonEncode(result);
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

  /// Broadcasts wallet events pushed from the WASM module, so the UI can react
  /// to a new transaction or a completed sync instead of polling for them.
  final StreamController<({WalletEvent type, Map<String, dynamic> data})>
      _events = StreamController.broadcast();

  bool _eventsStarted = false;

  WalletCApi();

  bool get isOpen => _open;

  /// Wallet events (synced / incoming transaction) pushed from WASM.
  Stream<({WalletEvent type, Map<String, dynamic> data})> get events =>
      _events.stream;

  /// Resolves once the WASM engine is usable, or throws
  /// [WalletEngineUnavailableException]. Lets the UI show a real error at
  /// startup rather than an indefinite spinner.
  Future<void> ensureEngineReady() => _waitForBridge();

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
    await _request('open', {
      'filename': filename,
      'password': password,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
    await _startEvents();
  }

  Future<void> create(
      String filename, String password,
      String daemonHost, int daemonPort,
      {bool ssl = false, int syncThreads = 0}) async {
    await _request('create', {
      'filename': filename,
      'password': password,
      'daemonHost': daemonHost,
      'daemonPort': daemonPort,
      'daemonSsl': ssl,
      'syncThreads': syncThreads,
    });
    _open = true;
    await _startEvents();
  }

  Future<void> restoreFromSeed(
      String mnemonicSeed,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    await _request('restoreFromSeed', {
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
    await _startEvents();
  }

  Future<void> restoreFromKeys(
      String privateSpendKey, String privateViewKey,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    await _request('restoreFromKeys', {
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
    await _startEvents();
  }

  Future<void> restoreViewWallet(
      String privateViewKey, String address,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    await _request('restoreViewWallet', {
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
    await _startEvents();
  }

  Future<void> close() async {
    if (!_open) return;
    // Mark closed first so pollers stop touching a wallet that is going away.
    _open = false;
    await _stopEvents();
    await _request('close');
  }

  Future<void> save() async {
    await _request('save');
  }

  Future<void> changePassword(String newPassword) async {
    // Routed through the lifecycle channel: the worker re-encrypts *and*
    // re-persists, otherwise IndexedDB keeps the old ciphertext and the new
    // password fails to open the wallet on the next page load.
    await _request('changePassword', {'newPassword': newPassword});
  }

  Future<String> exportJson() async {
    final result = await _call('exportJson');
    if (result is String) return result;
    return jsonEncode(result);
  }

  /// Delete a stored wallet.
  ///
  /// The filename is passed as a bare string. Wrapping it in a map made the
  /// JS proxy wrap it a second time, so IndexedDB received an object where a
  /// key was expected and threw DataError — the delete silently did nothing.
  Future<void> deleteFile(String filename) async {
    await _request('deleteFile', {'filename': filename});
  }

  Future<List<String>> listWallets() async {
    final result = await _request('listWallets');
    if (result is List) return result.map((e) => e.toString()).toList();
    return [];
  }

  /// Save the encrypted wallet file to the user's device.
  Future<void> downloadWallet([String? filename]) async {
    await _callNamed('downloadWallet', filename?.toJS);
  }

  /// Browser storage usage, or null when the API is unavailable.
  Future<({int usage, int quota})?> storageEstimate() async {
    final m = await _request('storageEstimate');
    if (m is! Map) return null;
    return (
      usage: (m['usage'] as num?)?.toInt() ?? 0,
      quota: (m['quota'] as num?)?.toInt() ?? 0,
    );
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
      bool broadcast = true}) {
    return _requestStr('sendBasic', {
      'destination': destination,
      'amount': amount,
      'paymentId': paymentId,
      'sendAll': sendAll,
      'broadcast': broadcast,
    });
  }

  Future<String> sendPrepared(String preparedTxHash) {
    return _requestStr('sendPrepared', {'preparedTxHash': preparedTxHash});
  }

  Future<Map<String, dynamic>> sendAdvanced(String requestJson,
      {bool broadcast = true}) {
    return _requestMap('sendAdvancedJson', {
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
      {String paymentId = '', int amountToSweep = 0}) {
    return _requestMap('sweepToAddress', {
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

  Future<Map<String, dynamic>> addSubwallet() {
    return _requestMap('addSubwallet');
  }

  Future<String> importSubwalletFromKey(String privateSpendKeyHex,
      {int scanHeight = 0}) {
    return _requestStr('importSubwalletFromKey', {
      'privateSpendKey': privateSpendKeyHex,
      'scanHeight': scanHeight,
    });
  }

  Future<String> importSubwalletFromIndex(int walletIndex,
      {int scanHeight = 0}) {
    return _requestStr('importSubwalletFromIndex', {
      'walletIndex': walletIndex,
      'scanHeight': scanHeight,
    });
  }

  Future<void> deleteSubwallet(String address) async {
    await _request('deleteSubwallet', {'address': address});
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

  /// Subscribes to WASM wallet events and republishes them on [events].
  ///
  /// This machinery existed on both sides of the worker but nothing ever
  /// switched it on, so the UI refetched the entire transaction history on a
  /// timer instead of being told when something changed.
  Future<void> _startEvents() async {
    if (_eventsStarted) return;
    _eventsStarted = true;
    try {
      await _waitForBridge();
      final callback = ((JSAny? type, JSAny? data) {
        if (_events.isClosed) return;
        final eventType = type != null && type.isA<JSNumber>()
            ? (type as JSNumber).toDartInt
            : 0;
        if (eventType == 0) return;

        Map<String, dynamic> payload = const {};
        if (data != null) {
          try {
            final decoded = jsonDecode(_jsonStringify(data).toDart);
            if (decoded is Map) payload = Map<String, dynamic>.from(decoded);
          } catch (_) {
            // Malformed payload — the event type alone is still useful.
          }
        }
        _events.add((type: WalletEvent.fromInt(eventType), data: payload));
      }).toJS;

      final promise =
          _jsBridge.callMethod('startEventPolling'.toJS, callback, 1000.toJS);
      if (promise != null && promise.isA<JSPromise>()) {
        await (promise as JSPromise).toDart;
      }
    } catch (_) {
      // Non-fatal: the providers keep a slow polling fallback.
      _eventsStarted = false;
    }
  }

  Future<void> _stopEvents() async {
    if (!_eventsStarted) return;
    _eventsStarted = false;
    try {
      await _callNamed('stopEventPolling');
    } catch (_) {
      // Bridge already gone.
    }
  }

  /// Releases the event stream. Call when the app shuts down.
  Future<void> dispose() async {
    await _stopEvents();
    await _events.close();
  }

  // --- logging ---

  Future<void> setLogLevel(String levelName) async {
    // Map Dart WalletLogLevel enum names to C++ Logger::LogLevel numeric values:
    // disabled=0, fatal=1, warning=2, info=3, debug=4, trace=5
    const nameToLevel = {
      'disabled': 0, 'fatal': 1, 'warning': 2, 'info': 3, 'debug': 4, 'trace': 5,
    };
    final numericLevel = nameToLevel[levelName.toLowerCase()] ?? 3;
    await _call('setLogLevel', {'level': numericLevel});
  }

  Future<Map<String, dynamic>> takeLogsAsync() => _callMap('takeLogsJson');
  // takeLogs() is kept for API compatibility but the log viewer uses takeLogsAsync().
  Map<String, dynamic> takeLogs() => {};

  Future<void> clearLogs() async {
    await _call('clearLogs');
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

  // --- scan coinbase ---

  Future<void> setScanCoinbase(bool scan) async {
    await _call('setScanCoinbase', {'scan': scan});
  }

  // --- error helpers ---

  String errorCodeToString(int code) {
    return 'Error code: $code';
  }
}
