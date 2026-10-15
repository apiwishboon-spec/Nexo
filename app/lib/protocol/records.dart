// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
/// Tag-based record codec. Mirrors firmware/lib/nexo_core/src/nexo_codec.{h,cpp}.
library;

import 'dart:convert';
import 'dart:typed_data';

import '../models/models.dart';
import 'byte_io.dart';

class _Tag {
  static const int baseRev = 0x01;
  static const int rev = 0x02;
  static const int created = 0x03;
  static const int updated = 0x04;
  static const int flags = 0x05;
  static const int status = 0x06;
  static const int dueDay = 0x07;
  static const int id = 0x10;
  static const int title = 0x11;
  static const int body = 0x12;
  // settings reuse 0x10/0x11/0x12, plus apPassword 0x13
  static const int apPassword = 0x13;
  static const int apEnabled = 0x20;
  static const int volume = 0x21;
  static const int soundOn = 0x22;
  static const int brightness = 0x23;
  static const int screenTimeout = 0x24;
  static const int autoAdvance = 0x25;
  static const int tzOffset = 0x26;
  static const int end = 0x80;
}

// DeviceInfo tag namespace (local to that record).
class _DevTag {
  static const int proto = 0x01;
  static const int battery = 0x02;
  static const int charging = 0x03;
  static const int uptime = 0x04;
  static const int freeHeap = 0x05;
  static const int sdPresent = 0x06;
  static const int sdFree = 0x07;
  static const int sdTotal = 0x08;
  static const int musicCount = 0x09;
  static const int volume = 0x0A;
  static const int soundOn = 0x0B;
  static const int hpIn = 0x0C;
  static const int apOn = 0x0D;
  static const int rssi = 0x0E;
  static const int screenTimeout = 0x0F;
  static const int name = 0x10;
  static const int fwVer = 0x11;
  static const int mac = 0x12;
  static const int apSsid = 0x13;
  static const int apPass = 0x14;
  static const int ip = 0x15;
  static const int end = 0x80;
}

class Limits {
  static const int idLen = 16;
  static const int title = 64;
  static const int body = 4096;
  static const int subject = 48;
  static const int description = 512;
  static const int name = 32;
  static const int pin = 16;
  static const int trackName = 96;
}


void encodeNote(ByteWriter w, Note n, int baseRev) {
  w.u8(_Tag.baseRev);
  w.u32(baseRev);
  w.u8(_Tag.rev);
  w.u32(n.rev);
  w.u8(_Tag.created);
  w.u32(n.created);
  w.u8(_Tag.updated);
  w.u32(n.updated);
  w.u8(_Tag.flags);
  w.u8(n.deleted ? 1 : 0);
  w.u8(_Tag.id);
  w.str16(n.id);
  w.u8(_Tag.title);
  w.str16(clamp(n.title, Limits.title));
  w.u8(_Tag.body);
  w.str16(clamp(n.body, Limits.body));
  w.u8(_Tag.end);
}

({Note note, int baseRev}) decodeNote(ByteReader r) {
  final n = Note(id: '');
  var baseRev = 0;
  while (true) {
    final tag = r.u8();
    if (!r.ok) break;
    if (tag == _Tag.end) break;
    switch (tag) {
      case _Tag.baseRev:
        baseRev = r.u32();
        break;
      case _Tag.rev:
        n.rev = r.u32();
        break;
      case _Tag.created:
        n.created = r.u32();
        break;
      case _Tag.updated:
        n.updated = r.u32();
        break;
      case _Tag.flags:
        n.deleted = (r.u8() & 0x01) != 0;
        break;
      case _Tag.id:
        n.id = r.str16();
        break;
      case _Tag.title:
        n.title = r.str16();
        break;
      case _Tag.body:
        n.body = r.str16();
        break;
      default:
        _skipTag(r, tag);
    }
  }
  return (note: n, baseRev: baseRev);
}

