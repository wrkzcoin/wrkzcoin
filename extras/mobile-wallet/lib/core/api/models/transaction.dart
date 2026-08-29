enum TransferType { outgoing, incoming }

class Transfer {
  final int amount;
  final TransferType type;

  const Transfer({required this.amount, required this.type});

  factory Transfer.fromJson(Map<String, dynamic> json) {
    final amount = (json['amount'] as num?)?.toInt() ?? 0;
    return Transfer(
      amount: amount,
      type: amount >= 0 ? TransferType.incoming : TransferType.outgoing,
    );
  }
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

  DateTime get dateTime =>
      DateTime.fromMillisecondsSinceEpoch(timestamp * 1000);

  /// Direction of the transaction as a whole.
  ///
  /// Derived from the net signed [totalAmount], not from the individual
  /// transfers: a send that routes change into a *different* subwallet
  /// produces a positive transfer alongside the negative one, which would
  /// otherwise make an outgoing transaction look incoming.
  bool get isIncoming => totalAmount >= 0;

  factory Transaction.fromJson(Map<String, dynamic> json) {
    final transfersList = (json['transfers'] as List<dynamic>?)
            ?.map((t) => Transfer.fromJson(t as Map<String, dynamic>))
            .toList() ??
        [];
    return Transaction(
      hash: json['hash'] as String? ?? '',
      timestamp: (json['timestamp'] as num?)?.toInt() ?? 0,
      blockHeight: (json['blockHeight'] as num?)?.toInt() ?? 0,
      paymentID: json['paymentID'] as String? ?? '',
      unlockTime: (json['unlockTime'] as num?)?.toInt() ?? 0,
      isConfirmed: json['isConfirmed'] as bool? ?? true,
      isCoinbaseTransaction:
          json['isCoinbaseTransaction'] as bool? ?? false,
      address: json['address'] as String? ?? '',
      transfers: transfersList,
      fee: (json['fee'] as num?)?.toInt() ?? 0,
      totalAmount: (json['totalAmount'] as num?)?.toInt() ?? 0,
    );
  }
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
        fee: (json['fee'] as num?)?.toInt() ?? 0,
        relayedToNetwork: json['relayedToNetwork'] as bool? ?? false,
      );
}
