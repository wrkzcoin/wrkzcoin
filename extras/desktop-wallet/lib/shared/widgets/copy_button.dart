import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import '../theme/app_theme.dart';

/// Icon button that copies [text] to clipboard and shows a brief checkmark.
class CopyButton extends StatefulWidget {
  final String text;
  final String? tooltip;
  final double size;

  const CopyButton({
    super.key,
    required this.text,
    this.tooltip,
    this.size = 18,
  });

  @override
  State<CopyButton> createState() => _CopyButtonState();
}

class _CopyButtonState extends State<CopyButton> {
  bool _copied = false;

  Future<void> _copy() async {
    await Clipboard.setData(ClipboardData(text: widget.text));
    if (!mounted) return;
    setState(() => _copied = true);
    await Future.delayed(const Duration(seconds: 2));
    if (mounted) setState(() => _copied = false);
  }

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: _copied ? 'Copied!' : (widget.tooltip ?? 'Copy'),
      child: InkWell(
        onTap: _copy,
        borderRadius: BorderRadius.circular(4),
        child: Padding(
          padding: const EdgeInsets.all(4),
          child: Icon(
            _copied ? Icons.check : Icons.copy_outlined,
            size: widget.size,
            color: _copied ? kSuccess : kTextSecondary,
          ),
        ),
      ),
    );
  }
}
