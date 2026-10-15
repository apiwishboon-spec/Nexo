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
import 'firmware_update_screen.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _deviceName = TextEditingController();
  final _apSsid = TextEditingController();
  final _pin = TextEditingController();
  bool _apEnabled = false;
  int _volume = 60;
  bool _soundEnabled = true;
  int _brightness = 100;
  int _screenTimeout = 30;
  bool _autoAdvance = true;
  bool _loaded = false;

  @override
  void dispose() {
    _deviceName.dispose();
    _apSsid.dispose();
    _pin.dispose();
    super.dispose();
  }

  void _loadFrom(AppState app) {
    if (_loaded) return;
    _deviceName.text = app.settings.deviceName;
    _apSsid.text = app.settings.apSsid;
    _apEnabled = app.settings.apEnabled;
    _volume = app.settings.volume;
    _soundEnabled = app.settings.soundEnabled;
    _brightness = app.settings.brightness;
    _screenTimeout = app.settings.screenTimeoutSec;
    _autoAdvance = app.settings.autoAdvanceMusic;
    _loaded = true;
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    _loadFrom(app);
    final scheme = Theme.of(context).colorScheme;
    final canPush = app.connState == NexoConnState.ready;

    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: const Text('Device settings'),
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          if (!canPush)
            PixelPanel(
              padding: EdgeInsets.zero,
              child: ListTile(
                leading: Icon(Icons.info_outline, color: scheme.onSurfaceVariant),
                title: const Text('Not connected'),
                subtitle: const Text(
                  'Settings are saved locally and pushed when you connect.',
                ),
              ),
            ),
          const SizedBox(height: 8),
          PixelPanel(
            padding: EdgeInsets.zero,
            child: SwitchListTile(
              contentPadding: const EdgeInsets.symmetric(horizontal: 14),
              value: app.autoSync,
              title: const Text('Auto-sync changes'),
              subtitle: const Text(
                  'Push and pull when data changes and on every reconnect'),
              onChanged: (v) => app.setAutoSync(v),
            ),
          ),
          const SizedBox(height: 8),
          PixelPanel(
            padding: EdgeInsets.zero,
            child: ListTile(
              leading: Icon(Icons.bluetooth_connected, color: scheme.primary),
              title: const Text('Background link'),
              subtitle: const Text(
                'Nexo keeps the Bluetooth connection alive while running in the '
                'background, so it syncs the moment the Cardputer reconnects '
                '(it has no clock, so there is no fixed sync time).',
              ),
            ),
          ),
          const SizedBox(height: 16),
          const PixelLabel('Device'),
          const SizedBox(height: 8),
          TextField(
            controller: _deviceName,
            maxLength: 32,
            decoration: const InputDecoration(
              labelText: 'Device name',
              counterText: '',
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _apSsid,
            maxLength: 32,
            decoration: const InputDecoration(
              labelText: 'Wi-Fi AP SSID',
              counterText: '',
            ),
          ),
          const SizedBox(height: 12),
          SwitchListTile(
            value: _apEnabled,
            title: const Text('Enable Wi-Fi access point'),
            subtitle: const Text('Serve the web UI for notes and settings'),
            onChanged: (v) => setState(() => _apEnabled = v),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _pin,
            obscureText: true,
            maxLength: 16,
            keyboardType: TextInputType.number,
            decoration: const InputDecoration(
              labelText: 'New device PIN (leave blank to keep)',
              counterText: '',
            ),
          ),
          const SizedBox(height: 20),
          const PixelLabel('Sound & display'),
          const SizedBox(height: 8),
          Row(
            children: [
              const SizedBox(width: 4),
              const Icon(Icons.volume_up, size: 20),
              Expanded(
                child: Slider(
                  value: _volume.toDouble(),
                  max: 100,
                  divisions: 20,
                  label: '$_volume',
                  onChanged: (v) => setState(() => _volume = v.round()),
                ),
              ),
              SizedBox(width: 32, child: Text('$_volume')),
            ],
          ),
          SwitchListTile(
            value: _soundEnabled,
            title: const Text('Sounds'),
            subtitle: const Text('Key clicks, alarms and music'),
            onChanged: (v) => setState(() => _soundEnabled = v),
          ),
          Row(
            children: [
              const SizedBox(width: 4),
              const Icon(Icons.brightness_6, size: 20),
              Expanded(
                child: Slider(
                  value: _brightness.toDouble(),
                  max: 100,
                  divisions: 20,
                  label: '$_brightness%',
                  onChanged: (v) => setState(() => _brightness = v.round()),
                ),
              ),
              SizedBox(width: 44, child: Text('$_brightness%')),
            ],
          ),
          const SizedBox(height: 8),
          DropdownButtonFormField<int>(
            initialValue: _screenTimeout,
            decoration: const InputDecoration(labelText: 'Screen timeout'),
            items: const [
              DropdownMenuItem(value: 0, child: Text('Never')),
              DropdownMenuItem(value: 15, child: Text('15 seconds')),
              DropdownMenuItem(value: 30, child: Text('30 seconds')),
              DropdownMenuItem(value: 60, child: Text('1 minute')),
              DropdownMenuItem(value: 120, child: Text('2 minutes')),
            ],
            onChanged: (v) => setState(() => _screenTimeout = v ?? 30),
          ),
          const SizedBox(height: 8),
          SwitchListTile(
            value: _autoAdvance,
            title: const Text('Auto-advance music'),
            subtitle: const Text('Play the next track when one finishes'),
            onChanged: (v) => setState(() => _autoAdvance = v),
          ),
          const SizedBox(height: 24),
          FilledButton.icon(
            onPressed: app.busy ? null : _save,
            icon: const Icon(Icons.save),
            label: Text(canPush ? 'Save & push to device' : 'Save locally'),
          ),
          if (canPush) ...[
            const SizedBox(height: 16),
            const PixelLabel('Device actions'),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: () => app.findDevice(),
              icon: const Icon(Icons.notifications_active),
              label: const Text('Find my device'),
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: _confirmReboot,
              icon: const Icon(Icons.restart_alt),
              label: const Text('Reboot device'),
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: _confirmFactoryReset,
              icon: const Icon(Icons.warning_amber),
              label: const Text('Factory reset'),
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: () => Navigator.of(context).push(
                MaterialPageRoute(
                    builder: (_) => const FirmwareUpdateScreen()),
              ),
              icon: const Icon(Icons.system_update_alt),
              label: const Text('Update firmware…'),
            ),
          ],
        ],
      ),
    );
  }

  Future<void> _confirmReboot() async {
    final app = context.read<AppState>();
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Reboot device'),
        content: const Text('The Cardputer will restart. Continue?'),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: const Text('Cancel')),
          FilledButton(
              onPressed: () => Navigator.pop(ctx, true),
              child: const Text('Reboot')),
        ],
      ),
    );
    if (ok == true) await app.rebootDevice();
  }

  Future<void> _confirmFactoryReset() async {
    final app = context.read<AppState>();
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Factory reset'),
        content: const Text(
          'This erases all notes, homework and settings on the device and '
          'cannot be undone. Continue?',
        ),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(ctx, false),
              child: const Text('Cancel')),
          FilledButton(
            style: FilledButton.styleFrom(
                backgroundColor: NexoColors.error,
                foregroundColor: NexoColors.onPrimary),
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Erase device'),
          ),
        ],
      ),
    );
    if (ok == true) await app.factoryResetDevice();
  }

  Future<void> _save() async {
    final app = context.read<AppState>();
    final newPin = _pin.text.trim();
    final settings = DeviceSettings(
      deviceName: _deviceName.text.trim(),
      apSsid: _apSsid.text.trim(),
      apEnabled: _apEnabled,
      volume: _volume,
      soundEnabled: _soundEnabled,
      brightness: _brightness,
      screenTimeoutSec: _screenTimeout,
      autoAdvanceMusic: _autoAdvance,
      timezoneOffsetMin: app.settings.timezoneOffsetMin,
      pin: newPin.isEmpty ? null : newPin,
    );
    await app.saveSettings(settings);
    if (mounted) {
      _pin.clear();
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Settings saved')),
      );
    }
  }
}
