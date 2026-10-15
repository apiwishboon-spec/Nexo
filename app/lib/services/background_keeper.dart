// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:io';

import 'package:flutter/services.dart';

/// Keeps the Android app alive (via a foreground service) so the live BLE link
/// to the Cardputer survives while Nexo is backgrounded. On the Cardputer there
/// is no RTC, so there is no fixed sync clock time: instead we reconcile the
/// moment we (re)connect, and the background service keeps that connection up.
///
/// No-op on platforms without a background-service concept (macOS/iOS here).
class BackgroundKeeper {
  static const MethodChannel _channel =
      MethodChannel('com.nexo.nexo_app/background');

  static bool _running = false;

  static Future<void> start() async {
    if (!Platform.isAndroid || _running) return;
    try {
      await _channel.invokeMethod('start');
      _running = true;
    } on PlatformException {
      _running = false;
    } on MissingPluginException {
      _running = false;
    }
  }

  static Future<void> stop() async {
    if (!Platform.isAndroid || !_running) return;
    try {
      await _channel.invokeMethod('stop');
    } on PlatformException {
      // ignore
    } on MissingPluginException {
      // ignore
    }
    _running = false;
  }
}
