class Balance {
  final int unlocked;
  final int locked;

  const Balance({required this.unlocked, required this.locked});

  int get total => unlocked + locked;

  factory Balance.zero() => const Balance(unlocked: 0, locked: 0);

  factory Balance.fromJson(Map<String, dynamic> json) => Balance(
        unlocked: (json['unlocked'] as num?)?.toInt() ?? 0,
        locked: (json['locked'] as num?)?.toInt() ?? 0,
      );
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

  int get total => unlocked + locked;

  factory AddressBalance.fromJson(Map<String, dynamic> json) => AddressBalance(
        address: json['address'] as String? ?? '',
        unlocked: (json['unlocked'] as num?)?.toInt() ?? 0,
        locked: (json['locked'] as num?)?.toInt() ?? 0,
      );
}
