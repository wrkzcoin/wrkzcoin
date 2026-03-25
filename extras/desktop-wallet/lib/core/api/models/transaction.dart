enum TransferType { outgoing, incoming }

class Transfer {
  final int amount;
  final TransferType type;

  const Transfer({required this.amount, required this.type});

  factory Transfer.fromJson(Map<String, dynamic> json) => Transfer(
        amount: (json['amount'] as num).toInt(),
        type: (json['type'] as int) == 0
            ? TransferType.outgoing
            : TransferType.incoming,
      );
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

  factory Transaction.fromJson(Map<String, dynamic> json) => Transaction(
        hash: json['hash'] as String,
        timestamp: (json['timestamp'] as num).toInt(),
        blockHeight: (json['blockHeight'] as num).toInt(),
        paymentID: json['paymentID'] as String? ?? '',
        unlockTime: (json['unlockTime'] as num? ?? 0).toInt(),
        isConfirmed: json['isConfirmed'] as bool? ?? false,
        isCoinbaseTransaction: json['isCoinbaseTransaction'] as bool? ?? false,
        address: json['address'] as String? ?? '',
        transfers: (json['transfers'] as List<dynamic>? ?? [])
            .map((t) => Transfer.fromJson(t as Map<String, dynamic>))
            .toList(),
        fee: (json['fee'] as num? ?? 0).toInt(),
        totalAmount: (json['totalAmount'] as num? ?? 0).toInt(),
      );

  DateTime get dateTime =>
      DateTime.fromMillisecondsSinceEpoch(timestamp * 1000);

  bool get isIncoming =>
      transfers.any((t) => t.type == TransferType.incoming);
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
        transactionHash: json['transactionHash'] as String,
        fee: (json['fee'] as num).toInt(),
        relayedToNetwork: json['relayedToNetwork'] as bool? ?? true,
      );
}
