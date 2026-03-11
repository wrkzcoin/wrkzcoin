import 'package:flutter/material.dart';
import '../theme/app_theme.dart';

/// App logo/brand widget. Replace the placeholder icon with an actual SVG/PNG
/// asset once the brand assets are ready.
class PlutonLogo extends StatelessWidget {
  final double size;
  const PlutonLogo({super.key, this.size = 36});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        // Placeholder logo — swap for Image.asset('assets/images/logo.png')
        Container(
          width: size,
          height: size,
          decoration: BoxDecoration(
            gradient: const LinearGradient(
              colors: [kPrimary, kAccent],
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
            ),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Icon(Icons.toll_outlined, color: Colors.white, size: size * 0.6),
        ),
        const SizedBox(width: 10),
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              'PLUTON',
              style: TextStyle(
                color: Theme.of(context).colorScheme.onSurface,
                fontSize: size * 0.44,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.2,
              ),
            ),
            Text(
              'v2',
              style: TextStyle(
                color: kAccent,
                fontSize: size * 0.3,
                fontWeight: FontWeight.w500,
              ),
            ),
          ],
        ),
      ],
    );
  }
}
