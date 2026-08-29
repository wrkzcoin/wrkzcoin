import 'dart:convert';
import 'dart:math';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

const _kStorageKey = 'pluton_address_book';

@immutable
class AddressBookEntry {
  final String id;
  final String name;
  final String address;
  final String? note;

  const AddressBookEntry({
    required this.id,
    required this.name,
    required this.address,
    this.note,
  });

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'address': address,
        if (note != null) 'note': note,
      };

  factory AddressBookEntry.fromJson(Map<String, dynamic> json) =>
      AddressBookEntry(
        id: json['id'] as String,
        name: json['name'] as String,
        address: json['address'] as String,
        note: json['note'] as String?,
      );
}

class AddressBookNotifier extends Notifier<List<AddressBookEntry>> {
  static const _storage = FlutterSecureStorage();
  static final _rnd = Random.secure();

  /// In-flight initial load. Mutations wait on it so an entry added before the
  /// read completes is not overwritten when the stored list finally lands.
  Future<void>? _loading;

  @override
  List<AddressBookEntry> build() {
    _loading = _load();
    return [];
  }

  Future<void> _load() async {
    try {
      final raw = await _storage.read(key: _kStorageKey);
      if (raw == null) return;
      final list = jsonDecode(raw) as List<dynamic>;
      state = list
          .whereType<Map>()
          .map((e) => AddressBookEntry.fromJson(Map<String, dynamic>.from(e)))
          .toList();
    } catch (e) {
      debugPrint('[addressbook] load failed: $e');
    }
  }

  Future<void> _ready() async {
    if (_loading != null) {
      await _loading;
      _loading = null;
    }
  }

  Future<void> _persist() async {
    await _storage.write(
      key: _kStorageKey,
      value: jsonEncode(state.map((e) => e.toJson()).toList()),
    );
  }

  /// Millisecond timestamps collide when two entries are added in the same
  /// tick (or imported in a loop), and a duplicate id makes remove() delete
  /// both. Suffix with randomness.
  String _newId() =>
      '${DateTime.now().microsecondsSinceEpoch.toRadixString(36)}'
      '-${_rnd.nextInt(0x100000).toRadixString(36)}';

  Future<void> add(String name, String address, {String? note}) async {
    await _ready();
    final entry = AddressBookEntry(
      id: _newId(),
      name: name,
      address: address,
      note: note,
    );
    state = [...state, entry];
    await _persist();
  }

  Future<void> remove(String id) async {
    await _ready();
    state = state.where((e) => e.id != id).toList();
    await _persist();
  }

  Future<void> update(String id, {String? name, String? note}) async {
    await _ready();
    state = state.map((e) {
      if (e.id != id) return e;
      return AddressBookEntry(
        id: e.id,
        name: name ?? e.name,
        address: e.address,
        note: note ?? e.note,
      );
    }).toList();
    await _persist();
  }
}

final addressBookProvider =
    NotifierProvider<AddressBookNotifier, List<AddressBookEntry>>(
        AddressBookNotifier.new);
