// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:io';

import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';
import 'package:provider/provider.dart';

import '../ble/nexo_ble_client.dart';
import '../services/updater.dart';
import '../services/wifi_joiner.dart';
import '../services/wifi_transfer.dart';
import '../state/app_state.dart';
import '../theme.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';

/// Installs firmware onto the Cardputer over Wi-Fi. The image is either pulled
/// straight from a GitHub release (pick a version) or chosen from a local file,
/// then uploaded to the device's `/api/ota` endpoint. The device reboots when
/// the image is accepted, so a dropped connection at the end is normal.
///
/// Note: the device access point has no internet, so a GitHub download always
/// happens *before* the phone joins the AP.
class FirmwareUpdateScreen extends StatefulWidget {
  const FirmwareUpdateScreen({super.key});

  @override
  State<FirmwareUpdateScreen> createState() => _FirmwareUpdateScreenState();
}

enum _Source { release, file }

enum _OtaPhase {
  idle,
  loadingReleases,
  downloading,
  enablingAp,
  joining,
  ready,
  uploading,
  done,
  error,
}

class _FirmwareUpdateScreenState extends State<FirmwareUpdateScreen> {
  _Source _source = _Source.release;
  _OtaPhase _phase = _OtaPhase.loadingReleases;
  String? _message;
  bool _autoJoin = false;

  List<AppRelease>? _releases;
  String? _selectedTag;

  String _ssid = '';
  String _password = '';
  String _ip = '192.168.4.1';
  bool _apWasEnabled = false;
  bool _restoredAp = false;

  XFile? _file;
  double _progress = 0;

