import 'package:flutter/material.dart';

// ── Brand colours (shared by both themes) ─────────────────────────────────────
const kPrimary = Color(0xFF1E88E5);
const kPrimaryDark = Color(0xFF1565C0);
const kAccent = Color(0xFF42A5F5);
const kSuccess = Color(0xFF43A047);
const kError = Color(0xFFE53935);
const kWarning = Color(0xFFFB8C00);

// ── Dark palette ──────────────────────────────────────────────────────────────
const kBgDark = Color(0xFF0D1117);
const kSurface = Color(0xFF161B22);
const kSurfaceVariant = Color(0xFF21262D);
const kDivider = Color(0xFF30363D);
const kTextPrimary = Color(0xFFE6EDF3);
const kTextSecondary = Color(0xFF8B949E);
const kTextDisabled = Color(0xFF484F58);

// ── Light palette ─────────────────────────────────────────────────────────────
const kBgLight = Color(0xFFF0F1F3);
const kSurfaceLight = Color(0xFFF8F9FA);
const kSurfaceVariantLight = Color(0xFFEAECEF);
const kDividerLight = Color(0xFFCDD1D8);
const kTextPrimaryLight = Color(0xFF1A1F2E);
const kTextSecondaryLight = Color(0xFF4A5568);
const kTextDisabledLight = Color(0xFF8A97A8);

// ── Theme-aware accessors ─────────────────────────────────────────────────────

/// Semantic colours that resolve against the *active* theme.
///
/// Both palettes were defined from the start, but the screens referenced the
/// dark constants (`kTextSecondary`, `kDivider`, `kBgDark`, …) directly, so
/// light mode rendered grey-on-grey text and dark dividers on light cards.
/// Going through these getters keeps a single call site working in both.
extension AppColors on BuildContext {
  ColorScheme get cs => Theme.of(this).colorScheme;

  bool get isDarkTheme => Theme.of(this).brightness == Brightness.dark;

  /// Muted body text / captions.
  Color get textSecondary => isDarkTheme ? kTextSecondary : kTextSecondaryLight;

  /// Disabled or placeholder text.
  Color get textDisabled => isDarkTheme ? kTextDisabled : kTextDisabledLight;

  /// Primary body text.
  Color get textPrimary => isDarkTheme ? kTextPrimary : kTextPrimaryLight;

  /// Raised fill behind inputs and chips.
  Color get surfaceVariantColor =>
      isDarkTheme ? kSurfaceVariant : kSurfaceVariantLight;

  /// Hairline separators.
  Color get dividerColor => isDarkTheme ? kDivider : kDividerLight;

  /// Card / sidebar background.
  Color get surfaceColor => isDarkTheme ? kSurface : kSurfaceLight;

  /// Page background.
  Color get appBackground => isDarkTheme ? kBgDark : kBgLight;
}

// ── Dark theme ────────────────────────────────────────────────────────────────
ThemeData buildDarkTheme() => ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      colorScheme: ColorScheme.dark(
        primary: kPrimary,
        onPrimary: Colors.white,
        secondary: kAccent,
        onSecondary: Colors.white,
        error: kError,
        surface: kSurface,
        onSurface: kTextPrimary,
      ),
      scaffoldBackgroundColor: kBgDark,
      cardTheme: CardThemeData(
        color: kSurface,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: kDivider, width: 1),
        ),
      ),
      dividerTheme: const DividerThemeData(color: kDivider, thickness: 1),
      appBarTheme: const AppBarTheme(
        backgroundColor: kBgDark,
        foregroundColor: kTextPrimary,
        elevation: 0,
        centerTitle: false,
        titleTextStyle: TextStyle(color: kTextPrimary, fontSize: 18, fontWeight: FontWeight.w600),
      ),
      inputDecorationTheme: _inputTheme(kSurfaceVariant, kDivider, kTextSecondary, kTextDisabled),
      filledButtonTheme: _filledBtn(),
      outlinedButtonTheme: _outlinedBtn(),
      textButtonTheme: TextButtonThemeData(style: TextButton.styleFrom(foregroundColor: kAccent)),
      chipTheme: _chipTheme(kSurfaceVariant, kDivider, kTextPrimary),
      textTheme: _textTheme(kTextPrimary, kTextSecondary, kTextDisabled),
    );

