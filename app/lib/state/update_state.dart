// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../services/update_installer.dart';
import '../services/updater.dart';

/// Owns the "check for app updates" flow: querying GitHub, downloading the
/// platform artifact, and handing it to the native installer.
class UpdateState extends ChangeNotifier {
  UpdateState({Updater? updater}) : _updater = updater ?? Updater();

  final Updater _updater;

  static const _kAutoCheck = 'update_auto_check';
  static const _kLastCheck = 'update_last_check';

  String currentVersion = '';
  String currentBuild = '';

  AppRelease? available;
  bool checking = false;
  bool downloading = false;
  double progress = 0;
  String? error;
  String? status;

  bool autoCheck = true;
  DateTime? lastChecked;

  /// Set once per session after we've offered an update, to avoid nagging.
  bool prompted = false;

  bool get hasUpdate => available != null;
  bool get supported =>
      Platform.isAndroid || Platform.isMacOS;

  Future<void> init() async {
    try {
      final info = await PackageInfo.fromPlatform();
      currentVersion = info.version;
      currentBuild = info.buildNumber;
    } catch (_) {
      currentVersion = '0.0.0';
    }
    final prefs = await SharedPreferences.getInstance();
    autoCheck = prefs.getBool(_kAutoCheck) ?? true;
    final ms = prefs.getInt(_kLastCheck);
    if (ms != null) {
      lastChecked = DateTime.fromMillisecondsSinceEpoch(ms);
    }
    notifyListeners();
    if (autoCheck) {
      // Fire-and-forget silent check shortly after launch.
      await check(silent: true);
    }
  }

  Future<void> setAutoCheck(bool value) async {
    autoCheck = value;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool(_kAutoCheck, value);
    notifyListeners();
  }

  Future<void> check({bool silent = false}) async {
    if (checking) return;
    checking = true;
    if (!silent) {
      error = null;
      status = 'Checking for updates…';
    }
    notifyListeners();
    try {
      available = await _updater.checkForUpdate(currentVersion);
      lastChecked = DateTime.now();
      final prefs = await SharedPreferences.getInstance();
      await prefs.setInt(_kLastCheck, lastChecked!.millisecondsSinceEpoch);
      if (!silent || available != null) {
        status = available == null
            ? 'You are on the latest version ($currentVersion).'
            : 'Version ${available!.version} is available.';
      }
    } catch (e) {
      if (!silent) error = 'Update check failed: $e';
    } finally {
      checking = false;
      notifyListeners();
    }
  }

  /// Downloads the platform artifact and launches the native installer.
  Future<void> downloadAndInstall() async {
    final release = available;
    if (release == null) return;
    final asset = release.platformAsset;
    if (asset == null) {
      error = 'No update is available for this platform.';
      notifyListeners();
      return;
    }

    downloading = true;
    progress = 0;
    error = null;
    status = 'Downloading ${asset.name}…';
    notifyListeners();

    File? file;
    try {
      final dir = await getTemporaryDirectory();
      file = File('${dir.path}/${asset.name}');
      if (await file.exists()) await file.delete();
      await _updater.download(
        asset,
        file,
        onProgress: (p) {
          progress = p;
          notifyListeners();
        },
      );

      if (Platform.isAndroid) {
        await _installAndroid(file.path);
      } else if (Platform.isMacOS) {
        await _installMac(file.path);
      } else {
        error = 'Self-update is not supported on this platform.';
      }
    } catch (e) {
      error = 'Update failed: $e';
    } finally {
      downloading = false;
      notifyListeners();
    }
  }

  Future<void> _installAndroid(String path) async {
    status = 'Launching installer…';
    notifyListeners();
    final canInstall = await UpdateInstaller.canInstallPackages();
    if (!canInstall) {
      await UpdateInstaller.openInstallSettings();
      error = 'Allow "Install unknown apps" for Nexo, then tap Update again.';
      return;
    }
    final ok = await UpdateInstaller.installApk(path);
    if (ok) {
      status = 'Follow the on-screen prompt to finish installing.';
    } else {
      error = 'Could not start the installer. Open the APK manually.';
    }
  }

  Future<void> _installMac(String path) async {
    status = 'Preparing to restart…';
    notifyListeners();
    try {
      final ok = await UpdateInstaller.applyMacUpdate(path);
      if (!ok) {
        await UpdateInstaller.reveal(path);
        error = 'Could not update automatically. The download was revealed in '
            'Finder — replace /Applications/Nexo.app with it.';
      }
    } catch (e) {
      await UpdateInstaller.reveal(path);
      final msg = e is PlatformException ? (e.message ?? e.toString()) : e.toString();
      error = '$msg The download was revealed in Finder — '
          'replace /Applications/Nexo.app with it.';
    }
    // On success the app is about to quit and relaunch.
  }

  /// Opens the release page in the browser (used as a manual fallback).
  static String releaseUrl(AppRelease? release) =>
      release?.htmlUrl ?? 'https://github.com/$kUpdateRepo/releases/latest';

  @override
  void dispose() {
    _updater.close();
    super.dispose();
  }
}
