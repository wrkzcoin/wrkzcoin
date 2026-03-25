import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../core/providers/app_providers.dart';
import '../../shared/theme/app_theme.dart';
import '../../shared/widgets/language_selector.dart';

class LanguagePickerScreen extends ConsumerStatefulWidget {
  const LanguagePickerScreen({super.key});

  @override
  ConsumerState<LanguagePickerScreen> createState() =>
      _LanguagePickerScreenState();
}

class _LanguagePickerScreenState extends ConsumerState<LanguagePickerScreen> {
  String _selectedCode = 'en';

  @override
  Widget build(BuildContext context) {
    final screenH = MediaQuery.of(context).size.height;
    return Scaffold(
      body: Center(
        child: ConstrainedBox(
          constraints: BoxConstraints(maxHeight: screenH * 0.9, maxWidth: 460),
          child: Card(
            child: Padding(
              padding: const EdgeInsets.all(32),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Container(
                    width: 72,
                    height: 72,
                    decoration: BoxDecoration(
                      gradient: const LinearGradient(
                        colors: [kPrimary, kAccent],
                        begin: Alignment.topLeft,
                        end: Alignment.bottomRight,
                      ),
                      borderRadius: BorderRadius.circular(18),
                    ),
                    child: const Icon(Icons.translate,
                        size: 36, color: Colors.white),
                  ),
                  const SizedBox(height: 20),
                  Text(
                    'Select Language',
                    style: Theme.of(context).textTheme.headlineMedium,
                  ),
                  const SizedBox(height: 8),
                  Text(
                    'Choose your preferred language',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 24),
                  Flexible(
                    child: ListView.builder(
                      shrinkWrap: true,
                      itemCount: languages.length,
                      itemBuilder: (_, i) {
                        final lang = languages[i];
                        final selected = _selectedCode == lang.code;
                        return Card(
                          color: selected ? kPrimary.withAlpha(20) : null,
                          shape: RoundedRectangleBorder(
                            borderRadius: BorderRadius.circular(12),
                            side: selected
                                ? const BorderSide(color: kPrimary, width: 2)
                                : BorderSide.none,
                          ),
                          child: ListTile(
                            leading: Text(lang.flag,
                                style: const TextStyle(fontSize: 28)),
                            title: Text(lang.nativeName,
                                style:
                                    Theme.of(context).textTheme.titleMedium),
                            trailing: selected
                                ? const Icon(Icons.check_circle,
                                    color: kPrimary)
                                : null,
                            onTap: () =>
                                setState(() => _selectedCode = lang.code),
                          ),
                        );
                      },
                    ),
                  ),
                  const SizedBox(height: 16),
                  SizedBox(
                    width: double.infinity,
                    child: FilledButton(
                      onPressed: () async {
                        ref
                            .read(localeProvider.notifier)
                            .set(Locale(_selectedCode));
                        await markFirstLaunchDone();
                        ref.invalidate(firstLaunchDoneProvider);
                        if (mounted) context.go('/setup');
                      },
                      child: const Text('Continue'),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
