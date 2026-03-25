import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ffi/wallet_ffi.dart';

// ── wallet_capi FFI ───────────────────────────────────────────────────────────

/// Singleton FFI binding for the wallet_capi shared library.
/// wallet_capi.dll (Windows) / libwallet_capi.so (Linux) / libwallet_capi.dylib (macOS)
/// must be placed next to the Flutter executable before launch.
final walletCApiProvider = Provider<WalletCApi>((_) => WalletCApi());

// ── Wallet open state ─────────────────────────────────────────────────────────

/// True once a wallet file has been opened/created successfully.
final walletOpenProvider = StateProvider<bool>((ref) => false);
