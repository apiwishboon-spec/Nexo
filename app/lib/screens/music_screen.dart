// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/nexo_ble_client.dart';
import '../models/models.dart';
import '../state/app_state.dart';
import '../theme.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';
import 'music_upload_screen.dart';

/// Mirror of the Cardputer's Music app: browse the SD library, control
/// playback over BLE, and import audio over Wi-Fi.
class MusicScreen extends StatefulWidget {
  const MusicScreen({super.key});

  @override
  State<MusicScreen> createState() => _MusicScreenState();
}

class _MusicScreenState extends State<MusicScreen> {
  bool _loaded = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final app = context.read<AppState>();
      if (app.connState == NexoConnState.ready && !_loaded) {
        _loaded = true;
        app.refreshMusic();
      }
    });
  }

  String _fmtSize(int bytes) {
    if (bytes <= 0) return '';
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(0)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  String _fmtTime(int sec) {
    final m = (sec ~/ 60).toString().padLeft(2, '0');
    final s = (sec % 60).toString().padLeft(2, '0');
    return '$m:$s';
  }

  Future<void> _confirmDelete(AppState app, MusicTrack t) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete track'),
        content: Text('Remove "${t.name}" from the device?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (ok == true) await app.deleteMusic(t.name);
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final scheme = Theme.of(context).colorScheme;
    final ready = app.connState == NexoConnState.ready;
    final np = app.nowPlaying;

    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32, mood: PetMood.happy),
        ),
        leadingWidth: 52,
        title: const Text('Music'),
        actions: [
          IconButton(
            tooltip: 'Add audio…',
            icon: const Icon(Icons.upload_file),
            onPressed: ready ? () => _openUpload(app) : null,
          ),
          IconButton(
            tooltip: 'Reload',
            icon: const Icon(Icons.refresh),
            onPressed: ready ? () => app.refreshMusic() : null,
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (!ready)
            PixelPanel(
              padding: EdgeInsets.zero,
              child: ListTile(
                leading: Icon(Icons.info_outline, color: scheme.onSurfaceVariant),
                title: const Text('Not connected'),
                subtitle: const Text('Connect to your Cardputer to manage music.'),
              ),
            )
          else ...[
            _nowPlayingCard(app, np, scheme),
            const SizedBox(height: 16),
            Row(
              children: [
                const PixelLabel('Library'),
                const Spacer(),
                Text(
                  '${app.music.length} track(s)',
                  style: TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            if (app.music.isEmpty)
              PixelPanel(
                child: Text(
                  'No tracks yet. Tap the upload icon to add MP3/WAV files over '
                  'Wi-Fi — they land in /music on the SD card.',
                  style: TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              )
            else
              ...app.music.asMap().entries.map((e) {
                final i = e.key;
                final t = e.value;
                final active = np?.index == i;
                return Padding(
                  padding: const EdgeInsets.only(bottom: 8),
                  child: PixelPanel(
                    padding: EdgeInsets.zero,
                    accent: active ? NexoColors.primary : null,
                    child: ListTile(
                      leading: Icon(
                        active && (np?.playing ?? false)
                            ? Icons.graphic_eq
                            : Icons.music_note,
                        color: active ? NexoColors.primary : scheme.onSurfaceVariant,
                      ),
                      title: Text(t.name),
                      subtitle: Text(_fmtSize(t.size)),
                      trailing: IconButton(
                        icon: const Icon(Icons.delete_outline),
                        onPressed: () => _confirmDelete(app, t),
                      ),
                      onTap: () => app.musicPlayIndex(i),
                    ),
                  ),
                );
              }),
          ],
        ],
      ),
    );
  }

  Widget _nowPlayingCard(AppState app, MusicStatus? np, ColorScheme scheme) {
    final playing = np?.playing ?? false;
    final hasTrack = np?.hasTrack ?? false;
    final pos = np?.positionSec ?? 0;
    final dur = np?.durationSec ?? 0;
    final progress = dur > 0 ? (pos / dur).clamp(0.0, 1.0) : 0.0;

    return PixelPanel(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              PixelTag(playing ? 'Playing' : 'Paused',
                  color: playing ? NexoColors.success : scheme.onSurfaceVariant),
              const Spacer(),
              Text(
                '${_fmtTime(pos)} / ${_fmtTime(dur)}',
                style: const TextStyle(
                  fontFamily: kFontBody,
                  fontSize: 18,
                  color: NexoColors.onSurfaceVariant,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          LinearProgressIndicator(value: progress, minHeight: 6),
          const SizedBox(height: 14),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              IconButton.filledTonal(
                onPressed: () => app.musicPrev(),
                icon: const Icon(Icons.skip_previous),
              ),
              const SizedBox(width: 12),
              IconButton.filled(
                iconSize: 32,
                onPressed: () => app.musicToggle(),
                icon: Icon(playing ? Icons.pause : Icons.play_arrow),
              ),
              const SizedBox(width: 12),
              IconButton.filledTonal(
                onPressed: () => app.musicNext(),
                icon: const Icon(Icons.skip_next),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Icon(Icons.volume_down, color: scheme.onSurfaceVariant),
              Expanded(
                child: Slider(
                  value: (np?.volume ?? app.settings.volume).toDouble(),
                  max: 100,
                  divisions: 20,
                  label: '${np?.volume ?? app.settings.volume}',
                  onChanged: (v) {},
                  onChangeEnd: (v) => app.musicSetVolume(v.round()),
                ),
              ),
              Icon(Icons.volume_up, color: scheme.onSurfaceVariant),
            ],
          ),
          if (hasTrack)
            Text(
              'Track ${(np!.index) + 1}',
              style: const TextStyle(
                fontFamily: kFontBody,
                fontSize: 16,
                color: NexoColors.onSurfaceVariant,
              ),
            ),
        ],
      ),
    );
  }

  Future<void> _openUpload(AppState app) async {
    await Navigator.of(context).push(
      MaterialPageRoute(builder: (_) => const MusicUploadScreen()),
    );
  }
}
