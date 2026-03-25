import 'dart:convert';
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

  @override
  List<AddressBookEntry> build() {
    _load();
    return [];
  }

  Future<void> _load() async {
    final raw = await _storage.read(key: _kStorageKey);
    if (raw == null) return;
    try {
      final list = jsonDecode(raw) as List<dynamic>;
      state = list
          .map((e) => AddressBookEntry.fromJson(e as Map<String, dynamic>))
          .toList();
    } catch (_) {}
  }

  Future<void> _persist() async {
    await _storage.write(
      key: _kStorageKey,
      value: jsonEncode(state.map((e) => e.toJson()).toList()),
    );
  }

  Future<void> add(String name, String address, {String? note}) async {
    final entry = AddressBookEntry(
      id: DateTime.now().millisecondsSinceEpoch.toString(),
      name: name,
      address: address,
      note: note,
    );
    state = [...state, entry];
    await _persist();
  }

  Future<void> remove(String id) async {
    state = state.where((e) => e.id != id).toList();
    await _persist();
  }

  Future<void> update(String id, {String? name, String? note}) async {
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
