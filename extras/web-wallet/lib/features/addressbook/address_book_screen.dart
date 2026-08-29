import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../l10n/generated/app_localizations.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/address_validator.dart';
import '../../shared/widgets/copy_button.dart';
import 'address_book_provider.dart';

// Address validation lives in shared/utils/address_validator.dart so the
// transfer screen and this one cannot drift apart.

class AddressBookScreen extends ConsumerWidget {
  const AddressBookScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tr = S.of(context);
    final entries = ref.watch(addressBookProvider);

    return Scaffold(
      appBar: AppBar(
        title: Text(tr?.tabAddressBook ?? 'Address Book'),
        actions: [
          FilledButton.icon(
            icon: const Icon(Icons.add, size: 16),
            label: Text(tr?.addButton ?? 'Add'),
            onPressed: () => _showAddDialog(context, ref),
          ),
          const SizedBox(width: 16),
        ],
      ),
      body: entries.isEmpty
          ? Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(Icons.contacts_outlined, size: 48,
                      color: Theme.of(context).colorScheme.onSurfaceVariant),
                  const SizedBox(height: 12),
                  Text(tr?.noSavedAddresses ?? 'No saved addresses',
                      style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant)),
                  const SizedBox(height: 4),
                  Text(tr?.tapAddToSave ?? 'Tap Add to save a frequently used address.',
                      style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 12)),
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
    final tr = S.of(context);
    final nameCtrl = TextEditingController();
    final addrCtrl = TextEditingController();
    final noteCtrl = TextEditingController();
    String? error;

    showDialog<void>(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setState) => AlertDialog(
          title: Text(tr?.addAddress ?? 'Add Address'),
          content: SizedBox(
            width: 400,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                TextField(controller: nameCtrl, decoration: InputDecoration(labelText: tr?.nameLabel ?? 'Name / label')),
                const SizedBox(height: 12),
                TextField(controller: addrCtrl, decoration: InputDecoration(labelText: tr?.addressLabel ?? 'Address')),
                const SizedBox(height: 12),
                TextField(controller: noteCtrl, decoration: InputDecoration(labelText: tr?.noteOptional ?? 'Note (optional)')),
                if (error != null) ...[
                  const SizedBox(height: 10),
                  Text(error!, style: const TextStyle(color: kError, fontSize: 12)),
                ],
              ],
            ),
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx), child: Text(tr?.cancel ?? 'Cancel')),
            FilledButton(
              onPressed: () {
                final name = nameCtrl.text.trim();
                final addr = addrCtrl.text.trim();
                if (name.isEmpty || addr.isEmpty) {
                  setState(() => error = tr?.nameAndAddressRequired ?? 'Name and address are required');
                  return;
                }
                if (!isValidAddress(addr)) {
                  setState(() => error =
                      tr?.invalidWrkzAddress ??
                      'Invalid WRKZ address. Must be 98 (standard), '
                      '120 (short integrated), or 186 (long integrated) '
                      'characters starting with "Wrkz".');
                  return;
                }
                // Saving the same address twice under two labels is almost
                // always a mistake, and makes the picker ambiguous.
                final existing = ref.read(addressBookProvider)
                    .where((e) => e.address == addr)
                    .firstOrNull;
                if (existing != null) {
                  setState(() => error = tr?.addressAlreadySaved(existing.name) ??
                      'Already saved as "${existing.name}"');
                  return;
                }
                ref.read(addressBookProvider.notifier).add(
                      name,
                      addr,
                      note: noteCtrl.text.trim().isEmpty ? null : noteCtrl.text.trim(),
                    );
                Navigator.pop(ctx);
              },
              child: Text(tr?.save ?? 'Save'),
            ),
          ],
        ),
      ),
    ).whenComplete(() {
      // Dialog-local controllers were never disposed.
      nameCtrl.dispose();
      addrCtrl.dispose();
      noteCtrl.dispose();
    });
  }
}

class _EntryCard extends ConsumerWidget {
  final AddressBookEntry entry;
  const _EntryCard({required this.entry});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tr = S.of(context);

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
                      style: TextStyle(color: Theme.of(context).colorScheme.onSurface, fontWeight: FontWeight.w500, fontSize: 14)),
                  const SizedBox(height: 2),
                  Text(entry.address,
                      style: TextStyle(color: Theme.of(context).colorScheme.onSurfaceVariant, fontSize: 11, fontFamily: 'monospace'),
                      maxLines: 1, overflow: TextOverflow.ellipsis),
                  if (entry.note != null) ...[
                    const SizedBox(height: 2),
                    Text(entry.note!, style: TextStyle(color: Theme.of(context).colorScheme.outline, fontSize: 11)),
                  ],
                ],
              ),
            ),
            CopyButton(text: entry.address, tooltip: tr?.copyAddress ?? 'Copy address'),
            const SizedBox(width: 4),
            IconButton(
              icon: const Icon(Icons.edit_outlined, size: 16),
              tooltip: tr?.edit ?? 'Edit',
              onPressed: () => _showEditDialog(context, ref),
            ),
            IconButton(
              icon: const Icon(Icons.delete_outline, size: 16, color: kError),
              tooltip: tr?.delete ?? 'Delete',
              onPressed: () => _confirmDelete(context, ref),
            ),
          ],
        ),
      ),
    );
  }

  void _showEditDialog(BuildContext context, WidgetRef ref) {
    final tr = S.of(context);
    final nameCtrl = TextEditingController(text: entry.name);
    final noteCtrl = TextEditingController(text: entry.note ?? '');

    showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr?.editEntry ?? 'Edit Entry'),
        content: SizedBox(
          width: 400,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(controller: nameCtrl, decoration: InputDecoration(labelText: tr?.nameLabel ?? 'Name / label')),
              const SizedBox(height: 12),
              TextField(controller: noteCtrl, decoration: InputDecoration(labelText: tr?.noteOptional ?? 'Note (optional)')),
            ],
          ),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: Text(tr?.cancel ?? 'Cancel')),
          FilledButton(
            onPressed: () {
              ref.read(addressBookProvider.notifier).update(
                    entry.id,
                    name: nameCtrl.text.trim(),
                    note: noteCtrl.text.trim().isEmpty ? null : noteCtrl.text.trim(),
                  );
              Navigator.pop(ctx);
            },
            child: Text(tr?.save ?? 'Save'),
          ),
        ],
      ),
    ).whenComplete(() {
      nameCtrl.dispose();
      noteCtrl.dispose();
    });
  }

  void _confirmDelete(BuildContext context, WidgetRef ref) {
    final tr = S.of(context);

    showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(tr?.deleteEntry ?? 'Delete Entry'),
        content: Text(tr?.removeFromAddressBook(entry.name) ?? 'Remove "${entry.name}" from your address book?'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx), child: Text(tr?.cancel ?? 'Cancel')),
          TextButton(
            style: TextButton.styleFrom(foregroundColor: kError),
            onPressed: () {
              ref.read(addressBookProvider.notifier).remove(entry.id);
              Navigator.pop(ctx);
            },
            child: Text(tr?.delete ?? 'Delete'),
          ),
        ],
      ),
    );
  }
}
