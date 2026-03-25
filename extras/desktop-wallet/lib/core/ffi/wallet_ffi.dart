/// wallet_ffi.dart
///
/// Dart FFI binding for the wallet_capi shared library (wallet_capi.dll /
/// libwallet_capi.so / libwallet_capi.dylib).
///
/// Place the compiled library next to the Flutter executable before running:
///   Windows : wallet_capi.dll
///   Linux   : libwallet_capi.so
///   macOS   : libwallet_capi.dylib
///
/// Usage:
///   final api = WalletCApi();
///   await api.create('my.wallet', 'pass', 'nodes.wrkz.work', 17856);
///   // ... use api ...
///   api.close();
library;

import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:ffi/ffi.dart';

// ─── opaque handle ────────────────────────────────────────────────────────────

final class _WalletHandleOpaque extends Opaque {}

typedef _HandlePtr = Pointer<_WalletHandleOpaque>;
typedef _HandlePtrPtr = Pointer<Pointer<_WalletHandleOpaque>>;

// ─── native function typedefs ─────────────────────────────────────────────────

// version
typedef _FnVersionNative = Uint32 Function();
typedef _FnVersionDart = int Function();
typedef _FnVersionStrNative = Pointer<Utf8> Function();
typedef _FnVersionStrDart = Pointer<Utf8> Function();

// wallet_open / wallet_create  (filename, password, host, port, ssl, threads, **out)
typedef _FnOpenNative = Int32 Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Uint16, Bool, Uint32,
    _HandlePtrPtr);
typedef _FnOpenDart = int Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, int, bool, int,
    _HandlePtrPtr);

// wallet_restore_from_seed (seed, filename, password, scanH, host, port, ssl, threads, **out)
typedef _FnRestoreSeedNative = Int32 Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Uint64, Pointer<Utf8>,
    Uint16, Bool, Uint32, _HandlePtrPtr);
typedef _FnRestoreSeedDart = int Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, int, Pointer<Utf8>,
    int, bool, int, _HandlePtrPtr);

// wallet_restore_from_keys (spendKey, viewKey, filename, password, scanH, host, port, ssl, threads, **out)
typedef _FnRestoreKeysNative = Int32 Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Uint64,
    Pointer<Utf8>, Uint16, Bool, Uint32, _HandlePtrPtr);
typedef _FnRestoreKeysDart = int Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, int,
    Pointer<Utf8>, int, bool, int, _HandlePtrPtr);

// wallet_restore_view (viewKey, address, filename, password, scanH, host, port, ssl, threads, **out)
typedef _FnRestoreViewNative = Int32 Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Uint64,
    Pointer<Utf8>, Uint16, Bool, Uint32, _HandlePtrPtr);
typedef _FnRestoreViewDart = int Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, int,
    Pointer<Utf8>, int, bool, int, _HandlePtrPtr);

// wallet_delete_file (filename)
typedef _FnDeleteFileNative = Int32 Function(Pointer<Utf8>);
typedef _FnDeleteFileDart = int Function(Pointer<Utf8>);

// wallet_close (handle)
typedef _FnCloseNative = Void Function(_HandlePtr);
typedef _FnCloseDart = void Function(_HandlePtr);

// wallet_save / wallet_is_view_wallet / wallet_change_password
typedef _FnHandleOnlyNative = Int32 Function(_HandlePtr);
typedef _FnHandleOnlyDart = int Function(_HandlePtr);

typedef _FnChangePwNative = Int32 Function(_HandlePtr, Pointer<Utf8>);
typedef _FnChangePwDart = int Function(_HandlePtr, Pointer<Utf8>);

// wallet_get_sync_status
typedef _FnSyncStatusNative = Int32 Function(
    _HandlePtr, Pointer<Uint64>, Pointer<Uint64>, Pointer<Uint64>);
typedef _FnSyncStatusDart = int Function(
    _HandlePtr, Pointer<Uint64>, Pointer<Uint64>, Pointer<Uint64>);

// wallet_daemon_online
typedef _FnDaemonOnlineNative = Int32 Function(_HandlePtr, Pointer<Bool>);
typedef _FnDaemonOnlineDart = int Function(_HandlePtr, Pointer<Bool>);

// wallet_is_view_wallet
typedef _FnIsViewNative = Int32 Function(_HandlePtr, Pointer<Bool>);
typedef _FnIsViewDart = int Function(_HandlePtr, Pointer<Bool>);

// wallet_get_total_balance
typedef _FnTotalBalanceNative = Int32 Function(
    _HandlePtr, Pointer<Uint64>, Pointer<Uint64>);
typedef _FnTotalBalanceDart = int Function(
    _HandlePtr, Pointer<Uint64>, Pointer<Uint64>);

// wallet_get_balance_for_address
typedef _FnAddressBalanceNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Uint64>, Pointer<Uint64>);
typedef _FnAddressBalanceDart = int Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Uint64>, Pointer<Uint64>);

