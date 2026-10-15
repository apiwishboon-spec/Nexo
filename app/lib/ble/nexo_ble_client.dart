// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:async';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/byte_io.dart';
import '../protocol/constants.dart';
import '../protocol/framing.dart';
import '../protocol/records.dart';
import '../models/models.dart';

/// High-level connection state for the UI.
enum NexoConnState { disconnected, connecting, connected, authenticating, ready }

/// Result of a single protocol request.
class NexoResponse {
  NexoResponse(this.status, this.body);

  final int status;
  final Uint8List body;

  bool get ok => status == NexoProtocol.stOk;

  ByteReader reader() => ByteReader(body);

  String get errorText => NexoProtocol.statusName(status);
}

/// Metadata returned by OP_GET_INFO.
class DeviceInfo {
  DeviceInfo({
    required this.version,
    required this.deviceName,
    required this.noteCount,
    required this.homeworkCount,
  });

  final int version;
  final String deviceName;
  final int noteCount;
  final int homeworkCount;
}

/// Live status broadcast on the Status characteristic.
class DeviceStatus {
  DeviceStatus({
    required this.version,
    required this.authenticated,
    required this.connected,
    required this.noteCount,
    required this.homeworkCount,
    required this.uptimeSeconds,
    this.dataRev = 0,
  });

  final int version;
  final bool authenticated;
  final bool connected;
  final int noteCount;
  final int homeworkCount;
  final int uptimeSeconds;

  /// Monotonic revision bumped by the device whenever its data changes.
  final int dataRev;

  static DeviceStatus fromBytes(List<int> b) => DeviceStatus(
        version: b.isNotEmpty ? b[0] : 0,
        authenticated: b.length > 1 && b[1] != 0,
        connected: b.length > 2 && b[2] != 0,
        noteCount: b.length >= 8
            ? ByteData.sublistView(Uint8List.fromList(b), 4, 8)
                .getUint32(0, Endian.little)
            : 0,
        homeworkCount: b.length >= 12
            ? ByteData.sublistView(Uint8List.fromList(b), 8, 12)
                .getUint32(0, Endian.little)
            : 0,
        uptimeSeconds: b.length >= 16
            ? ByteData.sublistView(Uint8List.fromList(b), 12, 16)
                .getUint32(0, Endian.little)
            : 0,
        dataRev: b.length >= 20
            ? ByteData.sublistView(Uint8List.fromList(b), 16, 20)
                .getUint32(0, Endian.little)
            : 0,
      );
}

/// Talks the Nexo BLE protocol to a Cardputer.
class NexoBleClient {
  NexoBleClient() {
    _connSub = null;
  }

  BluetoothDevice? _device;
  BluetoothCharacteristic? _rx;
  BluetoothCharacteristic? _tx;
  BluetoothCharacteristic? _status;

  final Reassembler _reassembler = Reassembler();
  final Map<int, Completer<NexoResponse>> _pending = {};
  final math.Random _rand = math.Random();
  int _mtuPayload = 20;

  StreamSubscription? _txSub;
  StreamSubscription? _statusSub;
  StreamSubscription? _connSub;
  StreamSubscription? _scanSub;

  final _connState = StreamController<NexoConnState>.broadcast();
  final _statusEvents = StreamController<DeviceStatus>.broadcast();
  final _logEvents = StreamController<String>.broadcast();

  NexoConnState _state = NexoConnState.disconnected;

  Stream<NexoConnState> get connectionState => _connState.stream;
  Stream<DeviceStatus> get statusEvents => _statusEvents.stream;
  Stream<String> get logEvents => _logEvents.stream;
  NexoConnState get state => _state;
  bool get isReady => _state == NexoConnState.ready;
  String? get peerName => _device?.platformName;
  String? get connectedId => _device?.remoteId.str;

  void _setState(NexoConnState s) {
    _state = s;
    if (!_connState.isClosed) _connState.add(s);
  }

  void _log(String msg) {
    if (!_logEvents.isClosed) _logEvents.add(msg);
  }

  // ---- Scanning -------------------------------------------------------------

  Stream<List<ScanResult>> scanResults() => FlutterBluePlus.scanResults;

