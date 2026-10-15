// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:io';

import 'package:flutter/services.dart';

/// Joins the Cardputer's Wi-Fi access point so bulk music transfer can run over
/// HTTP. Android uses [WifiNetworkSpecifier] and binds the process to that
/// network; other platforms return unsupported and the UI falls back to manual
/// joining (SSID / password shown to the user).
class WifiJoiner {
  static const MethodChannel _channel = MethodChannel('com.nexo.nexo_app/wifi');

  /// Whether automatic joining is available on this platform.
  static Future<bool> supported() async {
    if (!Platform.isAndroid) return false;
    try {
      final ok = await _channel.invokeMethod<bool>('supported');
      return ok ?? false;
    } on PlatformException {
      return false;
    } on MissingPluginException {
      return false;
    }
  }

  /// Attempts to join [ssid]. Returns true on success. Throws on explicit
  /// failure (wrong password / unavailable).
  static Future<bool> join({required String ssid, required String password}) async {
    if (!Platform.isAndroid) return false;
    try {
      final ok = await _channel.invokeMethod<bool>('join', {
        'ssid': ssid,
        'password': password,
      });
      return ok ?? false;
    } on MissingPluginException {
      return false;
    }
  }

  static Future<void> leave() async {
    if (!Platform.isAndroid) return;
    try {
      await _channel.invokeMethod('leave');
    } catch (_) {
      // best-effort
    }
  }
}
