// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
/// BLE chunk framing / reassembly. Mirrors nexo_frame.{h,cpp}.
library;

import 'dart:typed_data';

import 'constants.dart';

/// Splits a message into chunks of at most [mtuPayload] bytes (including the
/// 8-byte header). Always emits at least one chunk.
List<Uint8List> encodeMessage(
  Message msg, {
  int mtuPayload = 185,
}) {
  if (msg.payload.length > NexoProtocol.maxMessage) {
    throw ArgumentError('payload exceeds max message size');
  }
  final chunkData = mtuPayload - NexoProtocol.chunkHeaderLen;
  final total = msg.payload.length;
  final out = <Uint8List>[];
  var offset = 0;
  do {
    var n = total - offset;
    if (n > chunkData) n = chunkData;
    final chunk = Uint8List(8 + n);
    chunk[0] = msg.version & 0xFF;
    chunk[1] = msg.type & 0xFF;
    chunk[2] = msg.id & 0xFF;
    chunk[3] = (msg.id >> 8) & 0xFF;
    chunk[4] = offset & 0xFF;
    chunk[5] = (offset >> 8) & 0xFF;
    chunk[6] = total & 0xFF;
    chunk[7] = (total >> 8) & 0xFF;
    chunk.setRange(8, 8 + n, msg.payload, offset);
    out.add(chunk);
    offset += n;
  } while (offset < total);
  return out;
}

class Message {
  Message(this.version, this.type, this.id, this.payload);

  final int version;
  final int type;
  final int id;
  final Uint8List payload;
}

class _Slot {
  bool active = false;
  int type = 0;
  int id = 0;
  int total = 0;
  int filled = 0;
  late Uint8List buf;
  int lastSeen = 0;
}

/// Reassembles chunked messages from a single peer.
class Reassembler {
  static const int _slots = 8;
  final List<_Slot> _slot = List.generate(_slots, (_) => _Slot());
  int _clock = 0;
  int lastError = 0;

  /// Feeds one notification payload. Returns a completed [Message] or null.
  Message? feed(Uint8List data) {
    final len = data.length;
    if (len < 8) {
      lastError = 0x02;
      return null;
    }
    final view = ByteData.sublistView(data);
    final version = data[0];
    final type = data[1];
    final id = view.getUint16(2, Endian.little);
    final offset = view.getUint16(4, Endian.little);
    final total = view.getUint16(6, Endian.little);
    final chunkLen = len - 8;

    if (offset + chunkLen > total) {
      lastError = 0x02;
      return null;
    }

    var idx = -1;
    var freeIdx = -1;
    for (var i = 0; i < _slots; i++) {
      final s = _slot[i];
      if (s.active) {
        if (s.type == type && s.id == id && s.total == total) {
          idx = i;
          break;
        }
      } else if (freeIdx < 0) {
        freeIdx = i;
      }
    }

    if (idx < 0) {
      if (offset != 0) {
        lastError = 0x03;
        return null;
      }
      if (freeIdx < 0) {
        var oldest = 1 << 62;
        for (var i = 0; i < _slots; i++) {
          if (_slot[i].lastSeen < oldest) {
            oldest = _slot[i].lastSeen;
            freeIdx = i;
          }
        }
      }
      final s = _slot[freeIdx];
      s.active = true;
      s.type = type;
      s.id = id;
      s.total = total;
      s.filled = 0;
      s.buf = Uint8List(total);
      idx = freeIdx;
    }

    final s = _slot[idx];
    s.lastSeen = ++_clock;

    if (offset != s.filled) {
      s.active = false;
      lastError = 0x02;
      return null;
    }
    s.buf.setRange(offset, offset + chunkLen, data, 8);
    s.filled = offset + chunkLen;

    if (s.filled == s.total) {
      final msg = Message(version, type, id, s.buf);
      s.active = false;
      s.buf = Uint8List(0);
      return msg;
    }
    return null;
  }
}
