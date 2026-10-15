// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';

import '../theme.dart';

/// Faint pixel grid painted behind every screen for an 8-bit feel.
class PixelGrid extends StatelessWidget {
  const PixelGrid({super.key, this.cell = 16, this.child});

  final double cell;
  final Widget? child;

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: _GridPainter(cell),
      child: child,
    );
  }
}

class _GridPainter extends CustomPainter {
  _GridPainter(this.cell);
  final double cell;

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = NexoColors.outline.withValues(alpha: 0.22)
      ..strokeWidth = 1;
    for (double x = 0; x <= size.width; x += cell) {
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), paint);
    }
    for (double y = 0; y <= size.height; y += cell) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), paint);
    }
  }

  @override
  bool shouldRepaint(_GridPainter old) => old.cell != cell;
}

/// A chunky bordered panel with a hard drop shadow (no soft blur).
class PixelPanel extends StatelessWidget {
  const PixelPanel({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(16),
    this.color,
    this.accent,
  });

  final Widget child;
  final EdgeInsetsGeometry padding;
  final Color? color;
  final Color? accent;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: color ?? NexoColors.surface,
        border: Border.all(
          color: accent ?? NexoColors.outline,
          width: 2,
        ),
        borderRadius: BorderRadius.circular(4),
        boxShadow: const [
          BoxShadow(
            color: NexoColors.shadow,
            offset: Offset(3, 3),
            blurRadius: 0,
          ),
        ],
      ),
      padding: padding,
      child: child,
    );
  }
}

/// Small uppercase pixel label used as section headers.
class PixelLabel extends StatelessWidget {
  const PixelLabel(this.text, {super.key, this.color});

  final String text;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    return Text(
      text.toUpperCase(),
      style: TextStyle(
        fontFamily: kFontDisplay,
        fontSize: 8,
        letterSpacing: 1.5,
        color: color ?? NexoColors.onSurfaceVariant,
      ),
    );
  }
}

/// A tiny status pill (e.g. "READY", "OFFLINE").
class PixelTag extends StatelessWidget {
  const PixelTag(this.text, {super.key, required this.color});

  final String text;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.16),
        border: Border.all(color: color, width: 1.5),
        borderRadius: BorderRadius.circular(3),
      ),
      child: Text(
        text.toUpperCase(),
        style: TextStyle(
          fontFamily: kFontDisplay,
          fontSize: 7,
          letterSpacing: 1,
          color: color,
        ),
      ),
    );
  }
}
