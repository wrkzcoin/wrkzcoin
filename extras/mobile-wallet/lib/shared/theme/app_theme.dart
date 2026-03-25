import 'package:flutter/material.dart';

// ── palette ──────────────────────────────────────────────────────────────────

const kPrimary = Color(0xFF1E88E5);
const kAccent = Color(0xFF42A5F5);
const kSuccess = Color(0xFF43A047);
const kError = Color(0xFFE53935);
const kWarning = Color(0xFFFB8C00);

// dark
const _darkBg = Color(0xFF0D1117);
const _darkSurface = Color(0xFF161B22);
const _darkText = Color(0xFFE6EDF3);
const _darkTextSecondary = Color(0xFF8B949E);

// light
const _lightBg = Color(0xFFF0F1F3);
const _lightSurface = Color(0xFFF8F9FA);
const _lightText = Color(0xFF1A1F2E);
const _lightTextSecondary = Color(0xFF57606A);

// ── common ───────────────────────────────────────────────────────────────────

TextTheme _textTheme(Color primary, Color secondary) => TextTheme(
      headlineLarge: TextStyle(
          fontSize: 28, fontWeight: FontWeight.w700, color: primary),
      headlineMedium: TextStyle(
          fontSize: 22, fontWeight: FontWeight.w600, color: primary),
      headlineSmall: TextStyle(
          fontSize: 18, fontWeight: FontWeight.w600, color: primary),
      titleLarge: TextStyle(
          fontSize: 16, fontWeight: FontWeight.w600, color: primary),
      titleMedium: TextStyle(
          fontSize: 14, fontWeight: FontWeight.w500, color: primary),
      bodyLarge: TextStyle(fontSize: 16, color: primary),
      bodyMedium: TextStyle(fontSize: 14, color: primary),
      bodySmall: TextStyle(fontSize: 12, color: secondary),
      labelLarge: TextStyle(
          fontSize: 14, fontWeight: FontWeight.w600, color: primary),
      labelMedium: TextStyle(fontSize: 12, color: secondary),
      labelSmall: TextStyle(fontSize: 11, color: secondary),
    );

InputDecorationTheme _inputDecoration(
    Color fill, Color border, Color text, Color hint) {
  final outlineBorder = OutlineInputBorder(
    borderRadius: BorderRadius.circular(12),
    borderSide: BorderSide(color: border.withAlpha(80)),
  );
  return InputDecorationTheme(
    filled: true,
    fillColor: fill,
    contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
    border: outlineBorder,
    enabledBorder: outlineBorder,
    focusedBorder: OutlineInputBorder(
      borderRadius: BorderRadius.circular(12),
      borderSide: const BorderSide(color: kPrimary, width: 2),
    ),
    errorBorder: OutlineInputBorder(
      borderRadius: BorderRadius.circular(12),
      borderSide: const BorderSide(color: kError),
    ),
    hintStyle: TextStyle(color: hint),
    labelStyle: TextStyle(color: text),
  );
}

FilledButtonThemeData _filledButton() => FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: kPrimary,
        foregroundColor: Colors.white,
        minimumSize: const Size(double.infinity, 50),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
      ),
    );

OutlinedButtonThemeData _outlinedButton(Color fg) => OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: fg,
        minimumSize: const Size(double.infinity, 50),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        side: BorderSide(color: fg.withAlpha(80)),
        textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
      ),
    );

// ── dark theme ───────────────────────────────────────────────────────────────

ThemeData buildDarkTheme() => ThemeData(
      brightness: Brightness.dark,
      useMaterial3: true,
      scaffoldBackgroundColor: _darkBg,
      colorScheme: const ColorScheme.dark(
        primary: kPrimary,
        secondary: kAccent,
        surface: _darkSurface,
        error: kError,
        onPrimary: Colors.white,
        onSurface: _darkText,
        onError: Colors.white,
      ),
      cardTheme: CardThemeData(
        color: _darkSurface,
        elevation: 0,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      ),
      appBarTheme: const AppBarTheme(
        backgroundColor: _darkBg,
        foregroundColor: _darkText,
        elevation: 0,
        centerTitle: true,
      ),
      bottomNavigationBarTheme: const BottomNavigationBarThemeData(
        backgroundColor: _darkSurface,
        selectedItemColor: kPrimary,
        unselectedItemColor: _darkTextSecondary,
        type: BottomNavigationBarType.fixed,
        elevation: 8,
      ),
      textTheme: _textTheme(_darkText, _darkTextSecondary),
      inputDecorationTheme:
          _inputDecoration(_darkSurface, _darkTextSecondary, _darkText, _darkTextSecondary),
      filledButtonTheme: _filledButton(),
      outlinedButtonTheme: _outlinedButton(_darkText),
      chipTheme: ChipThemeData(
        backgroundColor: _darkSurface,
        selectedColor: kPrimary.withAlpha(50),
        labelStyle: const TextStyle(fontSize: 13, color: _darkText),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        side: BorderSide(color: _darkTextSecondary.withAlpha(60)),
      ),
      dividerColor: _darkTextSecondary.withAlpha(40),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: _darkSurface,
        contentTextStyle: const TextStyle(color: _darkText),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        behavior: SnackBarBehavior.floating,
      ),
    );

// ── light theme ──────────────────────────────────────────────────────────────

ThemeData buildLightTheme() => ThemeData(
      brightness: Brightness.light,
      useMaterial3: true,
      scaffoldBackgroundColor: _lightBg,
      colorScheme: const ColorScheme.light(
        primary: kPrimary,
        secondary: kAccent,
        surface: _lightSurface,
        error: kError,
        onPrimary: Colors.white,
        onSurface: _lightText,
        onError: Colors.white,
      ),
      cardTheme: CardThemeData(
        color: _lightSurface,
        elevation: 0,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      ),
      appBarTheme: const AppBarTheme(
        backgroundColor: _lightBg,
        foregroundColor: _lightText,
        elevation: 0,
        centerTitle: true,
      ),
      bottomNavigationBarTheme: const BottomNavigationBarThemeData(
        backgroundColor: _lightSurface,
        selectedItemColor: kPrimary,
        unselectedItemColor: _lightTextSecondary,
        type: BottomNavigationBarType.fixed,
        elevation: 8,
      ),
      textTheme: _textTheme(_lightText, _lightTextSecondary),
      inputDecorationTheme:
          _inputDecoration(_lightSurface, _lightTextSecondary, _lightText, _lightTextSecondary),
      filledButtonTheme: _filledButton(),
      outlinedButtonTheme: _outlinedButton(_lightText),
      chipTheme: ChipThemeData(
        backgroundColor: _lightSurface,
        selectedColor: kPrimary.withAlpha(30),
        labelStyle: const TextStyle(fontSize: 13, color: _lightText),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        side: BorderSide(color: _lightTextSecondary.withAlpha(60)),
      ),
      dividerColor: _lightTextSecondary.withAlpha(40),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: _lightSurface,
        contentTextStyle: const TextStyle(color: _lightText),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        behavior: SnackBarBehavior.floating,
      ),
    );
