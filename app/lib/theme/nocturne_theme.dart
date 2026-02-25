import 'package:flutter/material.dart';

/// MD3 Noir: #121212 background, #1E1E1E surfaces, #FF6600 accent (BMW Amber).
final ThemeData nocturneNoirTheme = ThemeData(
  useMaterial3: true,
  brightness: Brightness.dark,
  colorScheme: ColorScheme.dark(
    primary: const Color(0xFFFF6600),
    onPrimary: Colors.black,
    secondary: const Color(0xFFE65100),
    onSecondary: Colors.black,
    surface: const Color(0xFF1E1E1E),
    onSurface: const Color(0xFFE0E0E0),
    surfaceContainerHighest: const Color(0xFF2C2C2C),
    error: const Color(0xFFCF6679),
    onError: Colors.black,
    outline: const Color(0xFF616161),
  ),
  scaffoldBackgroundColor: const Color(0xFF121212),
  appBarTheme: const AppBarTheme(
    backgroundColor: Color(0xFF121212),
    foregroundColor: Color(0xFFE0E0E0),
    elevation: 0,
    centerTitle: true,
  ),
  cardTheme: CardThemeData(
    color: const Color(0xFF1E1E1E),
    elevation: 2,
    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
  ),
  elevatedButtonTheme: ElevatedButtonThemeData(
    style: ElevatedButton.styleFrom(
      backgroundColor: const Color(0xFFFF6600),
      foregroundColor: Colors.black,
      padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    ),
  ),
  outlinedButtonTheme: OutlinedButtonThemeData(
    style: OutlinedButton.styleFrom(
      foregroundColor: const Color(0xFFFF6600),
      side: const BorderSide(color: Color(0xFFFF6600)),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    ),
  ),
  floatingActionButtonTheme: const FloatingActionButtonThemeData(
    backgroundColor: Color(0xFFFF6600),
    foregroundColor: Colors.black,
  ),
);