void encodeHomework(ByteWriter w, Homework h, int baseRev) {
  w.u8(_Tag.baseRev);
  w.u32(baseRev);
  w.u8(_Tag.rev);
  w.u32(h.rev);
  w.u8(_Tag.created);
  w.u32(h.created);
  w.u8(_Tag.updated);
  w.u32(h.updated);
  w.u8(_Tag.flags);
  w.u8(h.deleted ? 1 : 0);
  w.u8(_Tag.status);
  w.u8(h.status.wire);
  w.u8(_Tag.dueDay);
  w.u32(h.dueEpochDay);
  w.u8(_Tag.id);
  w.str16(h.id);
  w.u8(_Tag.title);
  w.str16(clamp(h.subject, Limits.subject));
  w.u8(_Tag.body);
  w.str16(clamp(h.description, Limits.description));
  w.u8(_Tag.end);
}

({Homework homework, int baseRev}) decodeHomework(ByteReader r) {
  final h = Homework(id: '');
  var baseRev = 0;
  while (true) {
    final tag = r.u8();
    if (!r.ok) break;
    if (tag == _Tag.end) break;
    switch (tag) {
      case _Tag.baseRev:
        baseRev = r.u32();
        break;
      case _Tag.rev:
        h.rev = r.u32();
        break;
      case _Tag.created:
        h.created = r.u32();
        break;
      case _Tag.updated:
        h.updated = r.u32();
        break;
      case _Tag.flags:
        h.deleted = (r.u8() & 0x01) != 0;
        break;
      case _Tag.status:
        h.status = HomeworkStatusX.fromWire(r.u8());
        break;
      case _Tag.dueDay:
        h.dueEpochDay = r.u32();
        break;
      case _Tag.id:
        h.id = r.str16();
        break;
      case _Tag.title:
        h.subject = r.str16();
        break;
      case _Tag.body:
        h.description = r.str16();
        break;
      default:
        _skipTag(r, tag);
    }
  }
  return (homework: h, baseRev: baseRev);
}

void encodeSettings(ByteWriter w, DeviceSettings s, bool includePin) {
  w.u8(_Tag.id);
  w.str16(clamp(s.deviceName, Limits.name));
  w.u8(_Tag.body);
  w.str16(clamp(s.apSsid, Limits.name));
  if (includePin) {
    w.u8(_Tag.title);
    w.str16(clamp(s.pin ?? '', Limits.pin));
  }
  if (s.apPassword != null) {
    w.u8(_Tag.apPassword);
    w.str16(clamp(s.apPassword!, Limits.name));
  }
  w.u8(_Tag.apEnabled);
  w.u8(s.apEnabled ? 1 : 0);
  w.u8(_Tag.volume);
  w.u8(s.volume.clamp(0, 100));
  w.u8(_Tag.soundOn);
  w.u8(s.soundEnabled ? 1 : 0);
  w.u8(_Tag.brightness);
  w.u8(s.brightness.clamp(0, 100));
  w.u8(_Tag.screenTimeout);
  w.u16(s.screenTimeoutSec.clamp(0, 0xFFFF));
  w.u8(_Tag.autoAdvance);
  w.u8(s.autoAdvanceMusic ? 1 : 0);
  w.u8(_Tag.tzOffset);
  w.u16((s.timezoneOffsetMin << 16 >> 16) & 0xFFFF);
  w.u8(_Tag.end);
}

DeviceSettings decodeSettings(ByteReader r) {
  final s = DeviceSettings();
  while (true) {
    final tag = r.u8();
    if (!r.ok) break;
    if (tag == _Tag.end) break;
    switch (tag) {
      case _Tag.id:
        s.deviceName = r.str16();
        break;
      case _Tag.title:
        final pin = r.str16();
        if (pin.isNotEmpty) s.pin = clamp(pin, Limits.pin);
        break;
      case _Tag.body:
        s.apSsid = r.str16();
        break;
      case _Tag.apPassword:
        final ap = r.str16();
        if (ap.isNotEmpty) s.apPassword = ap;
        break;
      case _Tag.apEnabled:
        s.apEnabled = r.u8() != 0;
        break;
      case _Tag.volume:
        s.volume = r.u8().clamp(0, 100);
        break;
      case _Tag.soundOn:
        s.soundEnabled = r.u8() != 0;
        break;
      case _Tag.brightness:
        s.brightness = r.u8().clamp(0, 100);
        break;
      case _Tag.screenTimeout:
        s.screenTimeoutSec = r.u16();
        break;
      case _Tag.autoAdvance:
        s.autoAdvanceMusic = r.u8() != 0;
        break;
      case _Tag.tzOffset:
        s.timezoneOffsetMin = r.i16();
        break;
      default:
        _skipTag(r, tag);
    }
  }
  return s;
}

