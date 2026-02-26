import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

/// MD3 M-Power Noir: Graphite #121212, Surface #1E1E1E.
/// M-Red #E21A23 = critical states, FAB, Shift Light.
/// Light Blue #00A1E4 / Dark Blue #012169 = secondary.
/// Roboto for UI, Monospace for telemetry.
final ThemeData nocturneNoirTheme = ThemeData(
  useMaterial3: true,
  brightness: Brightness.dark,
  colorScheme: ColorScheme.dark(
    primary: const Color(0xFFE21A23),
    onPrimary: Colors.white,
    secondary: const Color(0xFF00A1E4),
    onSecondary: Colors.black,
    tertiary: const Color(0xFF012169),
    onTertiary: Colors.white,
    surface: const Color(0xFF1E1E1E),
    onSurface: const Color(0xFFE0E0E0),
    surfaceContainerHighest: const Color(0xFF2C2C2C),
    error: const Color(0xFFCF6679),
    onError: Colors.black,
    outline: const Color(0xFF616161),
  ),
  scaffoldBackgroundColor: const Color(0xFF121212),
  appBarTheme: AppBarTheme(
    backgroundColor: const Color(0xFF121212),
    foregroundColor: const Color(0xFFE0E0E0),
    elevation: 0,
    centerTitle: true,
    titleTextStyle: GoogleFonts.roboto(
      fontSize: 20,
      fontWeight: FontWeight.w600,
      color: const Color(0xFFE0E0E0),
    ),
  ),
  cardTheme: CardThemeData(
    color: const Color(0xFF1E1E1E),
    elevation: 2,
    shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
  ),
  textTheme: _noirTextTheme(),
  elevatedButtonTheme: ElevatedButtonThemeData(
    style: ElevatedButton.styleFrom(
      backgroundColor: const Color(0xFFE21A23),
      foregroundColor: Colors.white,
      padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    ),
  ),
  outlinedButtonTheme: OutlinedButtonThemeData(
    style: OutlinedButton.styleFrom(
      foregroundColor: const Color(0xFF00A1E4),
      side: const BorderSide(color: Color(0xFF00A1E4)),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
    ),
  ),
  floatingActionButtonTheme: const FloatingActionButtonThemeData(
    backgroundColor: Color(0xFFE21A23),
    foregroundColor: Colors.white,
  ),
  navigationBarTheme: NavigationBarThemeData(
    backgroundColor: const Color(0xFF1E1E1E),
    indicatorColor: const Color(0xFF012169),
    labelTextStyle: WidgetStateProperty.resolveWith((states) {
      return GoogleFonts.roboto(fontSize: 12, color: const Color(0xFFE0E0E0));
    }),
  ),
);

TextTheme _noirTextTheme() {
  final roboto = GoogleFonts.roboto(color: const Color(0xFFE0E0E0));
  const monospace = TextStyle(
    fontFamily: 'monospace',
    fontFamilyFallback: ['RobotoMono', 'Consolas', 'monospace'],
  );
  return ThemeData.dark().textTheme.copyWith(
        titleLarge: monospace.merge(ThemeData.dark().textTheme.titleLarge),
        titleMedium: monospace.merge(ThemeData.dark().textTheme.titleMedium),
        titleSmall: monospace.merge(ThemeData.dark().textTheme.titleSmall),
        bodyLarge: roboto.merge(ThemeData.dark().textTheme.bodyLarge),
        bodyMedium: roboto.merge(ThemeData.dark().textTheme.bodyMedium),
        bodySmall: roboto.merge(ThemeData.dark().textTheme.bodySmall),
        labelLarge: roboto.merge(ThemeData.dark().textTheme.labelLarge),
        labelMedium: roboto.merge(ThemeData.dark().textTheme.labelMedium),
        labelSmall: roboto.merge(ThemeData.dark().textTheme.labelSmall),
        headlineLarge: roboto.merge(ThemeData.dark().textTheme.headlineLarge),
        headlineMedium: roboto.merge(ThemeData.dark().textTheme.headlineMedium),
        headlineSmall: roboto.merge(ThemeData.dark().textTheme.headlineSmall),
      );
}
