/// WRKZ address and payment-ID validation.
///
/// These are *pre-flight* checks: they catch the typos and bad pastes that
/// otherwise only surface as an opaque error from deep inside the WASM module,
/// several seconds and one round trip later. The authoritative check (base58
/// checksum over the decoded key material) still happens in `wallet_capi`
/// when the transaction is prepared — this never replaces it.
///
/// Constants mirror src/config/WalletConfig.h:
///   addressPrefix              = "Wrkz"
///   standardAddressLength      = 98
///   integratedAddressLength    = 98 + (16 * 11 / 8) = 120   (short payment ID)
///   integratedAddressLengthLong= 98 + (64 * 11 / 8) = 186   (long payment ID)
library;

/// Human-readable prefix every WRKZ address starts with.
const String kAddressPrefix = 'Wrkz';

/// Length of a plain (non-integrated) address.
const int kStandardAddressLength = 98;

/// Length of an integrated address carrying a 16-hex-char payment ID.
const int kIntegratedAddressLength = 120;

/// Length of an integrated address carrying a 64-hex-char payment ID.
const int kIntegratedAddressLengthLong = 186;

const Set<int> _kValidAddressLengths = {
  kStandardAddressLength,
  kIntegratedAddressLength,
  kIntegratedAddressLengthLong,
};

/// The CryptoNote base58 alphabet — note the deliberate absence of
/// `0`, `O`, `I` and `l`, the characters people most often mistype.
final RegExp _kBase58 =
    RegExp(r'^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$');

final RegExp _kHex = RegExp(r'^[0-9a-fA-F]+$');

/// Why an address failed validation, so the UI can say something specific.
enum AddressError {
  empty,
  badPrefix,
  badLength,
  badCharacters,
}

/// Validates [address], returning null when it looks well-formed.
AddressError? validateAddress(String address) {
  final a = address.trim();
  if (a.isEmpty) return AddressError.empty;
  if (!a.startsWith(kAddressPrefix)) return AddressError.badPrefix;
  if (!_kValidAddressLengths.contains(a.length)) return AddressError.badLength;
  if (!_kBase58.hasMatch(a)) return AddressError.badCharacters;
  return null;
}

/// True when [address] is a syntactically valid WRKZ address.
bool isValidAddress(String address) => validateAddress(address) == null;

/// True when [address] carries an embedded payment ID.
bool isIntegratedAddress(String address) {
  final len = address.trim().length;
  return len == kIntegratedAddressLength || len == kIntegratedAddressLengthLong;
}

/// Validates a payment ID. Empty is allowed (payment IDs are optional);
/// anything else must be 16 or 64 hex characters.
bool isValidPaymentId(String paymentId) {
  final p = paymentId.trim();
  if (p.isEmpty) return true;
  if (p.length != 16 && p.length != 64) return false;
  return _kHex.hasMatch(p);
}

/// Renders a long address as `first…last` for compact display.
String shortenAddress(String address, {int head = 12, int tail = 8}) {
  final a = address.trim();
  if (a.length <= head + tail + 1) return a;
  return '${a.substring(0, head)}…${a.substring(a.length - tail)}';
}

/// Splits an address into fixed-width groups so a human can actually compare it
/// against another copy character by character before confirming a send.
List<String> chunkAddress(String address, {int size = 14}) {
  final a = address.trim();
  final chunks = <String>[];
  for (var i = 0; i < a.length; i += size) {
    chunks.add(a.substring(i, i + size > a.length ? a.length : i + size));
  }
  return chunks;
}
