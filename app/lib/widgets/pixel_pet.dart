// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/nexo_ble_client.dart';
import '../state/app_state.dart';
import '../theme.dart';

/// Mood of the mascot. When not supplied it is derived from [AppState].
enum PetMood { idle, happy, busy, sleepy, error }

/// A tiny 8-bit companion drawn entirely in code (no image assets) that
/// reacts to the connection / sync state of the Cardputer.
class PixelPet extends StatefulWidget {
  const PixelPet({
    super.key,
    this.size = 48,
    this.mood,
    this.animate = true,
  });

  final double size;

  /// Force a mood; when null the mood follows [AppState].
  final PetMood? mood;

  /// Set false to render a still frame (handy in tests).
  final bool animate;

  @override
  State<PixelPet> createState() => _PixelPetState();
}

class _PixelPetState extends State<PixelPet>
    with SingleTickerProviderStateMixin {
  late final AnimationController _bob;
  bool _blink = true;

  @override
  void initState() {
    super.initState();
    _controllerSetup();
  }

  void _controllerSetup() {
    _bob = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    );
    if (widget.animate) {
      _bob.repeat(reverse: true);
      _scheduleBlink();
    }
  }

  void _scheduleBlink() {
    _blink = true;
    Timer(Duration(milliseconds: 2600 + math.Random().nextInt(2400)), () {
      if (!mounted) return;
      setState(() => _blink = false);
      Future.delayed(const Duration(milliseconds: 120), () {
        if (!mounted) return;
        setState(() => _blink = true);
        _scheduleBlink();
      });
    });
  }

  @override
  void dispose() {
    _bob.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final mood = widget.mood ?? _moodFrom(app);
    return AnimatedBuilder(
      animation: _bob,
      builder: (context, _) {
        final phase = widget.animate ? _bob.value : 0.0;
        return CustomPaint(
          size: Size.square(widget.size),
          painter: _PetPainter(
            mood: mood,
            phase: phase,
            eyesOpen: _blink && mood != PetMood.sleepy,
          ),
        );
      },
    );
  }

  PetMood _moodFrom(AppState app) {
    if (app.error != null && app.error!.isNotEmpty) return PetMood.error;
    switch (app.connState) {
      case NexoConnState.ready:
        return PetMood.happy;
      case NexoConnState.connecting:
      case NexoConnState.authenticating:
        return PetMood.busy;
      case NexoConnState.connected:
        return app.busy ? PetMood.busy : PetMood.idle;
      case NexoConnState.disconnected:
        return PetMood.sleepy;
    }
  }
}

const _petSprite = <String>[
  '................',
  '.......AA.......',
  '.......KK.......',
  '...KKKKKKKKKK...',
  '..KBBBBBBBBBBK..',
  '..KBBBBBBBBBBK..',
  '..KBWWBBBBWWBK..',
  '..KBPPBBBBPPBK..',
  '..KCBBBBBBBBCK..',
  '..KBBBBPPBBBBK..',
  '..KBBBBBBBBBBK..',
  '..KBBBBBBBBBBK..',
  '...KKBBBBBBKK...',
  '....KKKKKKKK....',
  '.....KK..KK.....',
  '................',
];

class _Palette {
  const _Palette(this.body, this.shade, this.accent);
  final Color body;
  final Color shade;
  final Color accent;
}

class _PetPainter extends CustomPainter {
  _PetPainter({
    required this.mood,
    required this.phase,
    required this.eyesOpen,
  });

  final PetMood mood;
  final double phase;
  final bool eyesOpen;

  static const _outline = NexoColors.ink;
  static const _white = Color(0xFFFFFFFF);
  static const _pupil = NexoColors.ink;
  static const _cheek = NexoColors.error;

