// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../ble/nexo_ble_client.dart';
import '../models/models.dart';
import '../protocol/constants.dart';
import '../protocol/records.dart';
import '../services/background_keeper.dart';
import '../storage/local_cache.dart';

/// Central application state: owns the BLE client, the offline cache, the
/// in-memory document set, and the sync flow. Exposed via Provider.
class AppState extends ChangeNotifier {
  final NexoBleClient _ble = NexoBleClient();
  final LocalCache _cache = LocalCache();

  DeviceStatus? status;
  DeviceInfo? info;

  /// Rich snapshot from GET_DEVICE_INFO (battery, storage, Wi-Fi, etc.).
  DeviceDetails? details;
  List<MusicTrack> music = [];
  MusicStatus? nowPlaying;

  final List<Note> notes = [];
  final List<Homework> homework = [];
  DeviceSettings settings = DeviceSettings();

  int lastSync = 0;
  bool busy = false;
  String? error;
  String? toast;
  final List<String> logs = [];

  final List<StreamSubscription> _subs = [];

  bool _bleConnected = false;
  bool get connected => _bleConnected;
  String? get connectedId => _ble.connectedId;
  bool get hasLocalChanges =>
      notes.any((n) => n.dirty) || homework.any((h) => h.dirty);
  int get pendingCount =>
      notes.where((n) => n.dirty).length + homework.where((h) => h.dirty).length;

  bool autoSync = true;

  String? _lastDeviceId;
  String? _lastDeviceName;
  bool _userDisconnected = false;
  bool _autoReconnecting = false;
  bool _wasReady = false;
  int _lastDataRev = -1;
  int _reconnectAttempts = 0;
  Timer? _autoSyncTimer;
  Timer? _reconnectTimer;
  DateTime _suppressAutoUntil = DateTime.fromMillisecondsSinceEpoch(0);

  String? get lastDeviceName => _lastDeviceName;
  bool get canReconnect => _lastDeviceId != null;

  List<Note> get visibleNotes =>
      (List<Note>.from(notes)..removeWhere((n) => n.deleted))
          .toList()
        ..sort((a, b) => b.updated.compareTo(a.updated));

  List<Homework> get visibleHomework {
    final list = List<Homework>.from(homework)..removeWhere((h) => h.deleted);
    list.sort((a, b) {
      if (a.status != b.status) return a.status.index.compareTo(b.status.index);
      return a.dueEpochDay.compareTo(b.dueEpochDay);
    });
    return list;
  }

  Future<void> init() async {
    settings = await _localCacheSettings();
    final cachedNotes = await _cache.loadNotes();
    final cachedHw = await _cache.loadHomework();
    notes
      ..clear()
      ..addAll(cachedNotes);
    homework
      ..clear()
      ..addAll(cachedHw);
    lastSync = await _cache.loadLastSync();
    autoSync = await _cache.loadAutoSync();
    _lastDeviceId = await _cache.loadLastDeviceId();
    _lastDeviceName = await _cache.loadLastDeviceName();

    _subs.add(_ble.connectionState.listen((s) {
      _bleConnected = s != NexoConnState.disconnected;
      final droppedFromReady = _wasReady && s == NexoConnState.disconnected;
      _wasReady = s == NexoConnState.ready;
      if (_wasReady) _reconnectAttempts = 0;
      // Once authenticated, hold a foreground service so the link survives
      // while Nexo is backgrounded (Android). Harmless elsewhere.
      if (_wasReady) BackgroundKeeper.start();
      if (droppedFromReady && !_userDisconnected && !_autoReconnecting) {
        _scheduleReconnect();
      }
      notifyListeners();
    }));
    _subs.add(_ble.statusEvents.listen(_onStatus));
    _subs.add(_ble.logEvents.listen((m) {
      logs.add(m);
      if (logs.length > 200) logs.removeAt(0);
      notifyListeners();
    }));
    _subs.add(_ble.adapterState.listen((s) {
      if (s == BluetoothAdapterState.on && !_bleConnected && canReconnect) {
        _scheduleReconnect();
      }
    }));
    notifyListeners();
  }