// ── Light theme ───────────────────────────────────────────────────────────────
ThemeData buildLightTheme() => ThemeData(
      useMaterial3: true,
      brightness: Brightness.light,
      colorScheme: ColorScheme.light(
        primary: kPrimary,
        onPrimary: Colors.white,
        secondary: kAccent,
        onSecondary: Colors.white,
        error: kError,
        surface: kSurfaceLight,
        onSurface: kTextPrimaryLight,
      ),
      scaffoldBackgroundColor: kBgLight,
      cardTheme: CardThemeData(
        color: kSurfaceLight,
        elevation: 2,
        shadowColor: const Color(0x18000000),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: kDividerLight, width: 1),
        ),
      ),
      dividerTheme: const DividerThemeData(color: kDividerLight, thickness: 1),
      appBarTheme: const AppBarTheme(
        backgroundColor: kBgLight,
        foregroundColor: kTextPrimaryLight,
        elevation: 0,
        centerTitle: false,
        titleTextStyle: TextStyle(color: kTextPrimaryLight, fontSize: 18, fontWeight: FontWeight.w600),
      ),
      inputDecorationTheme: _inputTheme(kSurfaceVariantLight, kDividerLight, kTextSecondaryLight, kTextDisabledLight),
      filledButtonTheme: _filledBtn(),
      outlinedButtonTheme: _outlinedBtn(),
      textButtonTheme: TextButtonThemeData(style: TextButton.styleFrom(foregroundColor: kPrimary)),
      chipTheme: _chipTheme(kSurfaceVariantLight, kDividerLight, kTextPrimaryLight),
      textTheme: _textTheme(kTextPrimaryLight, kTextSecondaryLight, kTextDisabledLight),
    );

// ── Shared helpers ────────────────────────────────────────────────────────────

InputDecorationTheme _inputTheme(Color fill, Color border, Color label, Color hint) =>
    InputDecorationTheme(
      filled: true,
      fillColor: fill,
      border: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: BorderSide(color: border)),
      enabledBorder: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: BorderSide(color: border)),
      focusedBorder: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: const BorderSide(color: kPrimary, width: 1.5)),
      labelStyle: TextStyle(color: label),
      hintStyle: TextStyle(color: hint),
    );

FilledButtonThemeData _filledBtn() => FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: kPrimary,
        foregroundColor: Colors.white,
        minimumSize: const Size(120, 44),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      ),
    );

OutlinedButtonThemeData _outlinedBtn() => OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: kPrimary,
        side: const BorderSide(color: kPrimary),
        minimumSize: const Size(120, 44),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      ),
    );

ChipThemeData _chipTheme(Color bg, Color border, Color text) => ChipThemeData(
      backgroundColor: bg,
      labelStyle: TextStyle(color: text, fontSize: 12),
      side: BorderSide(color: border),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(6)),
    );

TextTheme _textTheme(Color primary, Color secondary, Color disabled) => TextTheme(
      displaySmall: TextStyle(color: primary, fontSize: 28, fontWeight: FontWeight.bold),
      headlineMedium: TextStyle(color: primary, fontSize: 22, fontWeight: FontWeight.w600),
      headlineSmall: TextStyle(color: primary, fontSize: 18, fontWeight: FontWeight.w600),
      titleMedium: TextStyle(color: primary, fontSize: 16, fontWeight: FontWeight.w500),
      titleSmall: TextStyle(color: secondary, fontSize: 14, fontWeight: FontWeight.w500),
      bodyLarge: TextStyle(color: primary, fontSize: 14),
      bodyMedium: TextStyle(color: secondary, fontSize: 13),
      bodySmall: TextStyle(color: disabled, fontSize: 12),
      labelLarge: TextStyle(color: primary, fontSize: 14, fontWeight: FontWeight.w600),
    );
