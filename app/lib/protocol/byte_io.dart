// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:convert';
import 'dart:typed_data';

/// Little-endian binary writer matching the firmware codec (nexo_codec.cpp).
class ByteWriter {
  final List<int> _buf = [];

  void u8(int v) => _buf.add(v & 0xFF);

  void u16(int v) {
    _buf.add(v & 0xFF);
    _buf.add((v >> 8) & 0xFF);
  }

  void u32(int v) {
    _buf.add(v & 0xFF);
    _buf.add((v >> 8) & 0xFF);
    _buf.add((v >> 16) & 0xFF);
    _buf.add((v >> 24) & 0xFF);
  }

  void raw(List<int> bytes) => _buf.addAll(bytes);

  void str16(String s) {
    final b = _utf8(s);
    u16(b.length);
    _buf.addAll(b);
  }

  /// Fixed-width string (used for 16-byte IDs).
  void fixed(String s, int len) {
    final b = _utf8(s);
    for (var i = 0; i < len; i++) {
      _buf.add(i < b.length ? b[i] : 0);
    }
  }

  Uint8List toBytes() => Uint8List.fromList(_buf);

  static List<int> _utf8(String s) =>
      Uint8List.fromList(const Utf8Encoder().convert(s));
}

/// Little-endian, bounds-checked reader matching the firmware ByteReader.
class ByteReader {
  ByteReader(this._data, [int? length])
      : _len = length ?? _data.length;

  final Uint8List _data;
  final int _len;
  int _i = 0;
  bool ok = true;

  int get remaining => _len - _i;

  int u8() {
    if (_i + 1 > _len) return _fail();
    return _data[_i++];
  }

  int u16() {
    if (_i + 2 > _len) return _fail();
    final v = _data[_i] | (_data[_i + 1] << 8);
    _i += 2;
    return v;
  }

  /// Signed 16-bit (two's complement).
  int i16() {
    final v = u16();
    return v >= 0x8000 ? v - 0x10000 : v;
  }

  /// Signed 8-bit (two's complement).
  int i8() {
    final v = u8();
    return v >= 0x80 ? v - 0x100 : v;
  }

  int u32() {
    if (_i + 4 > _len) return _fail();
    final v = _data[_i] |
        (_data[_i + 1] << 8) |
        (_data[_i + 2] << 16) |
        (_data[_i + 3] << 24);
    _i += 4;
    return v & 0xFFFFFFFF;
  }

  String str16() {
    final len = u16();
    if (_i + len > _len) {
      _fail();
      return '';
    }
    final s = _decode(_i, len);
    _i += len;
    return s;
  }

  String fixed(int len) {
    if (_i + len > _len) {
      _fail();
      return '';
    }
    final s = _decode(_i, len);
    _i += len;
    return s;
  }

  void skip(int n) {
    if (_i + n > _len) {
      _fail();
      return;
    }
    _i += n;
  }

  int _fail() {
    ok = false;
    return 0;
  }

  String _decode(int start, int len) {
    var end = start + len;
    // Trim trailing NULs used for fixed-width padding.
    while (end > start && _data[end - 1] == 0) {
      end--;
    }
    try {
      return const Utf8Decoder(allowMalformed: true)
          .convert(_data.sublist(start, end));
    } catch (_) {
      return '';
    }
  }
}
