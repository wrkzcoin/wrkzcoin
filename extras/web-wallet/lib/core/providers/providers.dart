import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../ffi/wallet_web.dart';

// ── wallet_capi web binding ──────────────────────────────────────────────────

/// Singleton web binding for the wallet WASM module.
/// Calls through to WalletBridge JS wrapper → WASM.
final walletCApiProvider = Provider<WalletCApi>((_) => WalletCApi());

// ── Wallet open state ─────────────────────────────────────────────────────────

/// True once a wallet file has been opened/created successfully.
final walletOpenProvider = StateProvider<bool>((ref) => false);
