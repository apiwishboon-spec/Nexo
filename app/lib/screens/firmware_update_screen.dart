// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
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

/// Uploads a new firmware image to the device over Wi-Fi. Uses the same AP
/// switching flow as music import. The device reboots when the image is
/// accepted, so a dropped connection at the end is normal.
class FirmwareUpdateScreen extends StatefulWidget {
  const FirmwareUpdateScreen({super.key});

  @override
  State<FirmwareUpdateScreen> createState() => _FirmwareUpdateScreenState();
}

enum _OtaPhase { idle, enablingAp, joining, ready, uploading, done, error }

class _FirmwareUpdateScreenState extends State<FirmwareUpdateScreen> {
  _OtaPhase _phase = _OtaPhase.idle;
  String? _message;
  bool _autoJoin = false;

  String _ssid = '';
  String _password = '';
  String _ip = '192.168.4.1';
  bool _apWasEnabled = false;
  bool _restoredAp = false;

  XFile? _file;
  double _progress = 0;

  AppState? _app;
  WifiTransfer? _transfer;

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
    final app = _app;
    if (!_restoredAp &&
        !_apWasEnabled &&
        app != null &&
        app.connState == NexoConnState.ready) {
      app.saveSettings(app.settings.copyWith(apEnabled: false, pin: null));
    }
    WifiJoiner.leave();
    _transfer?.close();
    super.dispose();
  }

  Future<void> _begin() async {
    final app = context.read<AppState>();
    setState(() {
      _phase = _OtaPhase.enablingAp;
      _message = 'Switching on the device access point…';
    });
    try {
      _apWasEnabled = app.settings.apEnabled;
      await app.saveSettings(app.settings.copyWith(apEnabled: true, pin: null));
      await Future.delayed(const Duration(milliseconds: 800));
      await app.refreshDeviceDetails();
      final d = app.details;
      if (d == null) throw Exception('Device did not report Wi-Fi details.');
      _ssid = d.apSsid.isNotEmpty ? d.apSsid : 'Nexo-AP';
      _password = d.apPassword;
      _ip = d.ip.isNotEmpty && d.ip != '-' ? d.ip : '192.168.4.1';

      setState(() {
        _phase = _OtaPhase.joining;
        _message = _autoJoin
            ? 'Joining $_ssid…'
            : 'Connect your phone to Wi-Fi "$_ssid" (password "$_password"), then tap Continue.';
      });
      if (_autoJoin) {
        final ok = await WifiJoiner.join(ssid: _ssid, password: _password);
        if (!ok) {
          throw Exception('Could not join "$_ssid". Connect manually and continue.');
        }
        await Future.delayed(const Duration(milliseconds: 600));
      }
      setState(() {
        _phase = _OtaPhase.ready;
        _message = 'Connected. Choose a firmware .bin to install.';
      });
    } catch (e) {
      setState(() {
        _phase = _OtaPhase.error;
        _message = e.toString();
      });
    }
  }

  Future<void> _pick() async {
    const group = XTypeGroup(label: 'Firmware', extensions: ['bin']);
    final f = await openFile(acceptedTypeGroups: const [group]);
    if (f == null) return;
    setState(() => _file = f);
  }

  Future<void> _install() async {
    var file = _file;
    if (file == null) {
      await _pick();
      file = _file;
      if (file == null) return;
    }
    setState(() {
      _phase = _OtaPhase.uploading;
      _progress = 0;
      _message = 'Uploading firmware… do not disconnect.';
    });
    final transfer = _transfer ??= WifiTransfer(baseUrl: 'http://$_ip');
    try {
      final pin = await _app!.savedPin();
      await transfer.login(pin);
      final f = File(file.path);
      final length = await f.length();
      await transfer.uploadFirmware(
        bytes: f.openRead(),
        length: length,
        onProgress: (sent, total) {
          setState(() => _progress = length == 0 ? 0 : (sent / total).clamp(0.0, 1.0));
        },
      );
      setState(() {
        _phase = _OtaPhase.done;
        _message = 'Update sent. The Cardputer is rebooting.';
      });
    } catch (e) {
      // A dropped connection right at the end usually means the device rebooted.
      setState(() {
        _phase = _OtaPhase.done;
        _message =
            'Transfer ended ($e). If the device reboots, the update likely '
            'succeeded — reconnect to verify the version.';
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
        title: const Text('Firmware update'),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          PixelPanel(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const PixelLabel('Over-the-air update'),
                const SizedBox(height: 8),
                Text(
                  'Make sure the device is powered and charged. The image is sent '
                  'over Wi-Fi to the inactive partition; if it is not accepted the '
                  'device keeps running the current firmware.',
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
          if (_message != null)
            PixelPanel(
              accent: _phase == _OtaPhase.error ? NexoColors.error : null,
              child: Text(_message!,
                  style: const TextStyle(fontFamily: kFontBody, fontSize: 18)),
            ),
          const SizedBox(height: 16),
          if (_phase == _OtaPhase.idle)
            FilledButton.icon(
              onPressed: _begin,
              icon: const Icon(Icons.wifi),
              label: const Text('Start'),
            ),
          if (_phase == _OtaPhase.enablingAp || _phase == _OtaPhase.joining)
            const Center(child: CircularProgressIndicator()),
          if (_phase == _OtaPhase.ready || _phase == _OtaPhase.uploading ||
              _phase == _OtaPhase.done) ...[
            PixelPanel(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const PixelLabel('Network'),
                  const SizedBox(height: 6),
                  Text('SSID: $_ssid   •   IP: $_ip',
                      style: const TextStyle(
                          fontFamily: kFontBody, fontSize: 18)),
                  if (!_autoJoin)
                    Text('Password: $_password',
                        style: const TextStyle(
                            fontFamily: kFontBody, fontSize: 18)),
                ],
              ),
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                OutlinedButton.icon(
                  onPressed: _phase == _OtaPhase.uploading ? null : _pick,
                  icon: const Icon(Icons.folder_open),
                  label: Text(_file?.name ?? 'Choose .bin'),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: FilledButton.icon(
                    onPressed: _phase == _OtaPhase.uploading ? null : _install,
                    icon: const Icon(Icons.system_update_alt),
                    label: const Text('Install'),
                  ),
                ),
              ],
            ),
            if (_phase == _OtaPhase.uploading) ...[
              const SizedBox(height: 16),
              LinearProgressIndicator(value: _progress, minHeight: 8),
              const SizedBox(height: 8),
              Text('${(_progress * 100).toStringAsFixed(0)}%',
                  style: const TextStyle(
                      fontFamily: kFontBody, fontSize: 18)),
            ],
            if (_phase == _OtaPhase.done) ...[
              const SizedBox(height: 24),
              FilledButton.icon(
                onPressed: _finish,
                icon: const Icon(Icons.check),
                label: const Text('Done'),
              ),
            ],
          ],
          if (_phase == _OtaPhase.error) ...[
            OutlinedButton.icon(
              onPressed: () => setState(() {
                _phase = _OtaPhase.idle;
                _message = null;
              }),
              icon: const Icon(Icons.refresh),
              label: const Text('Try again'),
            ),
          ],
        ],
      ),
    );
  }
}
