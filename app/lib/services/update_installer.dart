// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:io';

import 'package:flutter/services.dart';

/// Bridges to the native self-update helpers:
///  * Android — hands a downloaded APK to the system package installer.
///  * macOS  — swaps the running `.app` bundle for a downloaded one and relaunches.
///
/// All calls are best-effort and degrade gracefully on unsupported platforms.
class UpdateInstaller {
  static const MethodChannel _channel =
      MethodChannel('com.nexo.nexo_app/updater');

  /// Android only: whether the app is currently allowed to install packages.
  static Future<bool> canInstallPackages() async {
    if (!Platform.isAndroid) return false;
    try {
      return await _channel.invokeMethod<bool>('canInstall') ?? false;
    } on PlatformException {
      return false;
    } on MissingPluginException {
      return false;
    }
  }

  /// Android only: opens the "install unknown apps" settings for Nexo.
  static Future<void> openInstallSettings() async {
    if (!Platform.isAndroid) return;
    try {
      await _channel.invokeMethod('openInstallSettings');
    } on PlatformException {
      // ignore
    } on MissingPluginException {
      // ignore
    }
  }

  /// Android only: launches the package installer for the APK at [path].
  static Future<bool> installApk(String path) async {
    if (!Platform.isAndroid) return false;
    try {
      return await _channel.invokeMethod<bool>('installApk', {'path': path}) ??
          false;
    } on PlatformException {
      return false;
    } on MissingPluginException {
      return false;
    }
  }

  /// macOS only: replaces the running bundle with the archive at [zipPath] and
  /// relaunches. Returns false if the swap could not be started (the caller can
  /// then fall back to revealing the download).
  static Future<bool> applyMacUpdate(String zipPath) async {
    if (!Platform.isMacOS) return false;
    try {
      return await _channel
              .invokeMethod<bool>('applyUpdate', {'path': zipPath}) ??
          false;
    } on PlatformException {
      return false;
    } on MissingPluginException {
      return false;
    }
  }

  /// macOS: reveals a file in Finder.
  static Future<void> reveal(String path) async {
    if (!Platform.isMacOS) return;
    try {
      await _channel.invokeMethod('reveal', {'path': path});
    } on PlatformException {
      // ignore
    } on MissingPluginException {
      // ignore
    }
  }
}
