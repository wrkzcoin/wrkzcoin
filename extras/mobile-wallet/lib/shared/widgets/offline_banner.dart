import 'package:flutter/material.dart';

import '../../l10n/generated/app_localizations.dart';
import '../theme/app_theme.dart';

class OfflineBanner extends StatelessWidget {
  const OfflineBanner({super.key});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      decoration: BoxDecoration(
        color: kError.withAlpha(25),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          const Icon(Icons.cloud_off, size: 18, color: kError),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              S.of(context)?.noConnectionToDaemon ?? 'No connection to daemon',
              style: Theme.of(context)
                  .textTheme
                  .titleMedium
                  ?.copyWith(color: kError),
            ),
          ),
        ],
      ),
    );
  }
}
