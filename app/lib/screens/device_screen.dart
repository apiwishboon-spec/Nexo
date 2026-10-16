// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:provider/provider.dart';

import '../ble/nexo_ble_client.dart';
import '../state/app_state.dart';
import '../theme.dart';
import '../widgets/pixel_pet.dart';
import '../widgets/pixel_ui.dart';

class DeviceScreen extends StatefulWidget {
  const DeviceScreen({super.key});

  @override
  State<DeviceScreen> createState() => _DeviceScreenState();
}

class _DeviceScreenState extends State<DeviceScreen> {
  StreamSubscription? _scanSub;
  final Map<String, ScanResult> _found = {};
  bool _scanning = false;

  @override
  void initState() {
    super.initState();
    _scanSub = context.read<AppState>().scanResults().listen((results) {
      setState(() {
        for (final r in results) {
          _found[r.device.remoteId.str] = r;
        }
      });
    });
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    super.dispose();
  }

  Future<void> _toggleScan() async {
    final app = context.read<AppState>();
    if (_scanning) {
      await app.stopScan();
      setState(() => _scanning = false);
    } else {
      _found.clear();
      setState(() => _scanning = true);
      await app.startScan();
      Future.delayed(const Duration(seconds: 15), () {
        if (mounted) setState(() => _scanning = false);
      });
    }
  }