// ---- Device info ------------------------------------------------------------

DeviceDetails decodeDeviceDetails(ByteReader r) {
  final d = DeviceDetails();
  while (true) {
    final tag = r.u8();
    if (!r.ok) break;
    if (tag == _DevTag.end) break;
    switch (tag) {
      case _DevTag.proto:
        d.protocolVersion = r.u8();
        break;
      case _DevTag.battery:
        d.batteryPercent = r.u8();
        break;
      case _DevTag.charging:
        d.charging = r.u8() != 0;
        break;
      case _DevTag.uptime:
        d.uptimeSec = r.u32();
        break;
      case _DevTag.freeHeap:
        d.freeHeapKiB = r.u32();
        break;
      case _DevTag.sdPresent:
        d.sdPresent = r.u8() != 0;
        break;
      case _DevTag.sdFree:
        d.sdFreeMb = r.u32();
        break;
      case _DevTag.sdTotal:
        d.sdTotalMb = r.u32();
        break;
      case _DevTag.musicCount:
        d.musicCount = r.u16();
        break;
      case _DevTag.volume:
        d.volume = r.u8();
        break;
      case _DevTag.soundOn:
        d.soundEnabled = r.u8() != 0;
        break;
      case _DevTag.hpIn:
        d.headphoneIn = r.u8() != 0;
        break;
      case _DevTag.apOn:
        d.apEnabled = r.u8() != 0;
        break;
      case _DevTag.rssi:
        d.rssi = r.i8();
        break;
      case _DevTag.screenTimeout:
        d.screenTimeoutSec = r.u16();
        break;
      case _DevTag.name:
        d.deviceName = r.str16();
        break;
      case _DevTag.fwVer:
        d.firmwareVersion = r.str16();
        break;
      case _DevTag.mac:
        d.mac = r.str16();
        break;
      case _DevTag.apSsid:
        d.apSsid = r.str16();
        break;
      case _DevTag.apPass:
        d.apPassword = r.str16();
        break;
      case _DevTag.ip:
        d.ip = r.str16();
        break;
      default:
        _skipTag(r, tag);
    }
  }
  return d;
}

// ---- Music ------------------------------------------------------------------

List<MusicTrack> decodeMusicBatch(ByteReader r) {
  final count = r.u16();
  final out = <MusicTrack>[];
  // Per track: tag 0x10 (str16 name) followed by u32 size. No terminator.
  for (var i = 0; i < count && r.ok; i++) {
    final tag = r.u8();
    if (tag != _Tag.id) {
      _skipTag(r, tag);
    }
    final name = r.str16();
    final size = r.u32();
    out.add(MusicTrack(name, size));
  }
  return out;
}

MusicStatus decodeMusicStatus(ByteReader r) {
  final idx = r.i16();
  final playing = r.u8() != 0;
  final pos = r.u16();
  final dur = r.u16();
  final vol = r.u8();
  return MusicStatus(
    index: idx,
    playing: playing,
    positionSec: pos,
    durationSec: dur,
    volume: vol,
  );
}

// ---- Batches ----------------------------------------------------------------

typedef NoteBatch = ({List<Note> items, bool more});
typedef HomeworkBatch = ({List<Homework> items, bool more});

