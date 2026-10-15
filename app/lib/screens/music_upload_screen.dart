// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';
import 'dart:io';

import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/nexo_ble_client.dart';
import '../services/wifi_joiner.dart';
import '../services/wifi_transfer.dart';
import '../state/app_state.dart';
import '../theme.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';

/// Imports audio onto the device's SD card over Wi-Fi. The flow:
///   1. switch the device AP on (BLE),
///   2. join that AP from the phone (Android auto; elsewhere manual),
///   3. pick files and stream them to `/music`,
///   4. restore the previous AP state.
class MusicUploadScreen extends StatefulWidget {
  const MusicUploadScreen({super.key});

  @override
  State<MusicUploadScreen> createState() => _MusicUploadScreenState();
}

enum _Phase { idle, enablingAp, joining, ready, uploading, done, error }

class _MusicUploadScreenState extends State<MusicUploadScreen> {
  _Phase _phase = _Phase.idle;
  String? _message;
  bool _autoJoin = false;

  String _ssid = '';
  String _password = '';
  String _ip = '192.168.4.1';
  bool _apWasEnabled = false;
  bool _restoredAp = false;

  WifiTransfer? _transfer;

  AppState? _app;

  final List<XFile> _picked = [];
  int _uploadIndex = 0;
  double _uploadProgress = 0;
  final List<String> _uploaded = [];
  final List<String> _failed = [];

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      _app = context.read<AppState>();
      _autoJoin = await WifiJoiner.supported();
      setState(() {});
    });
  }

  @override
  void dispose() {
    // Restore the device AP state if we changed it.
    final app = _app;
    if (!_restoredAp &&
        _apWasEnabled == false &&
        app != null &&
        app.connState == NexoConnState.ready) {
      app.saveSettings(app.settings.copyWith(apEnabled: false, pin: null));
    }
    WifiJoiner.leave();
    _transfer?.close();
    super.dispose();
  }

  Future<void> _beginTransfer() async {
    final app = context.read<AppState>();
    setState(() {
      _phase = _Phase.enablingAp;
      _message = 'Switching on the device access point…';
    });
    try {
      _apWasEnabled = app.settings.apEnabled;
      // Turn the AP on and pull the credentials (preserving all other settings).
      await app
          .saveSettings(app.settings.copyWith(apEnabled: true, pin: null));
      await Future.delayed(const Duration(milliseconds: 800));
      await app.refreshDeviceDetails();
      final d = app.details;
      if (d == null) {
        throw Exception('Device did not report its Wi-Fi details.');
      }
      _ssid = d.apSsid.isNotEmpty ? d.apSsid : 'Nexo-AP';
      _password = d.apPassword;
      _ip = d.ip.isNotEmpty && d.ip != '-' ? d.ip : '192.168.4.1';

      setState(() {
        _phase = _Phase.joining;
        _message = _autoJoin
            ? 'Joining $_ssid…'
            : 'Connect your phone to Wi-Fi "$_ssid" (password "$_password"), then tap Continue.';
      });

      if (_autoJoin) {
        final ok = await WifiJoiner.join(ssid: _ssid, password: _password);
        if (!ok) {
          throw Exception('Could not join "$_ssid". Connect manually and tap Continue.');
        }
        await Future.delayed(const Duration(milliseconds: 600));
      }
      setState(() {
        _phase = _Phase.ready;
        _message = _autoJoin
            ? 'Connected to $_ssid.'
            : 'When your phone is on "$_ssid", tap Continue.';
      });
    } catch (e) {
      setState(() {
        _phase = _Phase.error;
        _message = e.toString();
      });
    }
  }

  Future<void> _pickFiles() async {
    const typeGroup = XTypeGroup(
      label: 'Audio',
      extensions: ['mp3', 'wav'],
      mimeTypes: ['audio/mpeg', 'audio/wav'],
    );
    final files = await openFiles(acceptedTypeGroups: const [typeGroup]);
    if (files.isEmpty) return;
    setState(() {
      _picked
        ..clear()
        ..addAll(files);
    });
  }

  Future<void> _upload() async {
    if (_picked.isEmpty) {
      await _pickFiles();
      if (_picked.isEmpty) return;
    }
    setState(() {
      _phase = _Phase.uploading;
      _uploadIndex = 0;
      _uploadProgress = 0;
      _uploaded.clear();
      _failed.clear();
    });

    final transfer = _transfer ??= WifiTransfer(baseUrl: 'http://$_ip');
    try {
      final pin = await _app!.savedPin();
      await transfer.login(pin);

      for (var i = 0; i < _picked.length; i++) {
        final f = _picked[i];
        setState(() => _uploadIndex = i);
        try {
          final file = File(f.path);
          final length = await file.length();
          await transfer.upload(
            name: f.name,
            bytes: file.openRead(),
            length: length,
            onProgress: (sent, total) {
              setState(() => _uploadProgress =
                  length == 0 ? 0 : (sent / total).clamp(0.0, 1.0));
            },
          );
          _uploaded.add(f.name);
        } catch (e) {
          _failed.add('${f.name}: $e');
        }
      }
      await _app!.refreshMusic();
      setState(() {
        _phase = _Phase.done;
        _message = 'Uploaded ${_uploaded.length} file(s)'
            '${_failed.isEmpty ? '' : ', ${_failed.length} failed'}.';
      });
    } catch (e) {
      setState(() {
        _phase = _Phase.error;
        _message = 'Upload failed: $e';
      });
    }
  }

  Future<void> _finish() async {
    final app = _app;
    if (!_apWasEnabled && app != null && app.connState == NexoConnState.ready) {
      try {
        await app.saveSettings(app.settings.copyWith(apEnabled: false, pin: null));
        _restoredAp = true;
      } catch (_) {}
    }
    await WifiJoiner.leave();
    if (mounted) Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Scaffold(
      backgroundColor: NexoColors.background,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: const Text('Add audio'),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          PixelPanel(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const PixelLabel('How it works'),
                const SizedBox(height: 8),
                Text(
                  'Music is too large for Bluetooth, so Nexo uses the device\'s '
                  'Wi-Fi. The app turns the access point on, joins it, streams '
                  'your files to /music on the SD card, then turns it back off.',
                  style: TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    color: scheme.onSurfaceVariant,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          _statusCard(scheme),
          const SizedBox(height: 16),
          if (_phase == _Phase.idle)
            FilledButton.icon(
              onPressed: _beginTransfer,
              icon: const Icon(Icons.wifi),
              label: const Text('Start transfer'),
            ),
          if (_phase == _Phase.enablingAp || _phase == _Phase.joining)
            const Center(child: CircularProgressIndicator()),
          if (_phase == _Phase.ready || _phase == _Phase.done) ...[
            _credentialsCard(scheme),
            const SizedBox(height: 16),
            Row(
              children: [
                OutlinedButton.icon(
                  onPressed: _pickFiles,
                  icon: const Icon(Icons.audio_file),
                  label: Text(_picked.isEmpty
                      ? 'Choose files'
                      : '${_picked.length} selected'),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: FilledButton.icon(
                    onPressed: _phase == _Phase.uploading ? null : _upload,
                    icon: const Icon(Icons.cloud_upload),
                    label: Text(_picked.isEmpty ? 'Upload picked' : 'Upload now'),
                  ),
                ),
              ],
            ),
            if (_phase == _Phase.uploading) ...[
              const SizedBox(height: 16),
              LinearProgressIndicator(value: _uploadProgress, minHeight: 8),
              const SizedBox(height: 8),
              Text(
                'Uploading ${_uploadIndex + 1}/${_picked.length}…',
                style: const TextStyle(
                  fontFamily: kFontBody,
                  fontSize: 18,
                  color: NexoColors.onSurfaceVariant,
                ),
              ),
            ],
            if (_uploaded.isNotEmpty) ...[
              const SizedBox(height: 12),
              ..._uploaded.map((n) => Text('✓ $n',
                  style: const TextStyle(
                      fontFamily: kFontBody,
                      fontSize: 16,
                      color: NexoColors.success))),
            ],
            if (_failed.isNotEmpty) ...[
              const SizedBox(height: 12),
              ..._failed.map((n) => Text('✗ $n',
                  style: const TextStyle(
                      fontFamily: kFontBody,
                      fontSize: 16,
                      color: NexoColors.error))),
            ],
            const SizedBox(height: 24),
            FilledButton.icon(
              onPressed: _finish,
              icon: const Icon(Icons.check),
              label: const Text('Done'),
            ),
          ],
          if (_phase == _Phase.error) ...[
            OutlinedButton.icon(
              onPressed: () {
                setState(() {
                  _phase = _Phase.idle;
                  _message = null;
                });
              },
              icon: const Icon(Icons.refresh),
              label: const Text('Try again'),
            ),
            const SizedBox(height: 12),
            FilledButton.icon(
              onPressed: _finish,
              icon: const Icon(Icons.check),
              label: const Text('Close'),
            ),
          ],
        ],
      ),
    );
  }

  Widget _statusCard(ColorScheme scheme) {
    if (_message == null) return const SizedBox.shrink();
    final isError = _phase == _Phase.error;
    return PixelPanel(
      accent: isError ? NexoColors.error : null,
      child: Row(
        children: [
          Icon(isError ? Icons.error_outline : Icons.info_outline,
              color: isError ? NexoColors.error : scheme.primary),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              _message!,
              style: const TextStyle(fontFamily: kFontBody, fontSize: 18),
            ),
          ),
        ],
      ),
    );
  }

  Widget _credentialsCard(ColorScheme scheme) {
    return PixelPanel(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const PixelLabel('Network'),
          const SizedBox(height: 8),
          _kv('SSID', _ssid),
          _kv('Password', _password),
          _kv('Device IP', _ip),
          if (!_autoJoin) ...[
            const SizedBox(height: 8),
            Text(
              'Join this Wi-Fi in your phone settings, then tap Upload.',
              style: TextStyle(
                fontFamily: kFontBody,
                fontSize: 16,
                color: scheme.onSurfaceVariant,
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _kv(String k, String v) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        children: [
          SizedBox(
            width: 90,
            child: Text(k,
                style: const TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    color: NexoColors.onSurfaceVariant)),
          ),
          Expanded(
            child: SelectableText(v,
                style: const TextStyle(
                    fontFamily: kFontBody, fontSize: 18)),
          ),
        ],
      ),
    );
  }
}
