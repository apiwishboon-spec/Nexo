// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:just_audio/just_audio.dart';
import 'package:path_provider/path_provider.dart';
import 'package:provider/provider.dart';
import 'package:share_plus/share_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../ble/nexo_ble_client.dart';
import '../models/models.dart';
import '../services/ap_connector.dart';
import '../services/speech_transcriber.dart';
import '../services/wifi_joiner.dart';
import '../services/wifi_transfer.dart';
import '../state/app_state.dart';
import '../theme.dart';
import '../widgets/pixel_ui.dart';

/// Lists the voice memos stored on the Cardputer's SD card, over the device's
/// local Wi-Fi AP. Memos can be played, saved/shared and — on macOS —
/// transcribed in-app with Apple's Speech framework.
class MemosScreen extends StatefulWidget {
  const MemosScreen({super.key});

  @override
  State<MemosScreen> createState() => _MemosScreenState();
}

enum _Phase { connecting, joining, listing, ready, error }

class _MemosScreenState extends State<MemosScreen> {
  _Phase _phase = _Phase.connecting;
  String? _message;
  bool _autoJoin = false;


  String _ip = '192.168.4.1';
  bool _apWasEnabled = false;
  Completer<void>? _manualJoin;
  final ApConnector _ap = ApConnector();

  AppState? _app;
  WifiTransfer? _transfer;

  List<MemoItem> _memos = [];
  final Set<String> _busy = {};

  final AudioPlayer _player = AudioPlayer();
  int? _playingIndex;
  bool _playing = false;

  final Map<String, String> _transcripts = {};
  static const _transcriptKey = 'nexo.memo_transcripts';

