// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

import '../models/models.dart';

/// Offline cache of the last known device state, plus dirty (unsynced) edits.
class LocalCache {
  static const _kNotes = 'nexo.notes';
  static const _kHomework = 'nexo.homework';
  static const _kSettings = 'nexo.settings';
  static const _kLastSync = 'nexo.lastSync';
  static const _kPin = 'nexo.pin';
  static const _kDeviceId = 'nexo.deviceId';
  static const _kLastDeviceId = 'nexo.lastDeviceId';
  static const _kLastDeviceName = 'nexo.lastDeviceName';
  static const _kAutoSync = 'nexo.autoSync';

  Future<SharedPreferences> get _prefs => SharedPreferences.getInstance();

  Future<String?> loadLastDeviceId() async =>
      (await _prefs).getString(_kLastDeviceId);

  Future<void> saveLastDeviceId(String? id) async {
    final p = await _prefs;
    if (id == null) {
      await p.remove(_kLastDeviceId);
    } else {
      await p.setString(_kLastDeviceId, id);
    }
  }

  Future<String?> loadLastDeviceName() async =>
      (await _prefs).getString(_kLastDeviceName);

  Future<void> saveLastDeviceName(String? name) async {
    final p = await _prefs;
    if (name == null) {
      await p.remove(_kLastDeviceName);
    } else {
      await p.setString(_kLastDeviceName, name);
    }
  }

  Future<bool> loadAutoSync() async => (await _prefs).getBool(_kAutoSync) ?? true;

  Future<void> saveAutoSync(bool value) async =>
      (await _prefs).setBool(_kAutoSync, value);

  Future<List<Note>> loadNotes() async {
    final p = await _prefs;
    final raw = p.getString(_kNotes);
    if (raw == null) return [];
    final list = jsonDecode(raw) as List<dynamic>;
    return list
        .map((e) => Note.fromJson(e as Map<String, dynamic>))
        .toList();
  }

  Future<void> saveNotes(List<Note> notes) async {
    final p = await _prefs;
    final list = notes.map((n) => n.toJson()).toList();
    await p.setString(_kNotes, jsonEncode(list));
  }

  Future<List<Homework>> loadHomework() async {
    final p = await _prefs;
    final raw = p.getString(_kHomework);
    if (raw == null) return [];
    final list = jsonDecode(raw) as List<dynamic>;
    return list
        .map((e) => Homework.fromJson(e as Map<String, dynamic>))
        .toList();
  }

  Future<void> saveHomework(List<Homework> homework) async {
    final p = await _prefs;
    final list = homework.map((h) => h.toJson()).toList();
    await p.setString(_kHomework, jsonEncode(list));
  }

  Future<DeviceSettings?> loadSettings() async {
    final p = await _prefs;
    final raw = p.getString(_kSettings);
    if (raw == null) return null;
    return DeviceSettings.fromJson(jsonDecode(raw) as Map<String, dynamic>);
  }

  Future<void> saveSettings(DeviceSettings s) async {
    final p = await _prefs;
    await p.setString(_kSettings, jsonEncode(s.toJson()));
  }

  Future<int> loadLastSync() async {
    final p = await _prefs;
    return p.getInt(_kLastSync) ?? 0;
  }

  Future<void> saveLastSync(int v) async {
    final p = await _prefs;
    await p.setInt(_kLastSync, v);
  }

  Future<String?> loadPin() async {
    final p = await _prefs;
    return p.getString(_kPin);
  }

  Future<void> savePin(String pin) async {
    final p = await _prefs;
    await p.setString(_kPin, pin);
  }

  Future<String?> loadDeviceId() async {
    final p = await _prefs;
    return p.getString(_kDeviceId);
  }

  Future<void> saveDeviceId(String id) async {
    final p = await _prefs;
    await p.setString(_kDeviceId, id);
  }
}
