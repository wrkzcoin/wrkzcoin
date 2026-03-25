import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../l10n/generated/app_localizations.dart';
import '../utils/haptics.dart';

class CopyButton extends StatefulWidget {
  final String text;
  final double size;

  const CopyButton({super.key, required this.text, this.size = 20});

  @override
  State<CopyButton> createState() => _CopyButtonState();
}

class _CopyButtonState extends State<CopyButton> {
  bool _copied = false;
  Timer? _timer;

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  void _copy() {
    Clipboard.setData(ClipboardData(text: widget.text));
    hapticLight();
    setState(() => _copied = true);
    _timer?.cancel();
    _timer = Timer(const Duration(seconds: 2), () {
      if (mounted) setState(() => _copied = false);
    });
  }

  @override
  Widget build(BuildContext context) {
    return IconButton(
      icon: Icon(
        _copied ? Icons.check : Icons.copy_outlined,
        size: widget.size,
        color: _copied
            ? Theme.of(context).colorScheme.primary
            : Theme.of(context).textTheme.bodySmall?.color,
      ),
      onPressed: _copy,
      tooltip: S.of(context)?.copy ?? 'Copy',
      visualDensity: VisualDensity.compact,
    );
  }
}
