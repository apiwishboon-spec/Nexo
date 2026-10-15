// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:nexo_app/ble/nexo_ble_client.dart';
import 'package:nexo_app/models/models.dart';
import 'package:nexo_app/protocol/byte_io.dart';
import 'package:nexo_app/protocol/framing.dart';
import 'package:nexo_app/protocol/records.dart';

void main() {
  group('models', () {
    test('generateId produces 16 hex chars and validates', () {
      for (var i = 0; i < 50; i++) {
        final id = generateId();
        expect(id.length, 16);
        expect(isValidId(id), isTrue);
      }
      expect(isValidId('not-an-id'), isFalse);
      expect(isValidId('0123456789ABCDEF'), isTrue);
    });

    test('due date round-trips epoch days', () {
      expect(dueDateFromEpochDay(0), isNull);
      expect(dueIsoFromEpochDay(0), '');
      final d = DateTime.utc(2026, 10, 14);
      final day = epochDayFromDate(d);
      expect(dueIsoFromEpochDay(day), '2026-10-14');
      expect(dueDateFromEpochDay(day), d);
      expect(epochDayFromDate(DateTime.utc(1970, 1, 1)), 1);
    });
  });

  group('framing', () {
    test('single-chunk round trip', () {
      final payload = Uint8List.fromList([1, 2, 3, 4]);
      final chunks = encodeMessage(Message(1, 0x12, 7, payload), mtuPayload: 20);
      expect(chunks.length, 1);
      final msg = Reassembler().feed(chunks.first);
      expect(msg, isNotNull);
      expect(msg!.type, 0x12);
      expect(msg.id, 7);
      expect(msg.payload, payload);
    });

    test('multi-chunk ordered round trip', () {
      final payload = Uint8List.fromList(List.generate(500, (i) => i % 256));
      final chunks =
          encodeMessage(Message(1, 0x41, 5150, payload), mtuPayload: 32);
      expect(chunks.length, greaterThan(1));
      final r = Reassembler();
      Message? done;
      for (final c in chunks) {
        done = r.feed(c) ?? done;
      }
      expect(done, isNotNull);
      expect(done!.id, 5150);
      expect(done.payload, payload);
    });
  });

  group('record codec', () {
    test('note round trip preserves fields', () {
      final w = ByteWriter();
      final n = Note(
        id: generateId(),
        title: 'Maths',
        body: 'Chapter 4',
        created: 100,
        updated: 200,
        rev: 3,
      );
      encodeNote(w, n, 3);
      final r = decodeNote(ByteReader(w.toBytes()));
      expect(r.note.title, 'Maths');
      expect(r.note.body, 'Chapter 4');
      expect(r.note.created, 100);
      expect(r.note.updated, 200);
      expect(r.note.rev, 3);
      expect(r.baseRev, 3);
    });

    test('homework round trip preserves fields', () {
      final w = ByteWriter();
      final h = Homework(
        id: generateId(),
        subject: 'Physics',
        description: 'Lab report',
        dueEpochDay: 20000,
        status: HomeworkStatus.inProgress,
        rev: 1,
      );
      encodeHomework(w, h, 1);
      final r = decodeHomework(ByteReader(w.toBytes()));
      expect(r.homework.subject, 'Physics');
      expect(r.homework.description, 'Lab report');
      expect(r.homework.status, HomeworkStatus.inProgress);
      expect(r.homework.dueEpochDay, 20000);
      expect(r.baseRev, 1);
    });

    test('settings omit the PIN unless requested', () {
      final w = ByteWriter();
      encodeSettings(
        w,
        DeviceSettings(
          deviceName: 'Study',
          apSsid: 'StudyAP',
          apEnabled: true,
          pin: '1234',
        ),
        false,
      );
      final decoded = decodeSettings(ByteReader(w.toBytes()));
      expect(decoded.deviceName, 'Study');
      expect(decoded.apSsid, 'StudyAP');
      expect(decoded.apEnabled, isTrue);
      expect(decoded.pin, isNull);
    });

    test('sync push encodes a note and a homework item', () {
      final n = Note(id: generateId(), title: 'T', body: 'B', rev: 2);
      final h = Homework(id: generateId(), subject: 'S', rev: 0);
      final bytes = encodeSyncPush([n], [h]);
      final r = ByteReader(bytes);
      final count = r.u16();
      expect(count, 2);
    });
  });

  group('device status', () {
    test('decodes 20-byte status with dataRev', () {
      final b = List<int>.filled(20, 0);
      b[0] = 1;
      b[1] = 1;
      b[2] = 1;
      b[4] = 2;
      b[8] = 3;
      b[12] = 42;
      b[16] = 7;
      final s = DeviceStatus.fromBytes(b);
      expect(s.version, 1);
      expect(s.authenticated, isTrue);
      expect(s.noteCount, 2);
      expect(s.homeworkCount, 3);
      expect(s.uptimeSeconds, 42);
      expect(s.dataRev, 7);
    });

    test('legacy 16-byte status decodes with dataRev 0', () {
      final s = DeviceStatus.fromBytes(List<int>.filled(16, 0));
      expect(s.dataRev, 0);
      expect(s.noteCount, 0);
    });
  });

  group('v2 codecs', () {
    test('extended settings round trip', () {
      final w = ByteWriter();
      encodeSettings(
        w,
        DeviceSettings(
          deviceName: 'Desk',
          apSsid: 'Nexo',
          apEnabled: true,
          volume: 42,
          soundEnabled: false,
          brightness: 70,
          screenTimeoutSec: 45,
          autoAdvanceMusic: false,
          timezoneOffsetMin: 420,
        ),
        false,
      );
      final d = decodeSettings(ByteReader(w.toBytes()));
      expect(d.deviceName, 'Desk');
      expect(d.volume, 42);
      expect(d.soundEnabled, isFalse);
      expect(d.brightness, 70);
      expect(d.screenTimeoutSec, 45);
      expect(d.autoAdvanceMusic, isFalse);
      expect(d.timezoneOffsetMin, 420);
    });

    test('negative timezone offset round trips', () {
      final w = ByteWriter();
      encodeSettings(w, DeviceSettings(timezoneOffsetMin: -300), false);
      final d = decodeSettings(ByteReader(w.toBytes()));
      expect(d.timezoneOffsetMin, -300);
    });

    test('device details decode', () {
      final w = ByteWriter();
      w.u8(0x01);
      w.u8(2); // proto
      w.u8(0x02);
      w.u8(88); // battery
      w.u8(0x03);
      w.u8(1); // charging
      w.u8(0x04);
      w.u32(1234); // uptime
      w.u8(0x09);
      w.u16(3); // music count
      w.u8(0x0A);
      w.u8(55); // volume
      w.u8(0x0E);
      w.u8(0xC0); // rssi -64
      w.u8(0x10);
      w.str16('Nexo');
      w.u8(0x11);
      w.str16('2.0.0');
      w.u8(0x15);
      w.str16('192.168.4.1');
      w.u8(0x80);
      final d = decodeDeviceDetails(ByteReader(w.toBytes()));
      expect(d.protocolVersion, 2);
      expect(d.batteryPercent, 88);
      expect(d.charging, isTrue);
      expect(d.uptimeSec, 1234);
      expect(d.musicCount, 3);
      expect(d.volume, 55);
      expect(d.rssi, -64);
      expect(d.deviceName, 'Nexo');
      expect(d.firmwareVersion, '2.0.0');
      expect(d.ip, '192.168.4.1');
    });

    test('music batch decode', () {
      final w = ByteWriter();
      w.u16(2);
      w.u8(0x10);
      w.str16('a.mp3');
      w.u32(100);
      w.u8(0x10);
      w.str16('b.mp3');
      w.u32(200);
      final tracks = decodeMusicBatch(ByteReader(w.toBytes()));
      expect(tracks.length, 2);
      expect(tracks[0].name, 'a.mp3');
      expect(tracks[0].size, 100);
      expect(tracks[1].name, 'b.mp3');
      expect(tracks[1].size, 200);
    });

    test('music status decode with negative index', () {
      final w = ByteWriter();
      w.u16(0xFFFF); // index -1
      w.u8(0);
      w.u16(0);
      w.u16(0);
      w.u8(60);
      final s = decodeMusicStatus(ByteReader(w.toBytes()));
      expect(s.index, -1);
      expect(s.playing, isFalse);
      expect(s.volume, 60);
    });
  });
}
