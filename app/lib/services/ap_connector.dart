// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';
import 'dart:io';

import 'package:url_launcher/url_launcher.dart';

import '../state/app_state.dart';
import 'wifi_joiner.dart';
import 'wifi_transfer.dart';

class ApCredentials {
  ApCredentials({
    required this.ssid,
    required this.password,
    required this.ip,
    required this.apWasEnabled,
  });

  final String ssid;
  final String password;
  final String ip;
  final bool apWasEnabled;
}

enum ApJoinPhase { enablingAp, joining, verifying, ready, error }

class ApConnector {
  ApConnector();

  final StreamController<ApJoinPhase> _phase = StreamController.broadcast();
  Stream<ApJoinPhase> get phase => _phase.stream;

  String? _lastMessage;
  String? get lastMessage => _lastMessage;

  Future<ApCredentials> prepare(AppState app, {required bool preferAutoJoin}) async {
    _emit(ApJoinPhase.enablingAp, 'Switching on the device access point…');
    final apWasEnabled = app.settings.apEnabled;
    await app.saveSettings(app.settings.copyWith(apEnabled: true, pin: null));
    await Future.delayed(const Duration(milliseconds: 800));
    await app.refreshDeviceDetails();
    final d = app.details;
    if (d == null) {
      throw Exception('Device did not report Wi-Fi details. Try again.');
    }
    final ssid = d.apSsid.isNotEmpty ? d.apSsid : 'Nexo-AP';
    final password = d.apPassword;
    final ip = d.ip.isNotEmpty && d.ip != '-' ? d.ip : '192.168.4.1';

    final autoJoin = preferAutoJoin && await WifiJoiner.supported();
    _emit(
      ApJoinPhase.joining,
      autoJoin
          ? 'Joining $ssid…'
          : 'Connect this device to Wi-Fi "$ssid" (password "$password"). '
              'No internet on this network — that is normal. Tap Continue when connected.',
    );

    if (autoJoin) {
      final ok = await WifiJoiner.join(ssid: ssid, password: password);
      if (!ok) {
        throw Exception('Could not join "$ssid" automatically. Connect manually and try again.');
      }
      await Future.delayed(const Duration(milliseconds: 700));
    }

    _emit(ApJoinPhase.verifying, 'Checking connection to the device…');
    final transfer = WifiTransfer(baseUrl: 'http://$ip');
    try {
      final pin = await app.savedPin();
      // Quick reachability + auth check with a short timeout.
      await transfer.login(pin).timeout(
            const Duration(seconds: 10),
            onTimeout: () => throw HttpException(
                'Timed out reaching $ip. Make sure you are connected to "$ssid" '
                'before continuing.'),
          );
    } catch (e) {
      transfer.close();
      if (e is HttpException) rethrow;
      throw Exception('Could not reach $ip. Connect to "$ssid" first.');
    }

    _emit(ApJoinPhase.ready, 'Connected to the device.');
    return ApCredentials(
      ssid: ssid,
      password: password,
      ip: ip,
      apWasEnabled: apWasEnabled,
    );
  }

  void _emit(ApJoinPhase p, String msg) {
    _lastMessage = msg;
    if (!_phase.isClosed) _phase.add(p);
  }

  void dispose() {
    if (!_phase.isClosed) _phase.close();
  }

  static Future<void> openWifiSettings() async {
    final candidates = Platform.isMacOS
        ? const ['x-apple.systempreferences:com.apple.wifi-settings-extension']
        : const ['android.settings.WIFI_SETTINGS'];
    for (final c in candidates) {
      try {
        if (await launchUrl(Uri.parse(c))) return;
      } catch (_) {
        // try next
      }
    }
  }
}