  void _onStatus(DeviceStatus st) {
    status = st;
    if (st.dataRev != _lastDataRev) {
      final first = _lastDataRev < 0;
      _lastDataRev = st.dataRev;
      // Ignore echoes caused by our own sync; react to other clients/device.
      if (!first && DateTime.now().isAfter(_suppressAutoUntil)) {
        _scheduleAutoSync();
      }
    }
    notifyListeners();
  }

  Future<DeviceSettings> _localCacheSettings() async {
    return await _cache.loadSettings() ?? DeviceSettings();
  }

  // ---- Connection ------------------------------------------------------------

  Stream<List<ScanResult>> scanResults() => _ble.scanResults();
  Future<void> startScan() => _ble.startScan();
  Future<void> stopScan() => _ble.stopScan();
  Stream<BluetoothAdapterState> get adapterState => _ble.adapterState;
  NexoConnState get connState => _ble.state;

  Future<bool> connectAndAuthenticate(BluetoothDevice device, String? pin) async {
    _userDisconnected = false;
    _clearMessages();
    busy = true;
    notifyListeners();
    try {
      await _ble.connect(device);
      _lastDeviceId = device.remoteId.str;
      await _cache.saveLastDeviceId(_lastDeviceId);
      final infoResp = await _ble.getInfo();
      _lastDeviceName = infoResp.deviceName;
      await _cache.saveLastDeviceName(_lastDeviceName);
      toast = 'Connected to ${infoResp.deviceName}';
      await authenticate(pin ?? await _cache.loadPin() ?? '');
      await refresh();
      // The Cardputer has no RTC, so push the phone's wall-clock time.
      await syncTimeToDevice();
      await sync(silent: true);
      await refreshDeviceDetails();
      return true;
    } catch (e) {
      error = e.toString();
      return false;
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  /// Reconnect to the last-used device without any user interaction.
  Future<bool> reconnect() async {
    final id = _lastDeviceId;
    if (id == null || _autoReconnecting || busy) return false;
    _userDisconnected = false;
    _autoReconnecting = true;
    _clearMessages();
    busy = true;
    notifyListeners();
    try {
      await _ble.connectById(id);
      final infoResp = await _ble.getInfo();
      _lastDeviceName = infoResp.deviceName;
      await _cache.saveLastDeviceName(_lastDeviceName);
      await authenticate(await _cache.loadPin() ?? '');
      await refresh();
      await syncTimeToDevice();
      await sync(silent: true);
      await refreshDeviceDetails();
      return true;
    } catch (e) {
      error = e.toString();
      return false;
    } finally {
      busy = false;
      _autoReconnecting = false;
      notifyListeners();
    }
  }

  void _scheduleReconnect() {
    if (_userDisconnected || _lastDeviceId == null) return;
    _reconnectTimer?.cancel();
    // Exponential backoff (2s, 4s, 8s, 16s, capped at 30s) so a device that is
    // rebooting/looping does not get hammered with a new connection every 2 s.
    final delay = Duration(seconds: (2 * (1 << _reconnectAttempts)).clamp(2, 30));
    if (_reconnectAttempts < 4) _reconnectAttempts++;
    _reconnectTimer = Timer(delay, () {
      if (!_bleConnected) reconnect();
    });
  }

  Future<void> authenticate(String pin) async {
    try {
      await _ble.authenticate(pin);
      await _cache.savePin(pin);
    } catch (e) {
      error = e.toString();
      rethrow;
    }
  }

  Future<void> disconnect() async {
    _userDisconnected = true;
    _autoSyncTimer?.cancel();
    _reconnectTimer?.cancel();
    await _ble.disconnect();
    await BackgroundKeeper.stop();
    notifyListeners();
  }

  Future<void> forgetDevice() async {
    await disconnect();
    _lastDeviceId = null;
    _lastDeviceName = null;
    _lastDataRev = -1;
    await _cache.saveLastDeviceId(null);
    await _cache.saveLastDeviceName(null);
    notifyListeners();
  }

  Future<void> setAutoSync(bool value) async {
    autoSync = value;
    await _cache.saveAutoSync(value);
    if (value) _scheduleAutoSync();
    notifyListeners();
  }

  void _scheduleAutoSync() {
    if (!autoSync || !_ble.isReady) return;
    _autoSyncTimer?.cancel();
    _autoSyncTimer = Timer(const Duration(milliseconds: 700), () {
      _autoSync();
    });
  }

  Future<void> _autoSync() async {
    if (!autoSync || !_ble.isReady || busy) return;
    _suppressAutoUntil = DateTime.now().add(const Duration(seconds: 3));
    await sync(silent: true);
  }

  Future<String> savedPin() async => await _cache.loadPin() ?? '';

  // ---- Data refresh (merge remote with local dirty edits) --------------------

  Future<void> refresh() async {
    busy = true;
    notifyListeners();
    try {
      final infoResp = await _ble.getInfo();
      final remoteSettings = await _ble.getSettings();
      final remoteNotes = await _ble.listNotes();
      final remoteHw = await _ble.listHomework();

      _mergeNotes(remoteNotes);
      _mergeHomework(remoteHw);

      settings = DeviceSettings(
        deviceName: remoteSettings.deviceName,
        apSsid: remoteSettings.apSsid,
        apEnabled: remoteSettings.apEnabled,
        volume: remoteSettings.volume,
        soundEnabled: remoteSettings.soundEnabled,
        brightness: remoteSettings.brightness,
        screenTimeoutSec: remoteSettings.screenTimeoutSec,
        timezoneOffsetMin: remoteSettings.timezoneOffsetMin,
        autoAdvanceMusic: remoteSettings.autoAdvanceMusic,
        pin: settings.pin,
      );

      await _persistAll();
      toast = 'Loaded ${visibleNotes.length} notes, '
          '${visibleHomework.length} homework from ${infoResp.deviceName}';
    } catch (e) {
      error = e.toString();
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  void _mergeNotes(List<Note> remote) {
    final localById = {for (final n in notes) n.id: n};
    final merged = <Note>[];
    for (final r in remote) {
      final local = localById.remove(r.id);
      if (local != null && local.dirty) {
        merged.add(local);
      } else {
        merged.add(r..dirty = false);
      }
    }
    // Keep local-only records (created offline, not yet pushed).
    merged.addAll(localById.values.where((n) => n.dirty));
    notes
      ..clear()
      ..addAll(merged);
  }

  void _mergeHomework(List<Homework> remote) {
    final localById = {for (final h in homework) h.id: h};
    final merged = <Homework>[];
    for (final r in remote) {
      final local = localById.remove(r.id);
      if (local != null && local.dirty) {
        merged.add(local);
      } else {
        merged.add(r..dirty = false);
      }
    }
    merged.addAll(localById.values.where((h) => h.dirty));
    homework
      ..clear()
      ..addAll(merged);
  }

  // ---- Notes CRUD (local-first) ---------------------------------------------

  Future<void> createNote({required String title, required String body}) async {
    final now = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    final n = Note(
      id: generateId(),
      title: title,
      body: body,
      created: now,
      updated: now,
      rev: 0,
      dirty: true,
    );
    notes.add(n);
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  Future<void> updateNote(Note n, {String? title, String? body}) async {
    final i = notes.indexWhere((e) => e.id == n.id);
    if (i < 0) return;
    notes[i] = notes[i].copyWith(
      title: title,
      body: body,
      dirty: true,
    );
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  Future<void> deleteNote(String id) async {
    final idx = notes.indexWhere((n) => n.id == id);
    if (idx < 0) return;
    if (notes[idx].rev == 0) {
      notes.removeAt(idx); // never pushed: drop locally
    } else {
      notes[idx] = notes[idx].copyWith(deleted: true, dirty: true);
    }
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  // ---- Homework CRUD ---------------------------------------------------------

  Future<void> createHomework(Homework h) async {
    h.dirty = true;
    homework.add(h);
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  Future<void> saveHomeworkRecord(Homework h) async {
    final idx = homework.indexWhere((x) => x.id == h.id);
    h.dirty = true;
    if (idx >= 0) {
      homework[idx] = h;
    } else {
      homework.add(h);
    }
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  Future<void> setHomeworkStatus(Homework h, HomeworkStatus status) async {
    h.status = status;
    h.dirty = true;
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  Future<void> deleteHomework(String id) async {
    final idx = homework.indexWhere((h) => h.id == id);
    if (idx < 0) return;
    if (homework[idx].rev == 0) {
      homework.removeAt(idx);
    } else {
      homework[idx] = homework[idx].copyWith(deleted: true, dirty: true);
    }
    await _persistAll();
    notifyListeners();
    _scheduleAutoSync();
  }

  // ---- Settings --------------------------------------------------------------

  Future<void> saveSettings(DeviceSettings s) async {
    if (_ble.isReady) {
      final updated = await _ble.putSettings(s);
      settings = DeviceSettings(
        deviceName: updated.deviceName,
        apSsid: updated.apSsid,
        apEnabled: updated.apEnabled,
        volume: updated.volume,
        soundEnabled: updated.soundEnabled,
        brightness: updated.brightness,
        screenTimeoutSec: updated.screenTimeoutSec,
        timezoneOffsetMin: updated.timezoneOffsetMin,
        autoAdvanceMusic: updated.autoAdvanceMusic,
        pin: settings.pin,
      );
      await _cache.saveSettings(settings);
    } else {
      settings = s;
      await _cache.saveSettings(settings);
    }
    notifyListeners();
  }

  // ---- Device management (v2) ------------------------------------------------

  /// Push the phone's current wall-clock time to the RTC-less device.
  Future<void> syncTimeToDevice() async {
    if (!_ble.isReady) return;
    try {
      await _ble.setTime(DateTime.now());
      settings.timezoneOffsetMin = DateTime.now().timeZoneOffset.inMinutes;
    } catch (_) {
      // Non-fatal; time sync is best-effort.
    }
  }

  Future<void> refreshDeviceDetails() async {
    if (!_ble.isReady) return;
    try {
      details = await _ble.getDeviceDetails();
      notifyListeners();
    } catch (e) {
      error = e.toString();
      notifyListeners();
    }
  }

  Future<void> findDevice() async {
    try {
      await _ble.findDevice();
      toast = 'Device is beeping';
    } catch (e) {
      error = e.toString();
    }
    notifyListeners();
  }

  Future<void> rebootDevice() async {
    try {
      await _ble.reboot();
      toast = 'Rebooting device…';
    } catch (e) {
      error = e.toString();
    }
    notifyListeners();
  }

  Future<void> factoryResetDevice() async {
    try {
      await _ble.factoryReset();
      toast = 'Device reset to defaults';
    } catch (e) {
      error = e.toString();
    }
    notifyListeners();
  }

  // ---- Music (v2) ------------------------------------------------------------

  Future<void> refreshMusic() async {
    if (!_ble.isReady) return;
    try {
      music = await _ble.listMusic();
      nowPlaying = await _ble.nowPlaying();
      notifyListeners();
    } catch (e) {
      error = e.toString();
      notifyListeners();
    }
  }

  Future<void> _music(int action, {int index = 0, int arg = 0}) async {
    if (!_ble.isReady) return;
    try {
      nowPlaying = await _ble.musicControl(action, index: index, arg: arg);
      notifyListeners();
    } catch (e) {
      error = e.toString();
      notifyListeners();
    }
  }

  Future<void> musicToggle() => _music(NexoProtocol.musicToggle);
  Future<void> musicNext() => _music(NexoProtocol.musicNext);
  Future<void> musicPrev() => _music(NexoProtocol.musicPrev);
  Future<void> musicStop() => _music(NexoProtocol.musicStop);
  Future<void> musicPlayIndex(int index) =>
      _music(NexoProtocol.musicPlayIndex, index: index);
  Future<void> musicSeek(int sec) => _music(NexoProtocol.musicSeek, arg: sec);
  Future<void> musicSetVolume(int percent) =>
      _music(NexoProtocol.musicSetVolume, arg: percent);

  Future<void> deleteMusic(String name) async {
    if (!_ble.isReady) return;
    try {
      await _ble.deleteMusic(name);
      await refreshMusic();
    } catch (e) {
      error = e.toString();
      notifyListeners();
    }
  }

  /// Replace the in-memory music list (e.g. after a Wi-Fi upload).
  void setMusicList(List<MusicTrack> tracks) {
    music = tracks;
    notifyListeners();
  }

  // ---- Sync ------------------------------------------------------------------

  Future<void> sync({bool silent = false}) async {
    if (!_ble.isReady) {
      if (!silent) {
        error = 'Not connected to a device.';
        notifyListeners();
      }
      return;
    }
    busy = true;
    _clearMessages();
    notifyListeners();
    try {
      await _pushDirty();
      await _pullRemote();
      await _persistAll();
      if (!silent) toast = 'Sync complete';
    } catch (e) {
      error = 'Sync failed: $e';
    } finally {
      busy = false;
      notifyListeners();
    }
  }

  Future<void> _pushDirty() async {
    final dirtyNotes = notes.where((n) => n.dirty).toList();
    final dirtyHw = homework.where((h) => h.dirty).toList();
    if (dirtyNotes.isEmpty && dirtyHw.isEmpty) return;

    final result = await _ble.syncPush(dirtyNotes, dirtyHw);

    // Server returns authoritative copies for every pushed record.
    for (final server in result.notes) {
      final idx = notes.indexWhere((n) => n.id == server.id);
      if (idx >= 0) {
        notes[idx] = server..dirty = false;
      } else {
        notes.add(server..dirty = false);
      }
    }
    for (final server in result.homework) {
      final idx = homework.indexWhere((h) => h.id == server.id);
      if (idx >= 0) {
        homework[idx] = server..dirty = false;
      } else {
        homework.add(server..dirty = false);
      }
    }
    if (result.conflicts > 0) {
      toast = '${result.conflicts} conflict(s) resolved to device version';
    }
  }

  Future<void> _pullRemote() async {
    var since = lastSync;
    var cursor = 0;
    var guard = 0;
    while (true) {
      final delta = await _ble.syncPull(since, cursor);
      _applyDelta(delta);
      since = delta.serverEpoch;
      if (!delta.more) break;
      cursor++;
      if (++guard > 1000) break;
    }
    lastSync = since;
    await _cache.saveLastSync(lastSync);
  }

  void _applyDelta(SyncDelta delta) {
    for (final r in delta.notes) {
      final idx = notes.indexWhere((n) => n.id == r.id);
      if (idx < 0) {
        notes.add(r..dirty = false);
      } else if (!notes[idx].dirty) {
        notes[idx] = r..dirty = false;
      } else if (r.updated >= notes[idx].updated) {
        // Device has a newer revision; drop the conflicting local edit.
        notes[idx] = r..dirty = false;
      }
    }
    for (final r in delta.homework) {
      final idx = homework.indexWhere((h) => h.id == r.id);
      if (idx < 0) {
        homework.add(r..dirty = false);
      } else if (!homework[idx].dirty) {
        homework[idx] = r..dirty = false;
      } else if (r.updated >= homework[idx].updated) {
        homework[idx] = r..dirty = false;
      }
    }
  }

  // ---- Helpers ---------------------------------------------------------------

  Future<void> _persistAll() async => _persist(notes: true, homework: true);

  Future<void> _persist({bool notes = false, bool homework = false}) async {
    if (notes) await _cache.saveNotes(this.notes);
    if (homework) await _cache.saveHomework(this.homework);
  }

  void _clearMessages() {
    error = null;
    toast = null;
  }

  void clearError() {
    error = null;
    notifyListeners();
  }

  void clearToast() {
    toast = null;
  }

  @override
  void dispose() {
    _autoSyncTimer?.cancel();
    _reconnectTimer?.cancel();
    for (final s in _subs) {
      s.cancel();
    }
    _ble.dispose();
    super.dispose();
  }
}
