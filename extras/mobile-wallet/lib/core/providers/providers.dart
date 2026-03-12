import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../ffi/wallet_ffi.dart';
import '../storage/wallet_registry.dart';

/// Singleton FFI binding — one per app lifetime.
final walletCApiProvider = Provider<WalletCApi>((_) => WalletCApi());

/// Wallet registry — multi-wallet management.
final walletRegistryProvider = Provider<WalletRegistry>((_) => WalletRegistry());

/// Whether a wallet is currently open (handle active).
final walletOpenProvider = StateProvider<bool>((ref) => false);

/// The filename of the currently selected/open wallet (null if none).
final activeWalletFilenameProvider = StateProvider<String?>((ref) => null);

/// Whether the open wallet is locked (needs password/biometric to unlock).
final walletLockedProvider = StateProvider<bool>((ref) => false);