  AppState? _app;
  WifiTransfer? _transfer;
  final Updater _updater = Updater();

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      _app = context.read<AppState>();
      _autoJoin = await WifiJoiner.supported();
      await _loadReleases();
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
    _updater.close();
    super.dispose();
  }

  AppRelease? get _selected {
    final list = _releases;
    if (list == null || list.isEmpty) return null;
    for (final r in list) {
      if (r.tag == _selectedTag) return r;
    }
    return list.first;
  }

  Future<void> _loadReleases() async {
    setState(() {
      _phase = _OtaPhase.loadingReleases;
      _message = null;
    });
    try {
      final all = await _updater.fetchReleases();
      final withFirmware = all.where((r) => r.firmwareAsset != null).toList();
      if (!mounted) return;
      setState(() {
        _releases = withFirmware;
        _selectedTag = withFirmware.isEmpty ? null : withFirmware.first.tag;
        _phase = _OtaPhase.idle;
        if (withFirmware.isEmpty) {
          _source = _Source.file;
          _message = 'No published release has a firmware image yet. '
              'Choose a local .bin instead.';
        }
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _source = _Source.file;
        _releases = const [];
        _phase = _OtaPhase.idle;
        _message = 'Could not reach GitHub ($e). Choose a local .bin instead.';
      });
    }
  }

  Future<void> _pick() async {
    const group = XTypeGroup(label: 'Firmware', extensions: ['bin']);
    final f = await openFile(acceptedTypeGroups: const [group]);
    if (f == null) return;
    setState(() => _file = f);
  }

  /// Brings the device AP up over BLE and joins it. Sets [_ssid]/[_password]/
  /// [_ip]; leaves the phase at `ready`.
  Future<void> _enableApAndJoin() async {
    final app = context.read<AppState>();
    setState(() {
      _phase = _OtaPhase.enablingAp;
      _message = 'Switching on the device access point…';
    });
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
      _message = 'Connected. Ready to install.';
    });
  }

  Future<void> _install() async {
    final app = _app!;
    File? src;
    if (_source == _Source.release) {
      final rel = _selected;
      final asset = rel?.firmwareAsset;
      if (rel == null || asset == null) {
        setState(() {
          _phase = _OtaPhase.error;
          _message = 'No firmware release selected.';
        });
        return;
      }
    } else {
      if (_file == null) {
        await _pick();
        if (_file == null) return;
      }
    }

    try {
      if (_source == _Source.release) {
        final rel = _selected!;
        final asset = rel.firmwareAsset!;
        setState(() {
          _phase = _OtaPhase.downloading;
          _progress = 0;
          _message = 'Downloading ${asset.name} (v${rel.version}) from GitHub…';
        });
        final dir = await getTemporaryDirectory();
        src = File('${dir.path}/nexo-firmware-${rel.version}.bin');
        await _updater.download(
          asset,
          src,
          onProgress: (p) => setState(() => _progress = (p * 0.4).clamp(0.0, 0.4)),
        );
      } else {
        src = File(_file!.path);
      }

      final length = await src.length();

      await _enableApAndJoin();

      setState(() {
        _phase = _OtaPhase.uploading;
        _progress = 0.4;
        _message = 'Uploading firmware… do not disconnect.';
      });
      final transfer = _transfer ??= WifiTransfer(baseUrl: 'http://$_ip');
      final pin = await app.savedPin();
      await transfer.login(pin);
      await transfer.uploadFirmware(
        bytes: src.openRead(),
        length: length,
        onProgress: (sent, total) {
          final frac = total == 0 ? 0.0 : (sent / total).clamp(0.0, 1.0);
          setState(() => _progress = (0.4 + 0.6 * frac).clamp(0.0, 1.0));
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

  String _sizeLabel(int bytes) {
    if (bytes <= 0) return '';
    final mb = bytes / (1024 * 1024);
    return '${mb.toStringAsFixed(1)} MB';
  }

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final busy = _phase == _OtaPhase.loadingReleases ||
        _phase == _OtaPhase.downloading ||
        _phase == _OtaPhase.enablingAp ||
        _phase == _OtaPhase.joining ||
        _phase == _OtaPhase.uploading;
    final deviceVersion = _app?.details?.firmwareVersion ?? '';
    final release = _selected;

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
                  'The image is sent over Wi-Fi to the inactive partition; if it '
                  'is not accepted the device keeps running the current firmware.',
                  style: TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    color: scheme.onSurfaceVariant,
                  ),
                ),
                if (deviceVersion.isNotEmpty) ...[
                  const SizedBox(height: 8),
                  Text('Device is running firmware v$deviceVersion.',
                      style: TextStyle(
                          fontFamily: kFontBody,
                          fontSize: 18,
                          color: scheme.onSurfaceVariant)),
                ],
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

          // Source picker.
          if (!busy && _phase != _OtaPhase.done) ...[
            SegmentedButton<_Source>(
              segments: const [
                ButtonSegment(
                  value: _Source.release,
                  icon: Icon(Icons.cloud_download),
                  label: Text('GitHub'),
                ),
                ButtonSegment(
                  value: _Source.file,
                  icon: Icon(Icons.folder_open),
                  label: Text('File'),
                ),
              ],
              selected: {_source},
              onSelectionChanged: (s) => setState(() => _source = s.first),
            ),
            const SizedBox(height: 16),
          ],

          // GitHub release picker.
          if (_source == _Source.release &&
              !busy &&
              _phase != _OtaPhase.done) ...[
            PixelPanel(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const PixelLabel('Release'),
                  const SizedBox(height: 8),
                  if (_phase == _OtaPhase.loadingReleases)
                    const Center(child: CircularProgressIndicator())
                  else if ((_releases ?? const []).isEmpty)
                    Text('No releases available.',
                        style: TextStyle(
                            fontFamily: kFontBody,
                            fontSize: 18,
                            color: scheme.onSurfaceVariant))
                  else
                    DropdownButtonFormField<String>(
                      initialValue: _selectedTag,
                      isExpanded: true,
                      items: [
                        for (final r in _releases!)
                          DropdownMenuItem(
                            value: r.tag,
                            child: Text(
                              r.tag,
                              style: const TextStyle(
                                  fontFamily: kFontBody, fontSize: 18),
                            ),
                          ),
                      ],
                      onChanged: (v) => setState(() => _selectedTag = v),
                    ),
                  if (release != null && release.firmwareAsset != null) ...[
                    const SizedBox(height: 8),
                    Text(
                      '${release.firmwareAsset!.name}'
                      '${release.firmwareAsset!.size > 0 ? ' · ${_sizeLabel(release.firmwareAsset!.size)}' : ''}',
                      style: TextStyle(
                          fontFamily: kFontBody,
                          fontSize: 18,
                          color: scheme.onSurfaceVariant),
                    ),
                  ],
                  const SizedBox(height: 8),
                  Text(
                    'Downloaded from GitHub before joining the device Wi-Fi, '
                    'which has no internet.',
                    style: TextStyle(
                        fontFamily: kFontBody,
                        fontSize: 16,
                        color: scheme.onSurfaceVariant),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),
          ],

          // Local file picker.
          if (_source == _Source.file &&
              !busy &&
              _phase != _OtaPhase.done) ...[
            OutlinedButton.icon(
              onPressed: _pick,
              icon: const Icon(Icons.folder_open),
              label: Text(_file?.name ?? 'Choose .bin'),
            ),
            const SizedBox(height: 16),
          ],

          // Begin / retry.
          if (_phase == _OtaPhase.idle || _phase == _OtaPhase.error)
            FilledButton.icon(
              onPressed: _install,
              icon: const Icon(Icons.system_update_alt),
              label: Text(_source == _Source.release
                  ? 'Download & install'
                  : 'Install'),
            ),

          if (_phase == _OtaPhase.enablingAp || _phase == _OtaPhase.joining)
            const Center(child: CircularProgressIndicator()),

          if (_phase == _OtaPhase.joining && !_autoJoin)
            const SizedBox(height: 8),

          if (busy &&
              (_phase == _OtaPhase.downloading ||
                  _phase == _OtaPhase.uploading)) ...[
            const SizedBox(height: 16),
            LinearProgressIndicator(value: _progress, minHeight: 8),
            const SizedBox(height: 8),
            Text('${(_progress * 100).toStringAsFixed(0)}%',
                style: const TextStyle(fontFamily: kFontBody, fontSize: 18)),
          ],

          if (_phase == _OtaPhase.done) ...[
            const SizedBox(height: 24),
            FilledButton.icon(
              onPressed: _finish,
              icon: const Icon(Icons.check),
              label: const Text('Done'),
            ),
          ],

          if (!busy && _phase == _OtaPhase.idle && _source == _Source.release)
            Padding(
              padding: const EdgeInsets.only(top: 12),
              child: TextButton.icon(
                onPressed: _loadReleases,
                icon: const Icon(Icons.refresh),
                label: const Text('Refresh release list'),
              ),
            ),
        ],
      ),
    );
  }
}
