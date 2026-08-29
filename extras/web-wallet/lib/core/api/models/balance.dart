class Balance {
  final int unlocked;
  final int locked;

  const Balance({required this.unlocked, required this.locked});

  factory Balance.fromJson(Map<String, dynamic> json) => Balance(
        unlocked: (json['unlocked'] as num? ?? 0).toInt(),
        locked: (json['locked'] as num? ?? 0).toInt(),
      );

  int get total => unlocked + locked;

  @override
  bool operator ==(Object other) =>
      other is Balance && other.unlocked == unlocked && other.locked == locked;

  @override
  int get hashCode => Object.hash(unlocked, locked);
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
        address: json['address'] as String? ?? '',
        unlocked: (json['unlocked'] as num? ?? 0).toInt(),
        locked: (json['locked'] as num? ?? 0).toInt(),
      );

  @override
  bool operator ==(Object other) =>
      other is AddressBalance &&
      other.address == address &&
      other.unlocked == unlocked &&
      other.locked == locked;

  @override
  int get hashCode => Object.hash(address, unlocked, locked);
}