  @override
  void initState() {
    super.initState();
    _player.playerStateStream.listen((s) {
      if (!mounted) return;
      if (s.processingState == ProcessingState.completed) {
        setState(() {
          _playing = false;
          _playingIndex = null;
        });
        _player.stop();
      }
    });
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      _app = context.read<AppState>();
      _autoJoin = await WifiJoiner.supported();
      await _loadTranscripts();
      await _connect();
    });
  }

  @override
  void dispose() {
    final app = _app;
    if (!_apWasEnabled && app != null && app.connState == NexoConnState.ready) {
      app.saveSettings(app.settings.copyWith(apEnabled: false, pin: null));
    }
    WifiJoiner.leave();
    _transfer?.close();
    _player.dispose();
    _ap.dispose();
    if (_manualJoin != null && !_manualJoin!.isCompleted) {
      _manualJoin!.complete();
    }
    super.dispose();
  }

  Future<void> _loadTranscripts() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_transcriptKey);
    if (raw == null) return;
    try {
      final map = jsonDecode(raw) as Map<String, dynamic>;
      map.forEach((k, v) => _transcripts[k] = v.toString());
      if (mounted) setState(() {});
    } catch (_) {
      // ignore corrupt cache
    }
  }

  Future<void> _saveTranscripts() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_transcriptKey, jsonEncode(_transcripts));
  }

  Future<void> _connect() async {
    final app = _app;
    if (app == null) return;
    setState(() {
      _phase = _Phase.connecting;
      _message = 'Preparing device Wi-Fi…';
    });
    try {
      ApCredentials creds;
      try {
        creds = await _ap.prepare(app, preferAutoJoin: true);
      } catch (e) {
        final m = e.toString().replaceFirst('Exception: ', '');
        if (!mounted) rethrow;
        setState(() {
          _phase = _Phase.joining;
          _message = m;
        });
        _manualJoin = Completer<void>();
        await _manualJoin!.future;
        creds = await _ap.prepare(app, preferAutoJoin: false);
      }
      _apWasEnabled = creds.apWasEnabled;
      _ip = creds.ip;
      _transfer ??= WifiTransfer(baseUrl: 'http://$_ip');
      await _loadMemos();
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _phase = _Phase.error;
        _message = e.toString().replaceFirst('Exception: ', '');
      });
    }
  }

  Future<void> _loadMemos() async {
    setState(() {
      _phase = _Phase.listing;
      _message = 'Loading voice memos…';
    });
    final list = await _transfer!.listMemos();
    if (!mounted) return;
    setState(() {
      _memos = list;
      _phase = _Phase.ready;
      _message = list.isEmpty ? 'No voice memos on the device yet.' : null;
    });
  }

  void _finishManualJoin() {
    final m = _manualJoin;
    if (m != null && !m.isCompleted) m.complete();
    setState(() => _message = 'Checking the connection…');
  }

  Future<void> _openWifiSettings() async {
    await ApConnector.openWifiSettings();
  }

  Directory? _memosDirCache;

  Future<File> _localFile(int index, {bool download = true}) async {
    final m = _memos[index];
    final dir = _memosDirCache ??=
        Directory('${(await getApplicationDocumentsDirectory()).path}/memos');
    if (!await dir.exists()) await dir.create(recursive: true);
    final f = File('${dir.path}/${m.name}');
    if (download && !await f.exists()) {
      final sink = f.openWrite();
      try {
        await _transfer!.downloadMemo(index, sink);
      } finally {
        await sink.close();
      }
    }
    return f;
  }

  Future<void> _togglePlay(int index) async {
    if (_playingIndex == index && _playing) {
      await _player.pause();
      if (mounted) setState(() => _playing = false);
      return;
    }
    if (_busy.contains(_memos[index].name)) return;
    _busy.add(_memos[index].name);
    if (mounted) setState(() => _message = null);
    try {
      final f = await _localFile(index);
      await _player.setFilePath(f.path);
      if (!mounted) return;
      setState(() {
        _playingIndex = index;
        _playing = true;
      });
      // play() completes only when playback finishes, so don't await it.
      unawaited(_player.play());
    } catch (e) {
      if (mounted) setState(() => _message = 'Could not play: $e');
    } finally {
      _busy.remove(_memos[index].name);
      if (mounted) setState(() {});
    }
  }

  Future<void> _share(int index) async {
    final m = _memos[index];
    _busy.add(m.name);
    if (mounted) setState(() {});
    try {
      final f = await _localFile(index);
      await SharePlus.instance.share(ShareParams(
        files: [XFile(f.path)],
        text: 'Nexo voice memo — ${m.name}',
        subject: 'Nexo voice memo',
      ));
    } catch (e) {
      if (mounted) setState(() => _message = 'Could not share: $e');
    } finally {
      _busy.remove(m.name);
      if (mounted) setState(() {});
    }
  }

  Future<void> _transcribe(int index) async {
    final m = _memos[index];
    if (!SpeechTranscriber.supported) {
      // Android & co.: no offline file STT — hand it to the share sheet.
      await _share(index);
      return;
    }
    _busy.add(m.name);
    if (mounted) setState(() => _message = 'Transcribing ${m.name}…');
    try {
      final f = await _localFile(index);
      final text = await SpeechTranscriber.transcribeFile(f.path);
      if (!mounted) return;
      _transcripts[m.name] = text;
      await _saveTranscripts();
      setState(() => _message = text.isEmpty ? 'No speech found.' : null);
      await _showTranscript(m, text);
    } catch (e) {
      if (mounted) setState(() => _message = 'Transcription failed: $e');
    } finally {
      _busy.remove(m.name);
      if (mounted) setState(() {});
    }
  }

  Future<void> _showTranscript(MemoItem m, String initial) async {
    final ctrl = TextEditingController(text: initial);
    final saved = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text('Transcript — ${m.name}',
            style: const TextStyle(fontSize: 16)),
        content: TextField(
          controller: ctrl,
          maxLines: 8,
          minLines: 4,
          decoration: const InputDecoration(
            hintText: 'Recognised text (editable)',
            border: OutlineInputBorder(),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Close'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, ctrl.text),
            child: const Text('Save'),
          ),
        ],
      ),
    );
    if (saved != null) {
      _transcripts[m.name] = saved;
      await _saveTranscripts();
      if (mounted) setState(() {});
    }
  }

  Future<void> _delete(int index) async {
    final m = _memos[index];
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete memo?'),
        content: Text('"${m.name}" will be removed from the device.'),
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
    if (ok != true) return;
    try {
      await _transfer!.deleteMemo(m.name);
      final f = await _localFile(index, download: false);
      if (await f.exists()) await f.delete();
      if (!mounted) return;
      setState(() {
        _memos.removeAt(index);
        _transcripts.remove(m.name);
      });
      await _saveTranscripts();
    } catch (e) {
      if (mounted) setState(() => _message = 'Delete failed: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final busyPhase = _phase == _Phase.connecting ||
        _phase == _Phase.joining ||
        _phase == _Phase.listing;
    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const PixelBackLeading(),
        leadingWidth: 96,
        title: const Text('Voice memos'),
        actions: [
          if (_phase == _Phase.ready)
            IconButton(
              tooltip: 'Reload',
              icon: const Icon(Icons.refresh),
              onPressed: _loadMemos,
            ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (_message != null)
            PixelPanel(
              child: Text(
                _message!,
                style: TextStyle(
                    fontFamily: kFontBody, fontSize: 18, color: scheme.onSurfaceVariant),
              ),
            ),
          if (busyPhase) ...[
            const SizedBox(height: 16),
            const Center(child: CircularProgressIndicator()),
          ],
          if (_phase == _Phase.joining && !_autoJoin) ...[
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: _finishManualJoin,
              icon: const Icon(Icons.check),
              label: const Text('I\'m connected — continue'),
            ),
            const SizedBox(height: 8),
            TextButton.icon(
              onPressed: _openWifiSettings,
              icon: const Icon(Icons.wifi),
              label: const Text('Open Wi-Fi settings'),
            ),
          ],
          if (_phase == _Phase.error) ...[
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: _connect,
              icon: const Icon(Icons.refresh),
              label: const Text('Try again'),
            ),
          ],
          if (_phase == _Phase.ready && _memos.isNotEmpty) ...[
            const SizedBox(height: 8),
            Text(
              '${_memos.length} memo(s) on the device',
              style: TextStyle(color: scheme.onSurfaceVariant),
            ),
            const SizedBox(height: 8),
            for (var i = 0; i < _memos.length; i++) _memoTile(i, scheme),
          ],
        ],
      ),
    );
  }

  Widget _memoTile(int index, ColorScheme scheme) {
    final m = _memos[index];
    final isPlaying = _playingIndex == index && _playing;
    final isBusy = _busy.contains(m.name);
    final transcript = _transcripts[m.name];
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: Column(
        children: [
          ListTile(
            leading: IconButton.filledTonal(
              tooltip: isPlaying ? 'Pause' : 'Play',
              icon: isBusy
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : Icon(isPlaying ? Icons.pause : Icons.play_arrow),
              onPressed: isBusy ? null : () => _togglePlay(index),
            ),
            title: Text(m.name,
                style: const TextStyle(fontFamily: kFontBody, fontSize: 18)),
            subtitle: Text('${(m.size / 1024).toStringAsFixed(0)} KB'),
            trailing: PopupMenuButton<String>(
              onSelected: (v) {
                switch (v) {
                  case 'share':
                    _share(index);
                  case 'transcribe':
                    _transcribe(index);
                  case 'delete':
                    _delete(index);
                }
              },
              itemBuilder: (_) => const [
                PopupMenuItem(value: 'share', child: Text('Share / save')),
                PopupMenuItem(value: 'transcribe', child: Text('Transcribe')),
                PopupMenuItem(value: 'delete', child: Text('Delete')),
              ],
            ),
          ),
          if (transcript != null && transcript.isNotEmpty)
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
              child: Align(
                alignment: Alignment.centerLeft,
                child: Text(
                  transcript,
                  style: TextStyle(
                      fontFamily: kFontBody,
                      fontSize: 17,
                      color: scheme.onSurfaceVariant),
                ),
              ),
            ),
        ],
      ),
    );
  }
}
