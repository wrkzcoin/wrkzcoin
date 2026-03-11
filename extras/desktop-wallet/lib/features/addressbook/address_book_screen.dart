import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/copy_button.dart';
import 'address_book_provider.dart';

// ── WRKZ address validation ───────────────────────────────────────────────────
// Standard: 98 chars | Short integrated: 120 chars | Long integrated: 186 chars
// All start with "Wrkz" and consist of base58 characters.
bool _isValidWrkzAddress(String address) {
  const validLengths = {98, 120, 186};
  if (!validLengths.contains(address.length)) return false;
  if (!address.startsWith('Wrkz')) return false;
  return RegExp(r'^[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]+$')
      .hasMatch(address);
}

class AddressBookScreen extends ConsumerWidget {
  const AddressBookScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final entries = ref.watch(addressBookProvider);

    return Scaffold(
      backgroundColor: kBgDark,
      appBar: AppBar(
        title: const Text('Address Book'),
        actions: [
          FilledButton.icon(
            icon: const Icon(Icons.add, size: 16),
            label: const Text('Add'),
            onPressed: () => _showAddDialog(context, ref),
          ),
          const SizedBox(width: 16),
        ],
      ),
      body: entries.isEmpty
          ? const Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.contacts_outlined, size: 48, color: kTextDisabled),
                  SizedBox(height: 12),
                  Text('No saved addresses', style: TextStyle(color: kTextDisabled)),
                  SizedBox(height: 4),
                  Text('Tap Add to save a frequently used address.',
                      style: TextStyle(color: kTextDisabled, fontSize: 12)),
                ],
              ),
            )
          : ListView.separated(
              padding: const EdgeInsets.all(16),
              itemCount: entries.length,
              separatorBuilder: (_, _) => const SizedBox(height: 4),
              itemBuilder: (_, i) => _EntryCard(entry: entries[i]),
            ),
    );
  }

  void _showAddDialog(BuildContext context, WidgetRef ref) {
    final nameCtrl = TextEditingController();
    final addrCtrl = TextEditingController();
    final noteCtrl = TextEditingController();
    String? error;

    showDialog(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setState) => AlertDialog(
          title: const Text('Add Address'),
          content: SizedBox(
            width: 400,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                TextField(controller: nameCtrl, decoration: const InputDecoration(labelText: 'Name / label')),
                const SizedBox(height: 12),
                TextField(controller: addrCtrl, decoration: const InputDecoration(labelText: 'Address')),
                const SizedBox(height: 12),
                TextField(controller: noteCtrl, decoration: const InputDecoration(labelText: 'Note (optional)')),
                if (error != null) ...[
                  const SizedBox(height: 10),
                  Text(error!, style: const TextStyle(color: kError, fontSize: 12)),
                ],
              ],
            ),
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
            FilledButton(
              onPressed: () {
                final name = nameCtrl.text.trim();
                final addr = addrCtrl.text.trim();
                if (name.isEmpty || addr.isEmpty) {
                  setState(() => error = 'Name and address are required');
                  return;
                }
                if (!_isValidWrkzAddress(addr)) {
                  setState(() => error =
                      'Invalid WRKZ address. Must be 98 (standard), '
                      '120 (short integrated), or 186 (long integrated) '
                      'characters starting with "Wrkz".');
                  return;
                }
                ref.read(addressBookProvider.notifier).add(
                      name,
                      addr,
                      note: noteCtrl.text.trim().isEmpty ? null : noteCtrl.text.trim(),
                    );
                Navigator.pop(ctx);
              },
              child: const Text('Save'),
            ),
          ],
        ),
      ),
    );
  }
}

class _EntryCard extends ConsumerWidget {
  final AddressBookEntry entry;
  const _EntryCard({required this.entry});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Row(
          children: [
            Container(
              width: 36, height: 36,
              decoration: BoxDecoration(
                color: kPrimary.withAlpha(25),
                borderRadius: BorderRadius.circular(8),
              ),
              child: const Icon(Icons.person_outline, size: 18, color: kPrimary),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(entry.name,
                      style: const TextStyle(color: kTextPrimary, fontWeight: FontWeight.w500, fontSize: 14)),
                  const SizedBox(height: 2),
                  Text(entry.address,
                      style: const TextStyle(color: kTextSecondary, fontSize: 11, fontFamily: 'monospace'),
                      maxLines: 1, overflow: TextOverflow.ellipsis),
                  if (entry.note != null) ...[
                    const SizedBox(height: 2),
                    Text(entry.note!, style: const TextStyle(color: kTextDisabled, fontSize: 11)),
                  ],
                ],
              ),
            ),
            CopyButton(text: entry.address, tooltip: 'Copy address'),
            const SizedBox(width: 4),
            IconButton(
              icon: const Icon(Icons.edit_outlined, size: 16),
              tooltip: 'Edit',
              onPressed: () => _showEditDialog(context, ref),
            ),
            IconButton(
              icon: const Icon(Icons.delete_outline, size: 16, color: kError),
              tooltip: 'Delete',
              onPressed: () => _confirmDelete(context, ref),
            ),
          ],
        ),
      ),
    );
  }

  void _showEditDialog(BuildContext context, WidgetRef ref) {
    final nameCtrl = TextEditingController(text: entry.name);
    final noteCtrl = TextEditingController(text: entry.note ?? '');

    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Edit Entry'),
        content: SizedBox(
          width: 400,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(controller: nameCtrl, decoration: const InputDecoration(labelText: 'Name / label')),
              const SizedBox(height: 12),
              TextField(controller: noteCtrl, decoration: const InputDecoration(labelText: 'Note (optional)')),
            ],
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          FilledButton(
            onPressed: () {
              ref.read(addressBookProvider.notifier).update(
                    entry.id,
                    name: nameCtrl.text.trim(),
                    note: noteCtrl.text.trim().isEmpty ? null : noteCtrl.text.trim(),
                  );
              Navigator.pop(ctx);
            },
            child: const Text('Save'),
          ),
        ],
      ),
    );
  }

  void _confirmDelete(BuildContext context, WidgetRef ref) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete Entry'),
        content: Text('Remove "${entry.name}" from your address book?'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: const Text('Cancel')),
          TextButton(
            style: TextButton.styleFrom(foregroundColor: kError),
            onPressed: () {
              ref.read(addressBookProvider.notifier).remove(entry.id);
              Navigator.pop(ctx);
            },
            child: const Text('Delete'),
          ),
        ],
      ),
    );
  }
}