// common JSON-out pattern: (handle, **out_json, *out_len)
typedef _FnJsonOutNative = Int32 Function(
    _HandlePtr, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnJsonOutDart = int Function(
    _HandlePtr, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_get_transactions_json (handle, startH, endH, includeUnconfirmed, **out, *len)
typedef _FnTxJsonNative = Int32 Function(
    _HandlePtr, Uint64, Uint64, Bool, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnTxJsonDart = int Function(
    _HandlePtr, int, int, bool, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_send_basic (handle, dest, amount, paymentId, sendAll, sendTx, **outHash, *len)
typedef _FnSendBasicNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Uint64, Pointer<Utf8>, Bool, Bool,
    Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnSendBasicDart = int Function(
    _HandlePtr, Pointer<Utf8>, int, Pointer<Utf8>, bool, bool,
    Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_send_prepared (handle, preparedHashHex, **outHash, *len)
typedef _FnSendPreparedNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnSendPreparedDart = int Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_send_advanced_json (handle, requestJson, sendTx, **outResult, *len)
typedef _FnSendAdvancedNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Bool, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnSendAdvancedDart = int Function(
    _HandlePtr, Pointer<Utf8>, bool, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_get_spend_keys_json / wallet_get_mnemonic_seed_for_address (handle, address, **out, *len)
typedef _FnWithAddressNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnWithAddressDart = int Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_get_tx_private_key / wallet_get_transactions_status_json (handle, hashHex, **out, *len)
typedef _FnWithHashNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnWithHashDart = int Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_swap_node
typedef _FnSwapNodeNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Uint16, Bool);
typedef _FnSwapNodeDart = int Function(
    _HandlePtr, Pointer<Utf8>, int, bool);

// wallet_reset
typedef _FnResetNative = Int32 Function(_HandlePtr, Uint64, Uint64);
typedef _FnResetDart = int Function(_HandlePtr, int, int);

// wallet_import_subwallet_from_key
typedef _FnImportSubKeyNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Uint64, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnImportSubKeyDart = int Function(
    _HandlePtr, Pointer<Utf8>, int, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_import_subwallet_from_index
typedef _FnImportSubIndexNative = Int32 Function(
    _HandlePtr, Uint64, Uint64, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnImportSubIndexDart = int Function(
    _HandlePtr, int, int, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_delete_subwallet
typedef _FnDeleteSubNative = Int32 Function(_HandlePtr, Pointer<Utf8>);
typedef _FnDeleteSubDart = int Function(_HandlePtr, Pointer<Utf8>);

// wallet_create_integrated_address (address, paymentId, **out, *len) — no handle
typedef _FnIntegratedAddrNative = Int32 Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnIntegratedAddrDart = int Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_poll_event (handle, timeoutMs, *outType, **outJson, *outLen)
typedef _FnPollEventNative = Int32 Function(
    _HandlePtr, Uint32, Pointer<Uint32>, Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnPollEventDart = int Function(
    _HandlePtr, int, Pointer<Uint32>, Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_sweep_to_address (handle, dest, paymentId, amountToSweep, **out, *len)
typedef _FnSweepNative = Int32 Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Utf8>, Uint64,
    Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnSweepDart = int Function(
    _HandlePtr, Pointer<Utf8>, Pointer<Utf8>, int,
    Pointer<Pointer<Utf8>>, Pointer<Size>);

// wallet_estimate_sweep (handle, amountToSweep, *txCount, *totalFee)
typedef _FnEstimateSweepNative = Int32 Function(
    _HandlePtr, Uint64, Pointer<Uint64>, Pointer<Uint64>);
typedef _FnEstimateSweepDart = int Function(
    _HandlePtr, int, Pointer<Uint64>, Pointer<Uint64>);

// wallet_string_free
typedef _FnStringFreeNative = Void Function(Pointer<Utf8>);
typedef _FnStringFreeDart = void Function(Pointer<Utf8>);

// wallet_error_code_to_string / wallet_last_error_message
typedef _FnConstStrFromIntNative = Pointer<Utf8> Function(Int32);
typedef _FnConstStrFromIntDart = Pointer<Utf8> Function(int);
typedef _FnConstStrNative = Pointer<Utf8> Function();
typedef _FnConstStrDart = Pointer<Utf8> Function();
typedef _FnVoidNative = Void Function();
typedef _FnVoidDart = void Function();

// wallet_set_log_level (level string)
typedef _FnSetLogLevelNative = Int32 Function(Pointer<Utf8>);
typedef _FnSetLogLevelDart = int Function(Pointer<Utf8>);

// wallet_take_logs_json / wallet_clear_logs
typedef _FnTakeLogsNative = Int32 Function(
    Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnTakeLogsDart = int Function(
    Pointer<Pointer<Utf8>>, Pointer<Size>);
typedef _FnClearLogsNative = Int32 Function();
typedef _FnClearLogsDart = int Function();

// wallet_get_pow_status (out_active, out_elapsed_ms, out_nonces)
typedef _FnPowStatusNative = Void Function(
    Pointer<Bool>, Pointer<Uint64>, Pointer<Uint64>);
typedef _FnPowStatusDart = void Function(
    Pointer<Bool>, Pointer<Uint64>, Pointer<Uint64>);

// wallet_set_scan_coinbase(bool scan)
typedef _FnSetScanCoinbaseNative = Void Function(Bool);
typedef _FnSetScanCoinbaseDart = void Function(bool);

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

// ─── main binding class ───────────────────────────────────────────────────────

/// Wraps the wallet_capi shared library.
/// One instance per application; call [open]/[create]/[restoreFrom*] before
/// any other wallet operations, and [close] when done.
class WalletCApi {
  late final DynamicLibrary _lib;
  _HandlePtr? _handle;

  // ── bound functions ───────────────────────────────────────────────────────
  // Bound functions — only those used on the main thread.
  // Heavy ops (open/create/restore/send/sweep/save) run in Isolate.run()
  // and bind their own functions from the shared library directly.
  late final _FnVersionDart _apiVersion;
  late final _FnVersionStrDart _versionString;
  late final _FnDeleteFileDart _walletDeleteFile;
  late final _FnCloseDart _walletClose;
  late final _FnChangePwDart _walletChangePassword;
  late final _FnJsonOutDart _walletExportJson;
  late final _FnSyncStatusDart _walletGetSyncStatus;
  late final _FnDaemonOnlineDart _walletDaemonOnline;
  late final _FnJsonOutDart _walletGetStatusJson;
  late final _FnJsonOutDart _walletGetNodeInfoJson;
  late final _FnSwapNodeDart _walletSwapNode;
  late final _FnResetDart _walletReset;
  late final _FnTotalBalanceDart _walletGetTotalBalance;
  late final _FnAddressBalanceDart _walletGetBalanceForAddress;
  late final _FnJsonOutDart _walletGetBalancesJson;
  late final _FnJsonOutDart _walletGetAddressesJson;
  late final _FnJsonOutDart _walletGetPrimaryAddress;
  late final _FnTxJsonDart _walletGetTransactionsJson;
  late final _FnWithHashDart _walletGetTxPrivateKey;
  late final _FnWithHashDart _walletGetTransactionsStatusJson;
  late final _FnJsonOutDart _walletGetPrivateViewKey;
  late final _FnWithAddressDart _walletGetSpendKeysJson;
  late final _FnJsonOutDart _walletGetMnemonicSeed;
  late final _FnWithAddressDart _walletGetMnemonicSeedForAddress;
  late final _FnIsViewDart _walletIsViewWallet;
  late final _FnJsonOutDart _walletAddSubwalletJson;
  late final _FnImportSubKeyDart _walletImportSubwalletFromKey;
  late final _FnImportSubIndexDart _walletImportSubwalletFromIndex;
  late final _FnDeleteSubDart _walletDeleteSubwallet;
  late final _FnIntegratedAddrDart _walletCreateIntegratedAddress;
  late final _FnPollEventDart _walletPollEvent;
  late final _FnStringFreeDart _walletStringFree;
  late final _FnConstStrFromIntDart _walletErrorCodeToString;
  late final _FnConstStrDart _walletLastErrorMessage;
  late final _FnVoidDart _walletClearLastError;
  late final _FnSetLogLevelDart _walletSetLogLevel;
  late final _FnTakeLogsDart _walletTakeLogsJson;
  late final _FnClearLogsDart _walletClearLogs;
  late final _FnPowStatusDart _walletGetPowStatus;
  _FnSetScanCoinbaseDart? _walletSetScanCoinbase;

  bool get isOpen => _handle != null && _handle!.address != 0;

  // ── constructor ───────────────────────────────────────────────────────────

  WalletCApi() {
    _lib = _openLibrary();
    _bind();
  }

  static DynamicLibrary _openLibrary() {
    if (Platform.isWindows) return DynamicLibrary.open('wallet_capi.dll');
    if (Platform.isLinux) return DynamicLibrary.open('libwallet_capi.so');
    if (Platform.isMacOS) return DynamicLibrary.open('libwallet_capi.dylib');
    throw UnsupportedError(
        'wallet_capi: unsupported platform ${Platform.operatingSystem}');
  }

  void _bind() {
    _apiVersion = _lib
        .lookupFunction<_FnVersionNative, _FnVersionDart>(
            'wallet_capi_api_version');
    _versionString = _lib
        .lookupFunction<_FnVersionStrNative, _FnVersionStrDart>(
            'wallet_capi_version_string');
    // Heavy ops (open/create/restore/send/sweep/save) are NOT bound here —
    // they run in Isolate.run() and bind directly from the shared library.
    _walletDeleteFile =
        _lib.lookupFunction<_FnDeleteFileNative, _FnDeleteFileDart>(
            'wallet_delete_file');
    _walletClose =
        _lib.lookupFunction<_FnCloseNative, _FnCloseDart>('wallet_close');
    _walletChangePassword =
        _lib.lookupFunction<_FnChangePwNative, _FnChangePwDart>(
            'wallet_change_password');
    _walletExportJson =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_export_json');
    _walletGetSyncStatus =
        _lib.lookupFunction<_FnSyncStatusNative, _FnSyncStatusDart>(
            'wallet_get_sync_status');
    _walletDaemonOnline =
        _lib.lookupFunction<_FnDaemonOnlineNative, _FnDaemonOnlineDart>(
            'wallet_daemon_online');
    _walletGetStatusJson =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_status_json');
    _walletGetNodeInfoJson =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_node_info_json');
    _walletSwapNode =
        _lib.lookupFunction<_FnSwapNodeNative, _FnSwapNodeDart>(
            'wallet_swap_node');
    _walletReset =
        _lib.lookupFunction<_FnResetNative, _FnResetDart>('wallet_reset');
    _walletGetTotalBalance =
        _lib.lookupFunction<_FnTotalBalanceNative, _FnTotalBalanceDart>(
            'wallet_get_total_balance');
    _walletGetBalanceForAddress =
        _lib.lookupFunction<_FnAddressBalanceNative, _FnAddressBalanceDart>(
            'wallet_get_balance_for_address');
    _walletGetBalancesJson =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_balances_json');
    _walletGetAddressesJson =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_addresses_json');
    _walletGetPrimaryAddress =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_primary_address');
    _walletGetTransactionsJson =
        _lib.lookupFunction<_FnTxJsonNative, _FnTxJsonDart>(
            'wallet_get_transactions_json');
    _walletGetTxPrivateKey =
        _lib.lookupFunction<_FnWithHashNative, _FnWithHashDart>(
            'wallet_get_tx_private_key');
    _walletGetTransactionsStatusJson =
        _lib.lookupFunction<_FnWithHashNative, _FnWithHashDart>(
            'wallet_get_transactions_status_json');
    _walletGetPrivateViewKey =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_private_view_key');
    _walletGetSpendKeysJson =
        _lib.lookupFunction<_FnWithAddressNative, _FnWithAddressDart>(
            'wallet_get_spend_keys_json');
    _walletGetMnemonicSeed =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_get_mnemonic_seed');
    _walletGetMnemonicSeedForAddress =
        _lib.lookupFunction<_FnWithAddressNative, _FnWithAddressDart>(
            'wallet_get_mnemonic_seed_for_address');
    _walletIsViewWallet =
        _lib.lookupFunction<_FnIsViewNative, _FnIsViewDart>(
            'wallet_is_view_wallet');
    _walletAddSubwalletJson =
        _lib.lookupFunction<_FnJsonOutNative, _FnJsonOutDart>(
            'wallet_add_subwallet_json');
    _walletImportSubwalletFromKey =
        _lib.lookupFunction<_FnImportSubKeyNative, _FnImportSubKeyDart>(
            'wallet_import_subwallet_from_key');
    _walletImportSubwalletFromIndex =
        _lib.lookupFunction<_FnImportSubIndexNative, _FnImportSubIndexDart>(
            'wallet_import_subwallet_from_index');
    _walletDeleteSubwallet =
        _lib.lookupFunction<_FnDeleteSubNative, _FnDeleteSubDart>(
            'wallet_delete_subwallet');
    _walletCreateIntegratedAddress =
        _lib.lookupFunction<_FnIntegratedAddrNative, _FnIntegratedAddrDart>(
            'wallet_create_integrated_address');
    _walletPollEvent =
        _lib.lookupFunction<_FnPollEventNative, _FnPollEventDart>(
            'wallet_poll_event');
    _walletStringFree =
        _lib.lookupFunction<_FnStringFreeNative, _FnStringFreeDart>(
            'wallet_string_free');
    _walletErrorCodeToString =
        _lib.lookupFunction<_FnConstStrFromIntNative, _FnConstStrFromIntDart>(
            'wallet_error_code_to_string');
    _walletLastErrorMessage =
        _lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
            'wallet_last_error_message');
    _walletClearLastError =
        _lib.lookupFunction<_FnVoidNative, _FnVoidDart>(
            'wallet_clear_last_error_message');
    _walletSetLogLevel =
        _lib.lookupFunction<_FnSetLogLevelNative, _FnSetLogLevelDart>(
            'wallet_set_log_level');
    _walletTakeLogsJson =
        _lib.lookupFunction<_FnTakeLogsNative, _FnTakeLogsDart>(
            'wallet_take_logs_json');
    _walletClearLogs =
        _lib.lookupFunction<_FnClearLogsNative, _FnClearLogsDart>(
            'wallet_clear_logs');
    _walletGetPowStatus =
        _lib.lookupFunction<_FnPowStatusNative, _FnPowStatusDart>(
            'wallet_get_pow_status');
    try {
      _walletSetScanCoinbase =
          _lib.lookupFunction<_FnSetScanCoinbaseNative, _FnSetScanCoinbaseDart>(
              'wallet_set_scan_coinbase');
    } catch (_) {
      _walletSetScanCoinbase = null;
    }
  }

  // ── internal helpers ─────────────────────────────────────────────────────

  String _lastError() => _walletLastErrorMessage().toDartString();

  void _check(int status) {
    if (status != 0) {
      final msg = _lastError();
      _walletClearLastError();
      throw WalletCApiException(status, msg);
    }
  }

  _HandlePtr _requireHandle() {
    final h = _handle;
    if (h == null || h.address == 0) {
      throw StateError('wallet_capi: no wallet is open');
    }
    return h;
  }

  /// Calls a C function that returns a heap-allocated JSON string via
  /// (handle, **outJson, *outLen).  Parses and returns as Map.
  Map<String, dynamic> _jsonOut(
      int Function(_HandlePtr, Pointer<Pointer<Utf8>>, Pointer<Size>) fn) {
    return using((arena) {
      final outStr = arena<Pointer<Utf8>>();
      final outLen = arena<Size>();
      _check(fn(_requireHandle(), outStr, outLen));
      final result = outStr.value.toDartString();
      _walletStringFree(outStr.value);
      return jsonDecode(result) as Map<String, dynamic>;
    });
  }

  /// Same as [_jsonOut] but without a handle (static C functions).
  Map<String, dynamic> _jsonOutStatic(
      int Function(Pointer<Pointer<Utf8>>, Pointer<Size>) fn) {
    return using((arena) {
      final outStr = arena<Pointer<Utf8>>();
      final outLen = arena<Size>();
      _check(fn(outStr, outLen));
      final result = outStr.value.toDartString();
      _walletStringFree(outStr.value);
      return jsonDecode(result) as Map<String, dynamic>;
    });
  }

  /// C function that produces a plain string result (not JSON), e.g. an address.
  String _strOut(
      int Function(_HandlePtr, Pointer<Pointer<Utf8>>, Pointer<Size>) fn) {
    return using((arena) {
      final outStr = arena<Pointer<Utf8>>();
      final outLen = arena<Size>();
      _check(fn(_requireHandle(), outStr, outLen));
      final result = outStr.value.toDartString();
      _walletStringFree(outStr.value);
      return result;
    });
  }

  // ── public API ───────────────────────────────────────────────────────────

  // --- info ---

  int get apiVersion => _apiVersion();
  String get nativeVersion => _versionString().toDartString();

  // --- lifecycle ---

  Future<void> open(
      String filename, String password, String daemonHost, int daemonPort,
      {bool ssl = false, int syncThreads = 0}) async {
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnOpenNative, _FnOpenDart>('wallet_open');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final out = arena<Pointer<_WalletHandleOpaque>>();
        final s = fn(
          filename.toNativeUtf8(allocator: arena),
          password.toNativeUtf8(allocator: arena),
          daemonHost.toNativeUtf8(allocator: arena),
          daemonPort, ssl, syncThreads, out,
        );
        return (
          status: s,
          addr: out.value.address,
          err: s != 0 ? lastErr().toDartString() : '',
        );
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    _handle = Pointer<_WalletHandleOpaque>.fromAddress(r.addr);
  }

  Future<void> create(
      String filename, String password, String daemonHost, int daemonPort,
      {bool ssl = false, int syncThreads = 0}) async {
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnOpenNative, _FnOpenDart>('wallet_create');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final out = arena<Pointer<_WalletHandleOpaque>>();
        final s = fn(
          filename.toNativeUtf8(allocator: arena),
          password.toNativeUtf8(allocator: arena),
          daemonHost.toNativeUtf8(allocator: arena),
          daemonPort, ssl, syncThreads, out,
        );
        return (
          status: s,
          addr: out.value.address,
          err: s != 0 ? lastErr().toDartString() : '',
        );
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    _handle = Pointer<_WalletHandleOpaque>.fromAddress(r.addr);
  }

  Future<void> restoreFromSeed(
      String mnemonicSeed, String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnRestoreSeedNative, _FnRestoreSeedDart>(
          'wallet_restore_from_seed');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final out = arena<Pointer<_WalletHandleOpaque>>();
        final s = fn(
          mnemonicSeed.toNativeUtf8(allocator: arena),
          filename.toNativeUtf8(allocator: arena),
          password.toNativeUtf8(allocator: arena),
          scanHeight,
          daemonHost.toNativeUtf8(allocator: arena),
          daemonPort, ssl, syncThreads, out,
        );
        return (
          status: s,
          addr: out.value.address,
          err: s != 0 ? lastErr().toDartString() : '',
        );
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    _handle = Pointer<_WalletHandleOpaque>.fromAddress(r.addr);
  }

  Future<void> restoreFromKeys(
      String privateSpendKey, String privateViewKey,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnRestoreKeysNative, _FnRestoreKeysDart>(
          'wallet_restore_from_keys');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final out = arena<Pointer<_WalletHandleOpaque>>();
        final s = fn(
          privateSpendKey.toNativeUtf8(allocator: arena),
          privateViewKey.toNativeUtf8(allocator: arena),
          filename.toNativeUtf8(allocator: arena),
          password.toNativeUtf8(allocator: arena),
          scanHeight,
          daemonHost.toNativeUtf8(allocator: arena),
          daemonPort, ssl, syncThreads, out,
        );
        return (
          status: s,
          addr: out.value.address,
          err: s != 0 ? lastErr().toDartString() : '',
        );
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    _handle = Pointer<_WalletHandleOpaque>.fromAddress(r.addr);
  }

  Future<void> restoreViewWallet(
      String privateViewKey, String address,
      String filename, String password,
      String daemonHost, int daemonPort,
      {int scanHeight = 0, bool ssl = false, int syncThreads = 0}) async {
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnRestoreViewNative, _FnRestoreViewDart>(
          'wallet_restore_view');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final out = arena<Pointer<_WalletHandleOpaque>>();
        final s = fn(
          privateViewKey.toNativeUtf8(allocator: arena),
          address.toNativeUtf8(allocator: arena),
          filename.toNativeUtf8(allocator: arena),
          password.toNativeUtf8(allocator: arena),
          scanHeight,
          daemonHost.toNativeUtf8(allocator: arena),
          daemonPort, ssl, syncThreads, out,
        );
        return (
          status: s,
          addr: out.value.address,
          err: s != 0 ? lastErr().toDartString() : '',
        );
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    _handle = Pointer<_WalletHandleOpaque>.fromAddress(r.addr);
  }

  void close() {
    final h = _handle;
    if (h != null && h.address != 0) {
      _walletClose(h);
      _handle = null;
    }
  }

  Future<void> save() async {
    final ha = _requireHandle().address;
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnHandleOnlyNative, _FnHandleOnlyDart>(
          'wallet_save');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      final s = fn(Pointer<_WalletHandleOpaque>.fromAddress(ha));
      return (status: s, err: s != 0 ? lastErr().toDartString() : '');
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
  }

  Future<void> changePassword(String newPassword) =>
      Future(() => using((arena) {
            _check(_walletChangePassword(
              _requireHandle(),
              newPassword.toNativeUtf8(allocator: arena),
            ));
          }));

  /// Returns the full wallet state as a JSON string.
  /// The caller is responsible for writing the string to a file if needed.
  Future<String> exportJson() =>
      Future(() => _strOut(_walletExportJson));

  Future<void> deleteFile(String filename) =>
      Future(() => using((arena) {
            _check(_walletDeleteFile(
                filename.toNativeUtf8(allocator: arena)));
          }));

  // --- sync / node ---

  Future<Map<String, int>> getSyncStatus() => Future(() => using((arena) {
        final wh = arena<Uint64>();
        final lh = arena<Uint64>();
        final nh = arena<Uint64>();
        _check(_walletGetSyncStatus(_requireHandle(), wh, lh, nh));
        return {
          'walletHeight': wh.value,
          'localDaemonHeight': lh.value,
          'networkHeight': nh.value,
        };
      }));

  Future<bool> isDaemonOnline() => Future(() => using((arena) {
        final out = arena<Bool>();
        _check(_walletDaemonOnline(_requireHandle(), out));
        return out.value;
      }));

  /// Returns raw status JSON map (walletBlockCount, networkBlockCount, …)
  Future<Map<String, dynamic>> getStatusJson() =>
      Future(() => _jsonOut(_walletGetStatusJson));

  Future<Map<String, dynamic>> getNodeInfoJson() =>
      Future(() => _jsonOut(_walletGetNodeInfoJson));

  Future<void> swapNode(String host, int port, {bool ssl = false}) =>
      Future(() => using((arena) {
            _check(_walletSwapNode(
              _requireHandle(),
              host.toNativeUtf8(allocator: arena),
              port,
              ssl,
            ));
          }));

  Future<void> reset({int scanHeight = 0, int timestamp = 0}) =>
      Future(() => _check(_walletReset(_requireHandle(), scanHeight, timestamp)));

  // --- balances ---

  Future<({int unlocked, int locked})> getTotalBalance() =>
      Future(() => using((arena) {
            final u = arena<Uint64>();
            final l = arena<Uint64>();
            _check(_walletGetTotalBalance(_requireHandle(), u, l));
            return (unlocked: u.value, locked: l.value);
          }));

  Future<({int unlocked, int locked})> getBalanceForAddress(
          String address) =>
      Future(() => using((arena) {
            final u = arena<Uint64>();
            final l = arena<Uint64>();
            _check(_walletGetBalanceForAddress(
              _requireHandle(),
              address.toNativeUtf8(allocator: arena),
              u,
              l,
            ));
            return (unlocked: u.value, locked: l.value);
          }));

  /// Returns {"balances":[{"address":…,"unlocked":…,"locked":…},…]}
  Future<Map<String, dynamic>> getBalancesJson() =>
      Future(() => _jsonOut(_walletGetBalancesJson));

  // --- addresses ---

  Future<String> getPrimaryAddress() =>
      Future(() => _strOut(_walletGetPrimaryAddress));

  /// Returns {"addresses":[…]}
  Future<Map<String, dynamic>> getAddressesJson() =>
      Future(() => _jsonOut(_walletGetAddressesJson));

  // --- transactions ---

  /// [startHeight] / [endHeight]: 0,0 = all txs.
  Future<Map<String, dynamic>> getTransactionsJson({
    int startHeight = 0,
    int endHeight = 0,
    bool includeUnconfirmed = true,
  }) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletGetTransactionsJson(
              _requireHandle(),
              startHeight,
              endHeight,
              includeUnconfirmed,
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return jsonDecode(result) as Map<String, dynamic>;
          }));

  /// Returns txHash hex on success.
  Future<String> sendBasic(String destination, int amount,
      {String paymentId = '', bool sendAll = false,
      bool broadcast = true}) async {
    final ha = _requireHandle().address;
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnSendBasicNative, _FnSendBasicDart>(
          'wallet_send_basic');
      final free = lib.lookupFunction<_FnStringFreeNative, _FnStringFreeDart>(
          'wallet_string_free');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final h = Pointer<_WalletHandleOpaque>.fromAddress(ha);
        final outStr = arena<Pointer<Utf8>>();
        final outLen = arena<Size>();
        final s = fn(h,
          destination.toNativeUtf8(allocator: arena), amount,
          paymentId.toNativeUtf8(allocator: arena),
          sendAll, broadcast, outStr, outLen,
        );
        if (s != 0) return (status: s, data: '', err: lastErr().toDartString());
        final data = outStr.value.toDartString();
        free(outStr.value);
        return (status: 0, data: data, err: '');
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    return r.data;
  }

  Future<String> sendPrepared(String preparedTxHash) async {
    final ha = _requireHandle().address;
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnSendPreparedNative, _FnSendPreparedDart>(
          'wallet_send_prepared');
      final free = lib.lookupFunction<_FnStringFreeNative, _FnStringFreeDart>(
          'wallet_string_free');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final h = Pointer<_WalletHandleOpaque>.fromAddress(ha);
        final outStr = arena<Pointer<Utf8>>();
        final outLen = arena<Size>();
        final s = fn(h,
          preparedTxHash.toNativeUtf8(allocator: arena),
          outStr, outLen,
        );
        if (s != 0) return (status: s, data: '', err: lastErr().toDartString());
        final data = outStr.value.toDartString();
        free(outStr.value);
        return (status: 0, data: data, err: '');
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    return r.data;
  }

  /// [requestJson]: {"destinations":[{"address":…,"amount":…}],…}
  /// Returns {"transactionHash":…,"fee":…,"relayedToNetwork":…}
  Future<Map<String, dynamic>> sendAdvanced(String requestJson,
          {bool broadcast = true}) async {
    final ha = _requireHandle().address;
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnSendAdvancedNative, _FnSendAdvancedDart>(
          'wallet_send_advanced_json');
      final free = lib.lookupFunction<_FnStringFreeNative, _FnStringFreeDart>(
          'wallet_string_free');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final h = Pointer<_WalletHandleOpaque>.fromAddress(ha);
        final outStr = arena<Pointer<Utf8>>();
        final outLen = arena<Size>();
        final s = fn(h,
          requestJson.toNativeUtf8(allocator: arena),
          broadcast, outStr, outLen,
        );
        if (s != 0) return (status: s, data: '', err: lastErr().toDartString());
        final data = outStr.value.toDartString();
        free(outStr.value);
        return (status: 0, data: data, err: '');
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    return jsonDecode(r.data) as Map<String, dynamic>;
  }

  /// [requestJson]: {"hashes":["<hex>",…]}
  /// Returns {"requestSucceeded":bool,"inPool":[…],"inBlock":[…],"unknown":[…]}
  Future<Map<String, dynamic>> getTransactionsStatus(String requestJson) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletGetTransactionsStatusJson(
              _requireHandle(),
              requestJson.toNativeUtf8(allocator: arena),
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return jsonDecode(result) as Map<String, dynamic>;
          }));

  /// Returns txPrivateKey hex.
  Future<String> getTxPrivateKey(String txHash) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletGetTxPrivateKey(
              _requireHandle(),
              txHash.toNativeUtf8(allocator: arena),
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return result;
          }));

  // --- sweep ---

  /// [amountToSweep] = 0 sweeps entire unlocked balance.
  /// Returns {"results":[{"txHash":…}|{"error":N,"errorMessage":…},…]}
  Future<Map<String, dynamic>> sweepToAddress(String destination,
      {String paymentId = '', int amountToSweep = 0}) async {
    final ha = _requireHandle().address;
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnSweepNative, _FnSweepDart>(
          'wallet_sweep_to_address');
      final free = lib.lookupFunction<_FnStringFreeNative, _FnStringFreeDart>(
          'wallet_string_free');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final h = Pointer<_WalletHandleOpaque>.fromAddress(ha);
        final outStr = arena<Pointer<Utf8>>();
        final outLen = arena<Size>();
        final s = fn(h,
          destination.toNativeUtf8(allocator: arena),
          paymentId.toNativeUtf8(allocator: arena),
          amountToSweep, outStr, outLen,
        );
        if (s != 0) return (status: s, data: '', err: lastErr().toDartString());
        final data = outStr.value.toDartString();
        free(outStr.value);
        return (status: 0, data: data, err: '');
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    return jsonDecode(r.data) as Map<String, dynamic>;
  }

  Future<({int txCount, int totalFee})> estimateSweep(
          {int amountToSweep = 0}) async {
    final ha = _requireHandle().address;
    final r = await Isolate.run(() {
      final lib = _openLibrary();
      final fn = lib.lookupFunction<_FnEstimateSweepNative, _FnEstimateSweepDart>(
          'wallet_estimate_sweep');
      final lastErr = lib.lookupFunction<_FnConstStrNative, _FnConstStrDart>(
          'wallet_last_error_message');
      return using((arena) {
        final h = Pointer<_WalletHandleOpaque>.fromAddress(ha);
        final cnt = arena<Uint64>();
        final fee = arena<Uint64>();
        final s = fn(h, amountToSweep, cnt, fee);
        if (s != 0) return (status: s, txCount: 0, totalFee: 0, err: lastErr().toDartString());
        return (status: 0, txCount: cnt.value, totalFee: fee.value, err: '');
      });
    });
    if (r.status != 0) throw WalletCApiException(r.status, r.err);
    return (txCount: r.txCount, totalFee: r.totalFee);
  }

  // --- keys / seeds ---

  Future<String> getPrivateViewKey() =>
      Future(() => _strOut(_walletGetPrivateViewKey));

  /// Returns {"publicSpendKey":…,"privateSpendKey":…,"walletIndex":…}
  Future<Map<String, dynamic>> getSpendKeysJson(String address) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletGetSpendKeysJson(
              _requireHandle(),
              address.toNativeUtf8(allocator: arena),
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return jsonDecode(result) as Map<String, dynamic>;
          }));

  Future<String> getMnemonicSeed() =>
      Future(() => _strOut(_walletGetMnemonicSeed));

  Future<String> getMnemonicSeedForAddress(String address) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletGetMnemonicSeedForAddress(
              _requireHandle(),
              address.toNativeUtf8(allocator: arena),
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return result;
          }));

  Future<bool> isViewWallet() => Future(() => using((arena) {
        final out = arena<Bool>();
        _check(_walletIsViewWallet(_requireHandle(), out));
        return out.value;
      }));

  // --- subwallets ---

  /// Returns {"address":…,"privateSpendKey":…,"walletIndex":…}
  Future<Map<String, dynamic>> addSubwallet() =>
      Future(() => _jsonOut(_walletAddSubwalletJson));

  Future<String> importSubwalletFromKey(String privateSpendKeyHex,
          {int scanHeight = 0}) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletImportSubwalletFromKey(
              _requireHandle(),
              privateSpendKeyHex.toNativeUtf8(allocator: arena),
              scanHeight,
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return result;
          }));

  Future<String> importSubwalletFromIndex(int walletIndex,
          {int scanHeight = 0}) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletImportSubwalletFromIndex(
              _requireHandle(),
              walletIndex,
              scanHeight,
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return result;
          }));

  Future<void> deleteSubwallet(String address) =>
      Future(() => using((arena) {
            _check(_walletDeleteSubwallet(
              _requireHandle(),
              address.toNativeUtf8(allocator: arena),
            ));
          }));

  // --- integrated address (no wallet handle needed) ---

  Future<String> createIntegratedAddress(
          String address, String paymentId) =>
      Future(() => using((arena) {
            final outStr = arena<Pointer<Utf8>>();
            final outLen = arena<Size>();
            _check(_walletCreateIntegratedAddress(
              address.toNativeUtf8(allocator: arena),
              paymentId.toNativeUtf8(allocator: arena),
              outStr,
              outLen,
            ));
            final result = outStr.value.toDartString();
            _walletStringFree(outStr.value);
            return result;
          }));

  // --- events ---

  /// Non-blocking poll: returns null if no event is ready.
  /// [timeoutMs] = 0 returns immediately; > 0 waits up to that many ms.
  ({WalletEvent type, Map<String, dynamic> data})? pollEvent(
      {int timeoutMs = 0}) {
    return using((arena) {
      final outType = arena<Uint32>();
      final outStr = arena<Pointer<Utf8>>();
      final outLen = arena<Size>();
      final status = _walletPollEvent(
          _requireHandle(), timeoutMs, outType, outStr, outLen);
      if (status != 0) return null;
      final t = WalletEvent.fromInt(outType.value);
      if (t == WalletEvent.none || outStr.value.address == 0) return null;
      final json = outStr.value.toDartString();
      _walletStringFree(outStr.value);
      return (type: t, data: jsonDecode(json) as Map<String, dynamic>);
    });
  }

  // --- logging ---

  void setLogLevel(String level) => using((arena) {
        _walletSetLogLevel(level.toNativeUtf8(allocator: arena));
      });

  Map<String, dynamic> takeLogs() => _jsonOutStatic(_walletTakeLogsJson);

  void clearLogs() => _walletClearLogs();

  // --- TX PoW progress ---

  /// Non-blocking query of TX PoW status. Safe to call from main thread.
  ({bool active, int elapsedMs, int nonces}) getPowStatus() {
    return using((arena) {
      final a = arena<Bool>();
      final e = arena<Uint64>();
      final n = arena<Uint64>();
      _walletGetPowStatus(a, e, n);
      return (active: a.value, elapsedMs: e.value, nonces: n.value);
    });
  }

  // --- scan coinbase ---

  void setScanCoinbase(bool scan) {
    _walletSetScanCoinbase?.call(scan);
  }

  // --- error helpers ---

  String errorCodeToString(int code) =>
      _walletErrorCodeToString(code).toDartString();
}
