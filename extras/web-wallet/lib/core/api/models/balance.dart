class Balance {
  final int unlocked;
  final int locked;

  const Balance({required this.unlocked, required this.locked});

  factory Balance.fromJson(Map<String, dynamic> json) => Balance(
        unlocked: (json['unlocked'] as num).toInt(),
        locked: (json['locked'] as num).toInt(),
      );

  int get total => unlocked + locked;
}

class AddressBalance {
  final String address;
  final int unlocked;
  final int locked;

  const AddressBalance({
    required this.address,
    required this.unlocked,
    required this.locked,
  });

  factory AddressBalance.fromJson(Map<String, dynamic> json) => AddressBalance(
        address: json['address'] as String,
        unlocked: (json['unlocked'] as num).toInt(),
        locked: (json['locked'] as num).toInt(),
      );
}
