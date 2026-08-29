enum TransferType { outgoing, incoming }

class Transfer {
  final int amount;
  final TransferType type;

  const Transfer({required this.amount, required this.type});

  factory Transfer.fromJson(Map<String, dynamic> json) => Transfer(
        amount: (json['amount'] as num?)?.toInt() ?? 0,
        type: ((json['type'] as num?)?.toInt() ?? 0) == 0
            ? TransferType.outgoing
            : TransferType.incoming,
      );

  @override
  bool operator ==(Object other) =>
      other is Transfer && other.amount == amount && other.type == type;

  @override
  int get hashCode => Object.hash(amount, type);
}

class Transaction {
  final String hash;
  final int timestamp;
  final int blockHeight;
  final String paymentID;
  final int unlockTime;
  final bool isConfirmed;
  final bool isCoinbaseTransaction;
  final String address;
  final List<Transfer> transfers;
  final int fee;
  final int totalAmount;

  const Transaction({
    required this.hash,
    required this.timestamp,
    required this.blockHeight,
    required this.paymentID,
    required this.unlockTime,
    required this.isConfirmed,
    required this.isCoinbaseTransaction,
    required this.address,
    required this.transfers,
    required this.fee,
    required this.totalAmount,
  });

  // Every field is read defensively. A transient WASM failure returns an empty
  // map, and a hard cast on a missing key turned that into a TypeError that
  // surfaced as "type 'Null' is not a subtype of type 'num'" across the UI.
  factory Transaction.fromJson(Map<String, dynamic> json) => Transaction(
        hash: json['hash'] as String? ?? '',
        timestamp: (json['timestamp'] as num? ?? 0).toInt(),
        blockHeight: (json['blockHeight'] as num? ?? 0).toInt(),
        paymentID: json['paymentID'] as String? ?? '',
        unlockTime: (json['unlockTime'] as num? ?? 0).toInt(),
        isConfirmed: json['isConfirmed'] as bool? ?? false,
        isCoinbaseTransaction: json['isCoinbaseTransaction'] as bool? ?? false,
        address: json['address'] as String? ?? '',
        transfers: (json['transfers'] as List<dynamic>? ?? [])
            .whereType<Map>()
            .map((t) => Transfer.fromJson(Map<String, dynamic>.from(t)))
            .toList(),
        fee: (json['fee'] as num? ?? 0).toInt(),
        totalAmount: (json['totalAmount'] as num? ?? 0).toInt(),
      );

  /// Null for a mempool transaction, which has no block timestamp yet.
  /// Rendering `timestamp == 0` as a date shows every pending transfer as
  /// 1 January 1970.
  DateTime? get dateTime => timestamp == 0
      ? null
      : DateTime.fromMillisecondsSinceEpoch(timestamp * 1000);

  bool get isIncoming =>
      transfers.any((t) => t.type == TransferType.incoming);

  /// Content equality so the transaction list can tell "same data, polled
  /// again" from "something actually changed".
  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    if (other is! Transaction) return false;
    if (other.hash != hash ||
        other.timestamp != timestamp ||
        other.blockHeight != blockHeight ||
        other.paymentID != paymentID ||
        other.unlockTime != unlockTime ||
        other.isConfirmed != isConfirmed ||
        other.isCoinbaseTransaction != isCoinbaseTransaction ||
        other.address != address ||
        other.fee != fee ||
        other.totalAmount != totalAmount ||
        other.transfers.length != transfers.length) {
      return false;
    }
    for (var i = 0; i < transfers.length; i++) {
      if (transfers[i] != other.transfers[i]) return false;
    }
    return true;
  }

  @override
  int get hashCode => Object.hash(
        hash,
        timestamp,
        blockHeight,
        paymentID,
        unlockTime,
        isConfirmed,
        isCoinbaseTransaction,
        address,
        fee,
        totalAmount,
        transfers.length,
      );
}

class SendResult {
  final String transactionHash;
  final int fee;
  final bool relayedToNetwork;

  const SendResult({
    required this.transactionHash,
    required this.fee,
    required this.relayedToNetwork,
  });

  factory SendResult.fromJson(Map<String, dynamic> json) => SendResult(
        transactionHash: json['transactionHash'] as String? ?? '',
        fee: (json['fee'] as num? ?? 0).toInt(),
        relayedToNetwork: json['relayedToNetwork'] as bool? ?? true,
      );
}
