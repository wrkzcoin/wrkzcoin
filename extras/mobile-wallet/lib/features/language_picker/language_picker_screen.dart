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
    return Scaffold(
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            children: [
              const Spacer(),
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
              Expanded(
                flex: 4,
                child: ListView.builder(
                  itemCount: languages.length,
                  itemBuilder: (_, i) {
                    final lang = languages[i];
                    final selected = _selectedCode == lang.code;
                    return Card(
                      color: selected
                          ? kPrimary.withAlpha(20)
                          : null,
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
                            style: Theme.of(context).textTheme.titleMedium),
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
              FilledButton(
                onPressed: () async {
                  // Resolve the router before the await so nothing reaches for
                  // a BuildContext across the async gap.
                  final router = GoRouter.of(context);
                  ref
                      .read(localeProvider.notifier)
                      .set(Locale(_selectedCode));
                  await markFirstLaunchDone();
                  ref.invalidate(firstLaunchDoneProvider);
                  if (!mounted) return;
                  router.go('/splash');
                },
                child: const Text('Continue'),
              ),
              const Spacer(),
            ],
          ),
        ),
      ),
    );
  }
}