  Stream<BluetoothAdapterState> get adapterState => FlutterBluePlus.adapterState;

  Future<void> startScan() async {
    await FlutterBluePlus.startScan(
      timeout: const Duration(seconds: 15),
      withServices: [Guid(NexoProtocol.serviceUuid)],
      continuousUpdates: true,
    );
  }

  Future<void> stopScan() => FlutterBluePlus.stopScan();

  // ---- Connection -----------------------------------------------------------

  Future<void> connect(BluetoothDevice device) async {
    await disconnect();
    _device = device;
    _setState(NexoConnState.connecting);
    _log('Connecting to ${device.remoteId.str}...');

    _connSub = device.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected) {
        _handleDisconnect();
      }
    });

    await device.connect(timeout: const Duration(seconds: 20), mtu: 512);

    _mtuPayload = math.max(20, math.min(device.mtuNow - 3, 244));
    _log('Connected. MTU=${device.mtuNow}, chunk payload=$_mtuPayload');

    final services = await device.discoverServices();
    final svc = services.firstWhere(
      (s) => s.uuid == Guid(NexoProtocol.serviceUuid),
      orElse: () => throw StateError('Nexo service not found'),
    );

    _rx = _char(svc, NexoProtocol.rxUuid);
    _tx = _char(svc, NexoProtocol.txUuid);
    _status = _char(svc, NexoProtocol.statusUuid);

    _txSub = _tx!.onValueReceived.listen(_onNotification);
    await _tx!.setNotifyValue(true);

    if (_status != null) {
      try {
        final initial = await _status!.read();
        if (initial.isNotEmpty) {
          _statusEvents.add(DeviceStatus.fromBytes(initial));
        }
      } catch (_) {}
      _statusSub = _status!.onValueReceived.listen((v) {
        _statusEvents.add(DeviceStatus.fromBytes(v));
      });
      await _status!.setNotifyValue(true);
    }

    _setState(NexoConnState.connected);
  }

  /// Reconnects to a previously known device by its remote id, falling back to
  /// a short scan if the OS no longer knows the peripheral (common on macOS
  /// after the app or adapter restarts).
  Future<void> connectById(String remoteId) async {
    try {
      await connect(BluetoothDevice.fromId(remoteId));
      return;
    } catch (e) {
      _log('Direct reconnect failed ($e); scanning...');
      try {
        await disconnect();
      } catch (_) {}
    }

    final completer = Completer<BluetoothDevice>();
    await startScan();
    final sub = scanResults().listen((results) {
      for (final r in results) {
        if (r.device.remoteId.str == remoteId && !completer.isCompleted) {
          completer.complete(r.device);
        }
      }
    });
    try {
      final dev = await completer.future.timeout(const Duration(seconds: 15));
      await connect(dev);
    } finally {
      await sub.cancel();
      await stopScan();
    }
  }

  BluetoothCharacteristic _char(BluetoothService svc, String uuid) {
    final want = Guid(uuid);
    final match = svc.characteristics.where((c) => c.characteristicUuid == want);
    if (match.isEmpty) throw StateError('Characteristic $uuid not found');
    return match.first;
  }

  Future<void> authenticate(String pin) async {
    _setState(NexoConnState.authenticating);
    final w = ByteWriter();
    final pinBytes = Uint8List.fromList(pin.codeUnits);
    w.u8(pinBytes.length);
    w.raw(pinBytes);
    final resp = await _request(NexoProtocol.opAuth, w.toBytes());
    if (resp.ok) {
      _setState(NexoConnState.ready);
      _log('Authenticated.');
    } else {
      _setState(NexoConnState.connected);
      throw StateError(resp.errorText);
    }
  }

  void _handleDisconnect() {
    _log('Disconnected.');
    for (final c in _pending.values) {
      if (!c.isCompleted) {
        c.completeError(StateError('Disconnected'));
      }
    }
    _pending.clear();
    _txSub?.cancel();
    _statusSub?.cancel();
    _connSub?.cancel();
    _txSub = _statusSub = _connSub = null;
    _rx = _tx = _status = null;
    _setState(NexoConnState.disconnected);
  }

  Future<void> disconnect() async {
    await _scanSub?.cancel();
    _scanSub = null;
    final d = _device;
    _device = null;
    try {
      await d?.disconnect();
    } catch (_) {}
    _handleDisconnect();
  }

  // ---- Request/response -----------------------------------------------------

  void _onNotification(List<int> data) {
    final msg = _reassembler.feed(Uint8List.fromList(data));
    if (msg == null) return;
    final respType = msg.type & ~NexoProtocol.responseBit;
    final isResponse = (msg.type & NexoProtocol.responseBit) != 0;
    if (!isResponse || msg.payload.isEmpty) return;
    final completer = _pending.remove(msg.id);
    if (completer == null) {
      _log('Unexpected response for id ${msg.id} (type $respType)');
      return;
    }
    final status = msg.payload[0];
    final body = msg.payload.sublist(1);
    completer.complete(NexoResponse(status, body));
  }

  Future<NexoResponse> _request(
    int op,
    Uint8List payload, {
    Duration timeout = const Duration(seconds: 12),
  }) async {
    if (_rx == null) throw StateError('Not connected');
    // Random, per-request ids so two apps talking to the same Cardputer at the
    // same time cannot collide on the id space (responses are broadcast).
    int id;
    do {
      id = _rand.nextInt(0xFFFE) + 1;
    } while (_pending.containsKey(id));

    final completer = Completer<NexoResponse>();
    _pending[id] = completer;

    final msg = Message(NexoProtocol.version, op, id, payload);
    final chunks = encodeMessage(msg, mtuPayload: _mtuPayload);

    try {
      for (final chunk in chunks) {
        await _rx!.write(chunk, withoutResponse: false);
      }
    } catch (e) {
      _pending.remove(id);
      rethrow;
    }

    return completer.future.timeout(timeout, onTimeout: () {
      _pending.remove(id);
      throw TimeoutException('Request 0x${op.toRadixString(16)} timed out');
    });
  }

  // ---- High-level operations ------------------------------------------------

  Future<int> ping() async {
    final resp = await _request(NexoProtocol.opPing, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    return resp.reader().u32();
  }

  Future<DeviceInfo> getInfo() async {
    final resp = await _request(NexoProtocol.opGetInfo, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    final r = resp.reader();
    final version = r.u8();
    final name = r.str16();
    final notes = r.u32();
    final hw = r.u32();
    return DeviceInfo(
      version: version,
      deviceName: name,
      noteCount: notes,
      homeworkCount: hw,
    );
  }

  Future<List<Note>> listNotes() async {
    final all = <Note>[];
    var offset = 0;
    while (true) {
      final w = ByteWriter();
      w.u16(offset);
      w.u16(100);
      final resp = await _request(NexoProtocol.opListNotes, w.toBytes());
      if (!resp.ok) throw StateError(resp.errorText);
      final batch = decodeNoteBatch(resp.reader());
      all.addAll(batch.items);
      if (!batch.more) break;
      offset += batch.items.length;
      if (batch.items.isEmpty) break;
    }
    return all;
  }

  Future<Note> putNote(Note n, int baseRev) async {
    final w = ByteWriter();
    encodeNote(w, n, baseRev);
    final resp = await _request(NexoProtocol.opPutNote, w.toBytes());
    final r = resp.reader();
    if (resp.status == NexoProtocol.stOk || resp.status == NexoProtocol.stConflict) {
      return decodeNote(r).note;
    }
    throw StateError(resp.errorText);
  }

  Future<void> deleteNote(String id, int baseRev) async {
    final w = ByteWriter();
    w.fixed(id, 16);
    w.u32(baseRev);
    final resp = await _request(NexoProtocol.opDeleteNote, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
  }

  Future<List<Homework>> listHomework() async {
    final all = <Homework>[];
    var offset = 0;
    while (true) {
      final w = ByteWriter();
      w.u16(offset);
      w.u16(100);
      final resp = await _request(NexoProtocol.opListHomework, w.toBytes());
      if (!resp.ok) throw StateError(resp.errorText);
      final batch = decodeHomeworkBatch(resp.reader());
      all.addAll(batch.items);
      if (!batch.more) break;
      offset += batch.items.length;
      if (batch.items.isEmpty) break;
    }
    return all;
  }

  Future<Homework> putHomework(Homework h, int baseRev) async {
    final w = ByteWriter();
    encodeHomework(w, h, baseRev);
    final resp = await _request(NexoProtocol.opPutHomework, w.toBytes());
    final r = resp.reader();
    if (resp.status == NexoProtocol.stOk || resp.status == NexoProtocol.stConflict) {
      return decodeHomework(r).homework;
    }
    throw StateError(resp.errorText);
  }

  Future<void> deleteHomework(String id, int baseRev) async {
    final w = ByteWriter();
    w.fixed(id, 16);
    w.u32(baseRev);
    final resp = await _request(NexoProtocol.opDeleteHomework, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
  }

  Future<DeviceSettings> getSettings() async {
    final resp = await _request(NexoProtocol.opGetSettings, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeSettings(resp.reader());
  }

  Future<DeviceSettings> putSettings(DeviceSettings s) async {
    final w = ByteWriter();
    encodeSettings(w, s, (s.pin != null && s.pin!.isNotEmpty));
    final resp = await _request(NexoProtocol.opPutSettings, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeSettings(resp.reader());
  }

  Future<SyncDelta> syncPull(int since, int cursor) async {
    final w = ByteWriter();
    w.u32(since);
    w.u8(cursor);
    final resp = await _request(NexoProtocol.opSyncPull, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeSyncDelta(resp.reader());
  }

  Future<SyncResult> syncPush(List<Note> notes, List<Homework> homework) async {
    final payload = encodeSyncPush(notes, homework);
    final resp = await _request(NexoProtocol.opSyncPush, payload);
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeSyncResult(resp.reader());
  }

  Future<({int notes, int homework, int maxUpdated})> syncSummary() async {
    final resp = await _request(NexoProtocol.opSyncSummary, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    final r = resp.reader();
    return (notes: r.u32(), homework: r.u32(), maxUpdated: r.u32());
  }

  // ---- v2: device management -------------------------------------------------

  Future<DeviceDetails> getDeviceDetails() async {
    final resp = await _request(NexoProtocol.opGetDeviceInfo, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeDeviceDetails(resp.reader());
  }

  Future<void> setTime(DateTime localTime, {int? tzOffsetMin}) async {
    final w = ByteWriter();
    w.u32(localTime.millisecondsSinceEpoch ~/ 1000);
    final tz = tzOffsetMin ?? localTime.timeZoneOffset.inMinutes;
    w.u16(tz & 0xFFFF);
    final resp = await _request(NexoProtocol.opSetTime, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
  }

  Future<void> reboot() async {
    final resp = await _request(NexoProtocol.opReboot, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
  }

  Future<void> factoryReset() async {
    final resp = await _request(NexoProtocol.opFactoryReset, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
  }

  Future<void> findDevice() async {
    final resp = await _request(NexoProtocol.opFindDevice, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
  }

  // ---- v2: music -------------------------------------------------------------

  Future<List<MusicTrack>> listMusic() async {
    final resp = await _request(NexoProtocol.opListMusic, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeMusicBatch(resp.reader());
  }

  Future<MusicStatus> musicControl(int action, {int index = 0, int arg = 0}) async {
    final w = ByteWriter();
    w.u8(action);
    w.u16(index & 0xFFFF);
    w.u16(arg & 0xFFFF);
    final resp = await _request(NexoProtocol.opMusicControl, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeMusicStatus(resp.reader());
  }

  Future<MusicStatus> nowPlaying() async {
    final resp = await _request(NexoProtocol.opNowPlaying, Uint8List(0));
    if (!resp.ok) throw StateError(resp.errorText);
    return decodeMusicStatus(resp.reader());
  }

  Future<void> deleteMusic(String name) async {
    final w = ByteWriter();
    w.str16(name);
    final resp = await _request(NexoProtocol.opDeleteMusic, w.toBytes());
    if (!resp.ok) throw StateError(resp.errorText);
  }

  void dispose() {
    _txSub?.cancel();
    _statusSub?.cancel();
    _connSub?.cancel();
    _scanSub?.cancel();
    _connState.close();
    _statusEvents.close();
    _logEvents.close();
  }
}