NoteBatch decodeNoteBatch(ByteReader r) {
  final count = r.u16();
  final items = <Note>[];
  for (var i = 0; i < count && r.ok; i++) {
    items.add(decodeNote(r).note);
  }
  final more = r.u8() != 0;
  return (items: items, more: more);
}

HomeworkBatch decodeHomeworkBatch(ByteReader r) {
  final count = r.u16();
  final items = <Homework>[];
  for (var i = 0; i < count && r.ok; i++) {
    items.add(decodeHomework(r).homework);
  }
  final more = r.u8() != 0;
  return (items: items, more: more);
}

// ---- Sync -------------------------------------------------------------------

typedef SyncDelta = ({
  List<Note> notes,
  List<Homework> homework,
  int serverEpoch,
  bool more,
});

SyncDelta decodeSyncDelta(ByteReader r) {
  final count = r.u16();
  final notes = <Note>[];
  final homework = <Homework>[];
  for (var i = 0; i < count && r.ok; i++) {
    final kind = r.u8();
    if (kind == RecordKind.note) {
      notes.add(decodeNote(r).note);
    } else if (kind == RecordKind.homework) {
      homework.add(decodeHomework(r).homework);
    }
  }
  final serverEpoch = r.u32();
  final more = r.u8() != 0;
  return (notes: notes, homework: homework, serverEpoch: serverEpoch, more: more);
}

/// Encodes dirty records for OP_SYNC_PUSH. Each record's rev field carries the
/// baseRev the client expects to overwrite (conflict detection).
Uint8List encodeSyncPush(List<Note> notes, List<Homework> homework) {
  final w = ByteWriter();
  final kinds = <int>[];
  for (final _ in notes) {
    kinds.add(RecordKind.note);
  }
  for (final _ in homework) {
    kinds.add(RecordKind.homework);
  }
  w.u16(kinds.length);
  var ni = 0, hi = 0;
  for (final kind in kinds) {
    w.u8(kind);
    if (kind == RecordKind.note) {
      encodeNote(w, notes[ni], notes[ni].rev);
      ni++;
    } else {
      encodeHomework(w, homework[hi], homework[hi].rev);
      hi++;
    }
  }
  return w.toBytes();
}

typedef SyncResult = ({
  int applied,
  int conflicts,
  int serverEpoch,
  List<Note> notes,
  List<Homework> homework,
});

SyncResult decodeSyncResult(ByteReader r) {
  final applied = r.u16();
  final conflicts = r.u16();
  final serverEpoch = r.u32();
  final notes = <Note>[];
  final homework = <Homework>[];
  // Remaining bytes are (kind, record) pairs until the buffer is exhausted.
  while (r.remaining > 0 && r.ok) {
    final kind = r.u8();
    if (!r.ok) break;
    if (kind == RecordKind.note) {
      notes.add(decodeNote(r).note);
    } else if (kind == RecordKind.homework) {
      homework.add(decodeHomework(r).homework);
    } else {
      break;
    }
  }
  return (
    applied: applied,
    conflicts: conflicts,
    serverEpoch: serverEpoch,
    notes: notes,
    homework: homework,
  );
}

// ---- Helpers ----------------------------------------------------------------

String clamp(String s, int maxBytes) {
  final bytes = utf8.encode(s);
  if (bytes.length <= maxBytes) return s;
  // Decode a byte-truncated prefix; allowMalformed drops a partial trailing rune.
  return utf8.decode(bytes.sublist(0, maxBytes), allowMalformed: true);
}

void _skipTag(ByteReader r, int tag) {
  if (tag >= 0x80) return;
  if (tag >= 0x10 && tag < 0x20) {
    final len = r.u16();
    r.skip(len);
    return;
  }
  if (tag >= 0x20) {
    final len = r.u8();
    r.skip(len);
    return;
  }
  switch (tag) {
    case _Tag.baseRev:
    case _Tag.rev:
    case _Tag.created:
    case _Tag.updated:
    case _Tag.dueDay:
      r.u32();
      break;
    case _Tag.flags:
    case _Tag.status:
    default:
      r.u8();
  }
}