  Future<void> _connect(BluetoothDevice device) async {
    final app = context.read<AppState>();
    final savedPin = await app.savedPin();
    if (!mounted) return;
    final pinCtrl = TextEditingController(text: savedPin);
    final pin = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Device PIN'),
        content: TextField(
          controller: pinCtrl,
          autofocus: true,
          obscureText: true,
          keyboardType: TextInputType.number,
          decoration: const InputDecoration(labelText: 'PIN'),
          onSubmitted: (v) => Navigator.pop(ctx, v),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, pinCtrl.text),
            child: const Text('Connect'),
          ),
        ],
      ),
    );
    if (pin == null) return;
    await app.stopScan();
    if (mounted) setState(() => _scanning = false);
    await app.connectAndAuthenticate(device, pin);
  }

  @override
  Widget build(BuildContext context) {
    final app = context.watch<AppState>();
    final scheme = Theme.of(context).colorScheme;

    return Scaffold(
      backgroundColor: Colors.transparent,
      appBar: AppBar(
        leading: const Padding(
          padding: EdgeInsets.only(left: 12),
          child: PixelPet(size: 32),
        ),
        leadingWidth: 52,
        title: const Text('Device'),
        actions: [
          if (app.connState != NexoConnState.disconnected)
            IconButton(
              tooltip: 'Disconnect',
              icon: const Icon(Icons.link_off),
              onPressed: () => app.disconnect(),
            ),
          if (app.connState == NexoConnState.disconnected && app.canReconnect)
            IconButton(
              tooltip: 'Forget device',
              icon: const Icon(Icons.delete_outline),
              onPressed: () => app.forgetDevice(),
            ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _connectionCard(app, scheme),
          const SizedBox(height: 16),
          if (app.connState == NexoConnState.disconnected) ...[
            if (app.canReconnect)
              Card(
                margin: const EdgeInsets.only(bottom: 12),
                child: ListTile(
                  leading:
                      Icon(Icons.bluetooth_connected, color: scheme.primary),
                  title: Text(
                      'Reconnect to ${app.lastDeviceName ?? "last device"}'),
                  subtitle: const Text('Saved device · no PIN needed'),
                  trailing: app.busy
                      ? const SizedBox(
                          width: 18,
                          height: 18,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.refresh),
                  onTap: app.busy ? null : () => app.reconnect(),
                ),
              ),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: _toggleScan,
                  icon: Icon(_scanning ? Icons.stop : Icons.search),
                  label: Text(_scanning ? 'Stop' : 'Scan for Nexo devices'),
                ),
                const SizedBox(width: 12),
                if (_scanning)
                  const SizedBox(
                    width: 18,
                    height: 18,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
              ],
            ),
            const SizedBox(height: 12),
            ..._found.values.map((r) => _deviceTile(r, scheme)),
            if (_found.isEmpty && !_scanning)
              Padding(
                padding: const EdgeInsets.only(top: 24),
                child: Text(
                  'Turn on your Cardputer, launch the Nexo app, then tap Scan.',
                  style: TextStyle(color: scheme.onSurfaceVariant),
                ),
              ),
          ],
          if (app.connState != NexoConnState.disconnected) ...[
            _detailsCard(app, scheme),
            const SizedBox(height: 16),
            _syncCard(app, scheme),
            const SizedBox(height: 16),
            _logsCard(app, scheme),
          ],
        ],
      ),
    );
  }

  Widget _connectionCard(AppState app, ColorScheme scheme) {
    final state = app.connState;
    final (label, color, icon) = switch (state) {
      NexoConnState.disconnected => ('Disconnected', scheme.outline, Icons.link_off),
      NexoConnState.connecting => ('Connecting…', scheme.tertiary, Icons.sync),
      NexoConnState.connected => ('Connected', scheme.primary, Icons.link),
      NexoConnState.authenticating => ('Authenticating…', scheme.tertiary, Icons.lock_open),
      NexoConnState.ready => ('Ready', scheme.primary, Icons.verified),
    };
    final mood = switch (state) {
      NexoConnState.ready => PetMood.happy,
      NexoConnState.connected => app.busy ? PetMood.busy : PetMood.idle,
      NexoConnState.connecting ||
      NexoConnState.authenticating =>
        PetMood.busy,
      NexoConnState.disconnected => PetMood.sleepy,
    };
    return PixelPanel(
      child: Row(
        children: [
          PixelPet(size: 64, mood: mood),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                PixelTag(label, color: color),
                const SizedBox(height: 10),
                Text(
                  app.connectedId ?? 'No device connected',
                  style: const TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    height: 1.1,
                    color: NexoColors.onSurfaceVariant,
                  ),
                ),
                if (app.hasLocalChanges)
                  Padding(
                    padding: const EdgeInsets.only(top: 4),
                    child: Text(
                      '${app.pendingCount} local change(s) pending sync',
                      style: TextStyle(
                        fontFamily: kFontBody,
                        fontSize: 18,
                        color: scheme.primary,
                      ),
                    ),
                  ),
              ],
            ),
          ),
          Icon(icon, color: color, size: 22),
        ],
      ),
    );
  }

  Widget _detailsCard(AppState app, ColorScheme scheme) {
    final d = app.details;
    if (d == null) {
      return Card(
        child: ListTile(
          leading: Icon(Icons.memory, color: scheme.primary),
          title: const Text('Device details'),
          subtitle: const Text('Tap to load'),
          trailing: const Icon(Icons.refresh),
          onTap: () => app.refreshDeviceDetails(),
        ),
      );
    }
    final battery = d.batteryKnown ? '${d.batteryPercent}%' : 'unknown';
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Text('Device details',
                    style: Theme.of(context).textTheme.titleMedium),
                const Spacer(),
                IconButton(
                  icon: const Icon(Icons.refresh, size: 18),
                  onPressed: () => app.refreshDeviceDetails(),
                ),
              ],
            ),
            const SizedBox(height: 4),
            _detailRow('Battery', battery, scheme,
                icon: d.charging ? Icons.battery_charging_full : Icons.battery_full),
            _detailRow('Firmware', d.firmwareVersion, scheme),
            _detailRow('Storage',
                d.sdPresent
                    ? '${d.sdFreeMb} MB free / ${d.sdTotalMb} MB'
                    : 'No SD card',
                scheme,
                icon: Icons.sd_card),
            _detailRow('Free heap', '${d.freeHeapKiB} KiB', scheme),
            _detailRow('Uptime', _fmtUptime(d.uptimeSec), scheme),
            _detailRow('IP', d.ip.isEmpty ? '-' : d.ip, scheme,
                icon: Icons.wifi),
            _detailRow('MAC', d.mac, scheme),
            _detailRow('Headphones', d.headphoneIn ? 'plugged in' : 'speaker',
                scheme,
                icon: d.headphoneIn ? Icons.headphones : Icons.speaker),
          ],
        ),
      ),
    );
  }

  String _fmtUptime(int sec) {
    final h = sec ~/ 3600;
    final m = (sec % 3600) ~/ 60;
    final s = sec % 60;
    if (h > 0) return '${h}h ${m}m';
    if (m > 0) return '${m}m ${s}s';
    return '${s}s';
  }

  Widget _detailRow(String label, String value, ColorScheme scheme,
      {IconData? icon}) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        children: [
          if (icon != null) ...[
            Icon(icon, size: 16, color: scheme.onSurfaceVariant),
            const SizedBox(width: 8),
          ],
          SizedBox(
            width: 110,
            child: Text(label,
                style: TextStyle(
                    fontFamily: kFontBody,
                    fontSize: 18,
                    color: scheme.onSurfaceVariant)),
          ),
          Expanded(
            child: Text(value,
                style: const TextStyle(fontFamily: kFontBody, fontSize: 18)),
          ),
        ],
      ),
    );
  }

  Widget _syncCard(AppState app, ColorScheme scheme) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Synchronization',
                style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 8),
            Text(
              app.hasLocalChanges
                  ? '${app.pendingCount} change(s) waiting to push.'
                  : 'No pending changes.',
              style: TextStyle(color: scheme.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: app.busy || !app.connected ? null : () => app.sync(),
                  icon: app.busy
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.sync),
                  label: const Text('Sync now'),
                ),
                const SizedBox(width: 12),
                OutlinedButton.icon(
                  onPressed: app.busy ? null : () => app.refresh(),
                  icon: const Icon(Icons.download),
                  label: const Text('Reload'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _logsCard(AppState app, ColorScheme scheme) {
    final logs = app.logs.reversed.take(12).toList();
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Activity', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 8),
            if (logs.isEmpty)
              Text('No activity yet.',
                  style: TextStyle(color: scheme.onSurfaceVariant))
            else
              ...logs.map(
                (l) => Padding(
                  padding: const EdgeInsets.symmetric(vertical: 2),
                  child: Text(
                    l,
                    style: TextStyle(
                      fontFamily: 'monospace',
                      fontSize: 12,
                      color: scheme.onSurfaceVariant,
                    ),
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }

  Widget _deviceTile(ScanResult r, ColorScheme scheme) {
    final name = r.advertisementData.advName.isEmpty
        ? (r.device.platformName.isEmpty ? 'Nexo device' : r.device.platformName)
        : r.advertisementData.advName;
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: scheme.primaryContainer,
          child: Icon(Icons.memory, color: scheme.onPrimaryContainer),
        ),
        title: Text(name),
        subtitle: Text('${r.device.remoteId.str}  •  ${r.rssi} dBm'),
        trailing: const Icon(Icons.chevron_right),
        onTap: () => _connect(r.device),
      ),
    );
  }
}
