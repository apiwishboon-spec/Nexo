// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';

/// Palette mirroring `firmware/src/ui/theme.h` so the phone app and the
/// Cardputer share one visual identity.
class NexoColors {
  const NexoColors._();

  static const background = Color(0xFF121316);
  static const surface = Color(0xFF1E2025);
  static const surfaceVariant = Color(0xFF2A2D33);
  static const primary = Color(0xFFA8C7FA);
  static const onPrimary = Color(0xFF062020);
  static const onSurface = Color(0xFFE3E3E6);
  static const onSurfaceVariant = Color(0xFF9AA0A6);
  static const outline = Color(0xFF3A3D44);
  static const error = Color(0xFFF2B8B5);
  static const success = Color(0xFFA0D6A8);

  /// Extra tones used by the pixel mascot / decorations.
  static const ink = Color(0xFF0B0C0F);
  static const shade = Color(0xFF6E8FD0);
  static const shadow = Color(0xFF07080A);
}

const String kFontDisplay = 'PressStart2P';
const String kFontBody = 'VT323';

ThemeData buildTheme([Brightness brightness = Brightness.dark]) {
  const scheme = ColorScheme.dark(
    primary: NexoColors.primary,
    onPrimary: NexoColors.onPrimary,
    secondary: NexoColors.success,
    onSecondary: NexoColors.onPrimary,
    tertiary: NexoColors.primary,
    onTertiary: NexoColors.onPrimary,
    error: NexoColors.error,
    onError: NexoColors.onPrimary,
    surface: NexoColors.background,
    onSurface: NexoColors.onSurface,
    surfaceContainerLowest: NexoColors.background,
    surfaceContainerLow: NexoColors.surface,
    surfaceContainer: NexoColors.surface,
    surfaceContainerHigh: NexoColors.surfaceVariant,
    surfaceContainerHighest: NexoColors.surfaceVariant,
    onSurfaceVariant: NexoColors.onSurfaceVariant,
    outline: NexoColors.outline,
    outlineVariant: NexoColors.outline,
  );

  final base = Typography.material2021(platform: TargetPlatform.android)
      .white
      .apply(
        fontFamily: kFontBody,
        bodyColor: NexoColors.onSurface,
        displayColor: NexoColors.onSurface,
      );

  final text = base.copyWith(
    displayLarge: _display(20, NexoColors.onSurface),
    displayMedium: _display(18, NexoColors.onSurface),
    displaySmall: _display(14, NexoColors.primary),
    headlineMedium: _display(13, NexoColors.onSurface),
    headlineSmall: _display(12, NexoColors.onSurface),
    titleLarge: _display(11, NexoColors.primary),
    titleMedium: _display(9, NexoColors.onSurface),
    titleSmall: _display(8, NexoColors.onSurfaceVariant),
    labelLarge: _display(9, NexoColors.onSurface),
    labelMedium: _display(8, NexoColors.onSurfaceVariant),
    labelSmall: _display(7, NexoColors.onSurfaceVariant),
    bodyLarge: _body(20),
    bodyMedium: _body(18),
    bodySmall: _body(16),
  );

  const pixelShape = RoundedRectangleBorder(
    borderRadius: BorderRadius.all(Radius.circular(4)),
    side: BorderSide(color: NexoColors.outline, width: 2),
  );

  return ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    colorScheme: scheme,
    fontFamily: kFontBody,
    textTheme: text,
    primaryTextTheme: text,
    scaffoldBackgroundColor: NexoColors.background,
    canvasColor: NexoColors.background,
    dividerTheme: const DividerThemeData(
      color: NexoColors.outline,
      thickness: 2,
      space: 2,
    ),
    appBarTheme: AppBarTheme(
      backgroundColor: NexoColors.surface,
      surfaceTintColor: Colors.transparent,
      foregroundColor: NexoColors.onSurface,
      elevation: 0,
      centerTitle: false,
      toolbarHeight: 60,
      titleTextStyle: text.titleLarge,
      shape: const Border(
        bottom: BorderSide(color: NexoColors.outline, width: 2),
      ),
    ),
    cardTheme: CardThemeData(
      elevation: 0,
      margin: EdgeInsets.zero,
      color: NexoColors.surface,
      surfaceTintColor: Colors.transparent,
      shape: pixelShape,
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: NexoColors.surfaceVariant,
      contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 16),
      hintStyle: text.bodyMedium?.copyWith(color: NexoColors.onSurfaceVariant),
      labelStyle: text.labelMedium,
      floatingLabelStyle: text.labelMedium?.copyWith(color: NexoColors.primary),
      border: const OutlineInputBorder(
        borderRadius: BorderRadius.all(Radius.circular(4)),
        borderSide: BorderSide(color: NexoColors.outline, width: 2),
      ),
      enabledBorder: const OutlineInputBorder(
        borderRadius: BorderRadius.all(Radius.circular(4)),
        borderSide: BorderSide(color: NexoColors.outline, width: 2),
      ),
      focusedBorder: const OutlineInputBorder(
        borderRadius: BorderRadius.all(Radius.circular(4)),
        borderSide: BorderSide(color: NexoColors.primary, width: 2),
      ),
      errorBorder: const OutlineInputBorder(
        borderRadius: BorderRadius.all(Radius.circular(4)),
        borderSide: BorderSide(color: NexoColors.error, width: 2),
      ),
      counterStyle: text.labelSmall,
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: NexoColors.primary,
        foregroundColor: NexoColors.onPrimary,
        textStyle: text.labelLarge,
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(4)),
          side: BorderSide(color: NexoColors.primary, width: 2),
        ),
      ),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: NexoColors.primary,
        textStyle: text.labelLarge,
        padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
        side: const BorderSide(color: NexoColors.outline, width: 2),
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(4)),
        ),
      ),
    ),
    textButtonTheme: TextButtonThemeData(
      style: TextButton.styleFrom(
        foregroundColor: NexoColors.primary,
        textStyle: text.labelLarge,
      ),
    ),
    iconTheme: const IconThemeData(color: NexoColors.onSurfaceVariant),
    floatingActionButtonTheme: const FloatingActionButtonThemeData(
      backgroundColor: NexoColors.primary,
      foregroundColor: NexoColors.onPrimary,
      elevation: 4,
    ),
    segmentedButtonTheme: SegmentedButtonThemeData(
      style: ButtonStyle(
        textStyle: WidgetStatePropertyAll(text.labelSmall),
        backgroundColor: const WidgetStatePropertyAll(NexoColors.surface),
        foregroundColor: WidgetStateProperty.resolveWith(
          (s) => s.contains(WidgetState.selected)
              ? NexoColors.onPrimary
              : NexoColors.onSurfaceVariant,
        ),
        side: const WidgetStatePropertyAll(
          BorderSide(color: NexoColors.outline, width: 2),
        ),
      ),
    ),
    chipTheme: ChipThemeData(
      backgroundColor: NexoColors.surfaceVariant,
      side: const BorderSide(color: NexoColors.outline, width: 2),
      labelStyle: text.labelSmall,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.all(Radius.circular(4)),
      ),
    ),
    navigationBarTheme: NavigationBarThemeData(
      backgroundColor: NexoColors.surface,
      surfaceTintColor: Colors.transparent,
      indicatorColor: NexoColors.surfaceVariant,
      elevation: 0,
      height: 68,
      labelBehavior: NavigationDestinationLabelBehavior.alwaysShow,
      labelTextStyle: WidgetStatePropertyAll(text.labelSmall),
      iconTheme: WidgetStateProperty.resolveWith(
        (states) => IconThemeData(
          color: states.contains(WidgetState.selected)
              ? NexoColors.primary
              : NexoColors.onSurfaceVariant,
        ),
      ),
    ),
    navigationRailTheme: const NavigationRailThemeData(
      backgroundColor: NexoColors.surface,
      indicatorColor: NexoColors.surfaceVariant,
      selectedIconTheme: IconThemeData(color: NexoColors.primary),
      unselectedIconTheme: IconThemeData(color: NexoColors.onSurfaceVariant),
      selectedLabelTextStyle: TextStyle(
        fontFamily: kFontDisplay,
        fontSize: 8,
        color: NexoColors.primary,
      ),
      unselectedLabelTextStyle: TextStyle(
        fontFamily: kFontDisplay,
        fontSize: 8,
        color: NexoColors.onSurfaceVariant,
      ),
    ),
    dialogTheme: DialogThemeData(
      backgroundColor: NexoColors.surface,
      surfaceTintColor: Colors.transparent,
      shape: pixelShape,
      titleTextStyle: text.titleMedium,
      contentTextStyle: text.bodyMedium,
    ),
    snackBarTheme: SnackBarThemeData(
      backgroundColor: NexoColors.surfaceVariant,
      contentTextStyle: text.bodyMedium,
      behavior: SnackBarBehavior.floating,
      shape: pixelShape,
    ),
    listTileTheme: ListTileThemeData(
      iconColor: NexoColors.onSurfaceVariant,
      textColor: NexoColors.onSurface,
      titleTextStyle: text.titleSmall,
      subtitleTextStyle: text.bodyMedium?.copyWith(color: NexoColors.onSurfaceVariant),
    ),
    progressIndicatorTheme: const ProgressIndicatorThemeData(
      color: NexoColors.primary,
    ),
    switchTheme: SwitchThemeData(
      thumbColor: WidgetStateProperty.resolveWith(
        (s) => s.contains(WidgetState.selected)
            ? NexoColors.primary
            : NexoColors.onSurfaceVariant,
      ),
      trackColor: WidgetStateProperty.resolveWith(
        (s) => s.contains(WidgetState.selected)
            ? NexoColors.surfaceVariant
            : NexoColors.outline,
      ),
      trackOutlineColor: const WidgetStatePropertyAll(NexoColors.outline),
    ),
  );
}

TextStyle _display(double size, Color color) => TextStyle(
      fontFamily: kFontDisplay,
      fontSize: size,
      height: 1.6,
      letterSpacing: 0.5,
      color: color,
    );

TextStyle _body(double size) => TextStyle(
      fontFamily: kFontBody,
      fontSize: size,
      height: 1.1,
      color: NexoColors.onSurface,
    );
