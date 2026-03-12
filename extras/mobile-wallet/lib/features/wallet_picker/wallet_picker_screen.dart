import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:intl/intl.dart';

import '../../core/providers/providers.dart';
import '../../core/storage/wallet_registry.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/utils/haptics.dart';

class WalletPickerScreen extends ConsumerStatefulWidget {
  const WalletPickerScreen({super.key});

  @override
  ConsumerState<WalletPickerScreen> createState() => _WalletPickerScreenState();
}

class _WalletPickerScreenState extends ConsumerState<WalletPickerScreen> {
  List<WalletEntry> _wallets = [];

  @override
  void initState() {
    super.initState();
    _refresh();
  }

  void _refresh() {
    final registry = ref.read(walletRegistryProvider);
    setState(() => _wallets = registry.wallets.toList());
  }

  void _selectWallet(WalletEntry entry) {
    hapticSelection();
    ref.read(activeWalletFilenameProvider.notifier).state = entry.filename;
    context.go('/lock');
  }

  Future<void> _deleteWallet(WalletEntry entry) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete Wallet'),
        content: Text(
            'Delete "${entry.caption}"?\n\nThis will permanently remove the wallet file and keys. Make sure you have backed up your seed phrase.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(backgroundColor: kError),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirmed != true) return;

    final registry = ref.read(walletRegistryProvider);
    await registry.deleteWallet(entry.filename);
    hapticMedium();
    _refresh();
  }

  @override
  Widget build(BuildContext context) {
    final dateFormat = DateFormat('MMM d, yyyy');

    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const SizedBox(height: 20),
              Center(
                child: Container(
                  width: 64,
                  height: 64,
                  decoration: BoxDecoration(
                    gradient: const LinearGradient(
                      colors: [kPrimary, kAccent],
                      begin: Alignment.topLeft,
                      end: Alignment.bottomRight,
                    ),
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: const Icon(
                    Icons.account_balance_wallet,
                    size: 32,
                    color: Colors.white,
                  ),
                ),
              ),
              const SizedBox(height: 20),
              Center(
                child: Text(
                  'PLUTON Mobile',
                  style: Theme.of(context).textTheme.headlineMedium,
                ),
              ),
              const SizedBox(height: 8),
              Center(
                child: Text(
                  _wallets.isEmpty
                      ? 'Create your first wallet to get started'
                      : 'Select a wallet to open',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
              const SizedBox(height: 32),
              if (_wallets.isNotEmpty) ...[
                Text(
                  'Your Wallets',
                  style: Theme.of(context).textTheme.titleLarge,
                ),
                const SizedBox(height: 12),
              ],
              Expanded(
                child: _wallets.isEmpty
                    ? Center(
                        child: Column(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Icon(
                              Icons.wallet_outlined,
                              size: 64,
                              color: Theme.of(context)
                                  .textTheme
                                  .bodySmall
                                  ?.color,
                            ),
                            const SizedBox(height: 16),
                            Text(
                              'No wallets yet',
                              style: Theme.of(context).textTheme.bodyMedium,
                            ),
                          ],
                        ),
                      )
                    : ListView.separated(
                        itemCount: _wallets.length,
                        separatorBuilder: (_, __) => const SizedBox(height: 8),
                        itemBuilder: (context, index) {
                          final entry = _wallets[index];
                          final isLastOpened = entry.filename ==
                              ref.read(walletRegistryProvider).lastOpened;
                          return Card(
                            child: ListTile(
                              contentPadding: const EdgeInsets.symmetric(
                                  horizontal: 16, vertical: 8),
                              leading: CircleAvatar(
                                backgroundColor: kPrimary.withAlpha(30),
                                child: const Icon(
                                    Icons.account_balance_wallet,
                                    color: kPrimary),
                              ),
                              title: Row(
                                children: [
                                  Expanded(
                                    child: Text(
                                      entry.caption,
                                      style: Theme.of(context)
                                          .textTheme
                                          .titleMedium,
                                    ),
                                  ),
                                  if (isLastOpened)
                                    Container(
                                      padding: const EdgeInsets.symmetric(
                                          horizontal: 6, vertical: 2),
                                      decoration: BoxDecoration(
                                        color: kPrimary.withAlpha(25),
                                        borderRadius:
                                            BorderRadius.circular(4),
                                      ),
                                      child: Text(
                                        'Last opened',
                                        style: Theme.of(context)
                                            .textTheme
                                            .labelSmall
                                            ?.copyWith(color: kPrimary),
                                      ),
                                    ),
                                ],
                              ),
                              subtitle: Text(
                                'Created ${dateFormat.format(entry.createdAt)}',
                                style: Theme.of(context).textTheme.bodySmall,
                              ),
                              trailing: IconButton(
                                icon: Icon(Icons.delete_outline,
                                    color: Theme.of(context)
                                        .textTheme
                                        .bodySmall
                                        ?.color),
                                onPressed: () => _deleteWallet(entry),
                              ),
                              onTap: () => _selectWallet(entry),
                            ),
                          );
                        },
                      ),
              ),
              const SizedBox(height: 16),
              FilledButton.icon(
                onPressed: () async {
                  await context.push('/setup');
                  _refresh();
                },
                icon: const Icon(Icons.add),
                label: Text(_wallets.isEmpty
                    ? 'Create First Wallet'
                    : 'Add Wallet'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