  _Palette get _palette => switch (mood) {
        PetMood.idle => const _Palette(
            NexoColors.primary, NexoColors.shade, NexoColors.success),
        PetMood.happy => const _Palette(
            NexoColors.success, Color(0xFF5E9C6A), NexoColors.primary),
        PetMood.busy => const _Palette(
            NexoColors.primary, NexoColors.shade, NexoColors.success),
        PetMood.sleepy => const _Palette(
            NexoColors.shade, Color(0xFF44548A), NexoColors.onSurfaceVariant),
        PetMood.error => const _Palette(
            NexoColors.error, Color(0xFFC98A88), NexoColors.error),
      };

  @override
  void paint(Canvas canvas, Size size) {
    const n = 16;
    final cell = size.width / n;
    final pal = _palette;

    final bob = mood == PetMood.sleepy ? 0 : (phase < 0.5 ? 0 : 1);
    final dy = bob * cell;

    final paint = Paint()..isAntiAlias = false;

    void px(int col, int row, Color c) {
      paint.color = c;
      canvas.drawRect(
        Rect.fromLTWH(col * cell, row * cell + dy, cell + 0.5, cell + 0.5),
        paint,
      );
    }

    Color? colorFor(String ch) {
      switch (ch) {
        case 'K':
          return _outline;
        case 'B':
          return pal.body;
        case 'D':
          return pal.shade;
        case 'W':
          return _white;
        case 'P':
          return _pupil;
        case 'C':
          return _cheek;
        case 'A':
          return pal.accent;
        default:
          return null;
      }
    }

    for (var row = 0; row < n; row++) {
      final line = _petSprite[row];
      for (var col = 0; col < n; col++) {
        var ch = line[col];
        if (ch == '.') continue;

        // Eye animation.
        final isEye = (row == 6 || row == 7) &&
            (col == 4 || col == 5 || col == 10 || col == 11);
        if (isEye) {
          if (mood == PetMood.error) {
            ch = 'P';
          } else if (!eyesOpen) {
            ch = row == 6 ? 'B' : 'P';
          }
        }

        // Mouth: cheer wider when happy.
        if (row == 9 && mood == PetMood.happy && (col == 6 || col == 9)) {
          ch = 'P';
        }

        final c = colorFor(ch);
        if (c != null) px(col, row, c);
      }
    }

    _paintExtras(canvas, cell);

    if (mood == PetMood.busy) {
      final positions = <Offset>[
        const Offset(1, 4),
        const Offset(14, 4),
        const Offset(14, 11),
        const Offset(1, 11),
      ];
      final step = (phase * positions.length).floor() % positions.length;
      for (var i = 0; i < positions.length; i++) {
        final on = i == step;
        final p = positions[i];
        px(p.dx.toInt(), p.dy.toInt(), on ? NexoColors.primary : NexoColors.outline);
      }
    }
  }

  void _paintExtras(Canvas canvas, double cell) {
    if (mood == PetMood.happy) {
      final sparkle = Paint()..color = NexoColors.primary;
      void plus(int col, int row) {
        final x = col * cell;
        final y = row * cell;
        canvas.drawRect(Rect.fromLTWH(x, y + cell, cell, cell), sparkle);
        canvas.drawRect(Rect.fromLTWH(x + cell, y, cell, cell), sparkle);
        canvas.drawRect(Rect.fromLTWH(x + cell, y + cell, cell, cell), sparkle);
        canvas.drawRect(Rect.fromLTWH(x + cell, y + cell * 2, cell, cell), sparkle);
      }

      plus(1, 1);
      plus(13, 3);
    }

    if (mood == PetMood.sleepy) {
      final tp = TextPainter(
        text: const TextSpan(
          text: 'z z',
          style: TextStyle(
            fontFamily: kFontDisplay,
            fontSize: 8,
            color: NexoColors.onSurfaceVariant,
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      final dx = 12 * cell - (tp.width / 2);
      final dy = 0.5 * cell;
      tp.paint(canvas, Offset(dx.clamp(0, double.infinity), dy));
    }
  }

  @override
  bool shouldRepaint(_PetPainter old) =>
      old.mood != mood || old.phase != phase || old.eyesOpen != eyesOpen;
}
