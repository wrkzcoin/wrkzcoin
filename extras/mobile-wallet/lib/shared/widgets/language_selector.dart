import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/providers/app_providers.dart';
import '../../l10n/generated/app_localizations.dart';

/// Language metadata: code, native name, flag emoji.
class LangInfo {
  final String code;
  final String nativeName;
  final String flag;

  const LangInfo(this.code, this.nativeName, this.flag);
}

const languages = [
  LangInfo('en', 'English', '🇬🇧'),
  LangInfo('fr', 'Français', '🇫🇷'),
  LangInfo('de', 'Deutsch', '🇩🇪'),
  LangInfo('zh', '中文', '🇨🇳'),
  LangInfo('vi', 'Tiếng Việt', '🇻🇳'),
  LangInfo('ja', '日本語', '🇯🇵'),
  LangInfo('es', 'Español', '🇪🇸'),
  LangInfo('pt', 'Português', '🇧🇷'),
  LangInfo('ru', 'Русский', '🇷🇺'),
];

LangInfo currentLangInfo(Locale? locale) {
  if (locale == null) return languages.first;
  return languages.firstWhere(
    (l) => l.code == locale.languageCode,
    orElse: () => languages.first,
  );
}

/// Small flag button for AppBar — opens the language picker bottom sheet.
class LanguageSelectorButton extends ConsumerWidget {
  const LanguageSelectorButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final locale = ref.watch(localeProvider);
    final info = currentLangInfo(locale);

    return IconButton(
      onPressed: () => showLanguagePicker(context, ref),
      icon: Text(info.flag, style: const TextStyle(fontSize: 22)),
      tooltip: S.of(context)?.selectLanguage ?? 'Select Language',
    );
  }
}

/// Shows a bottom sheet with all available languages.
void showLanguagePicker(BuildContext context, WidgetRef ref) {
  final currentLocale = ref.read(localeProvider);
  final tr = S.of(context);

  showModalBottomSheet(
    context: context,
    shape: const RoundedRectangleBorder(
      borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
    ),
    builder: (ctx) {
      return SafeArea(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const SizedBox(height: 12),
            Container(
              width: 40,
              height: 4,
              decoration: BoxDecoration(
                color: Theme.of(ctx).dividerColor,
                borderRadius: BorderRadius.circular(2),
              ),
            ),
            const SizedBox(height: 16),
            Text(
              tr?.selectLanguage ?? 'Select Language',
              style: Theme.of(ctx).textTheme.titleLarge,
            ),
            const SizedBox(height: 8),
            ...languages.map((lang) {
              final selected =
                  (currentLocale?.languageCode ?? 'en') == lang.code;
              return ListTile(
                leading: Text(lang.flag, style: const TextStyle(fontSize: 28)),
                title: Text(lang.nativeName),
                trailing: selected
                    ? Icon(Icons.check_circle,
                        color: Theme.of(ctx).colorScheme.primary)
                    : null,
                onTap: () {
                  ref
                      .read(localeProvider.notifier)
                      .set(Locale(lang.code));
                  Navigator.pop(ctx);
                },
              );
            }),
            const SizedBox(height: 16),
          ],
        ),
      );
    },
  );
}
