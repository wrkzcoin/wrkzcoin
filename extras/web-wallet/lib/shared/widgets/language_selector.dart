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

/// Small flag button for sidebar footer — opens the language picker dialog.
class LanguageSelectorButton extends ConsumerWidget {
  const LanguageSelectorButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final locale = ref.watch(localeProvider);
    final info = currentLangInfo(locale);

    return InkWell(
      onTap: () => showLanguagePicker(context, ref),
      borderRadius: BorderRadius.circular(4),
      child: Padding(
        padding: const EdgeInsets.all(4),
        child: Tooltip(
          message: S.of(context)?.selectLanguage ?? 'Select Language',
          child: Text(info.flag, style: const TextStyle(fontSize: 18)),
        ),
      ),
    );
  }
}

/// Shows a dialog with all available languages (desktop-friendly).
void showLanguagePicker(BuildContext context, WidgetRef ref) {
  final currentLocale = ref.read(localeProvider);
  final tr = S.of(context);

  showDialog(
    context: context,
    builder: (ctx) {
      return AlertDialog(
        title: Text(tr?.selectLanguage ?? 'Select Language'),
        content: SizedBox(
          width: 340,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: languages.map((lang) {
              final selected =
                  (currentLocale?.languageCode ?? 'en') == lang.code;
              return ListTile(
                leading: Text(lang.flag, style: const TextStyle(fontSize: 24)),
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
            }).toList(),
          ),
        ),
      );
    },
  );
}
