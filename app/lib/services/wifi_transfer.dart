// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:convert';
import 'dart:io';

import '../models/models.dart';

/// Talks to the Cardputer's local Wi-Fi HTTP server (see docs/PROTOCOL.md §9).
///
/// Bulk audio transfer cannot go over BLE, so the app switches the device's
/// access point on, joins `http://192.168.4.1/`, and streams files over HTTP.
/// The phone must be on the device AP (the Wi-Fi screen handles joining it).
class WifiTransfer {
  WifiTransfer({this.baseUrl = 'http://192.168.4.1'});

  final String baseUrl;
  String? _cookie;

  final HttpClient _client = HttpClient()
    ..connectionTimeout = const Duration(seconds: 8);

  Uri _u(String path, [Map<String, String>? q]) =>
      Uri.parse('$baseUrl$path').replace(queryParameters: q);

  Future<_Resp> _send(HttpClientRequest req) async {
    if (_cookie != null) req.headers.set(HttpHeaders.cookieHeader, _cookie!);
    final res = await req.close();
    final setCookie = res.headers[HttpHeaders.setCookieHeader];
    if (setCookie != null && setCookie.isNotEmpty) {
      _cookie = setCookie.first.split(';').first;
    }
    final body = await res.transform(utf8.decoder).join();
    return _Resp(res.statusCode, body);
  }

  /// Authenticates with the device PIN. Throws on failure. Times out quickly so
  /// a device that is not reachable (e.g. the phone never joined the AP) fails
  /// with a clear error instead of hanging.
  Future<void> login(String pin) async {
    final req = await _client.postUrl(_u('/api/login'));
    req.headers.contentType = ContentType.json;
    req.write(jsonEncode({'pin': pin}));
    final res = await _send(req).timeout(
      const Duration(seconds: 12),
      onTimeout: () => throw HttpException(
          'Timed out reaching the device. Connect to its Wi-Fi first.'),
    );
    if (res.status != 200) {
      throw HttpException('Login failed (${res.status})');
    }
  }

  Future<List<MusicTrack>> list() async {
    final res = await _send(await _client.getUrl(_u('/api/music')));
    if (res.status != 200) throw HttpException('List failed (${res.status})');
    final doc = jsonDecode(res.body) as Map<String, dynamic>;
    final items = (doc['items'] as List? ?? []);
    return items
        .map((e) => MusicTrack(
              (e['name'] ?? '') as String,
              ((e['size'] ?? 0) as num).toInt(),
            ))
        .toList();
  }

  /// Streams a local file to `/music/<name>` via multipart upload.
  Future<void> upload({
    required String name,
    required Stream<List<int>> bytes,
    required int length,
    String path = '/api/music',
    void Function(int sent, int total)? onProgress,
  }) async {
    const boundary = 'nexo-upload-boundary';
    final head = utf8.encode('--$boundary\r\n'
        'Content-Disposition: form-data; name="file"; filename="$name"\r\n'
        'Content-Type: application/octet-stream\r\n\r\n');
    final tail = utf8.encode('\r\n--$boundary--\r\n');
    final total = head.length + length + tail.length;

    final req = await _client.postUrl(_u(path));
    req.headers.set(
      HttpHeaders.contentTypeHeader,
      'multipart/form-data; boundary=$boundary',
    );
    req.headers.contentLength = total;

    var sent = 0;
    Stream<List<int>> body() async* {
      sent += head.length;
      onProgress?.call(sent, total);
      yield head;
      await for (final chunk in bytes) {
        sent += chunk.length;
        onProgress?.call(sent, total);
        yield chunk;
      }
      sent += tail.length;
      onProgress?.call(sent, total);
      yield tail;
    }

    await req.addStream(body());
    final res = await _send(req);
    if (res.status != 200) {
      throw HttpException('Upload failed (${res.status})');
    }
  }

  /// Uploads a firmware image to the device's OTA endpoint. The device reboots
  /// on success, so the connection is expected to drop.
  Future<void> uploadFirmware({
    required Stream<List<int>> bytes,
    required int length,
    void Function(int sent, int total)? onProgress,
  }) async {
    await upload(
      name: 'firmware.bin',
      bytes: bytes,
      length: length,
      path: '/api/ota',
      onProgress: onProgress,
    );
  }

  Future<List<MemoItem>> listMemos() async {
    final res = await _send(await _client.getUrl(_u('/api/memos')));
    if (res.status != 200) throw HttpException('List failed (${res.status})');
    final doc = jsonDecode(res.body) as Map<String, dynamic>;
    final items = (doc['items'] as List? ?? []);
    return items
        .map((e) => MemoItem(
              (e['name'] ?? '') as String,
              ((e['size'] ?? 0) as num).toInt(),
            ))
        .toList();
  }

  /// Streams a memo (16 kHz mono WAV) to a sink without text decoding.
  Future<void> downloadMemo(int index, IOSink sink) async {
    final req = await _client.getUrl(_u('/api/memos/file', {'i': '$index'}));
    if (_cookie != null) req.headers.set(HttpHeaders.cookieHeader, _cookie!);
    final res = await req.close();
    if (res.statusCode != 200) {
      throw HttpException('Download failed (${res.statusCode})');
    }
    await res.pipe(sink);
  }

  Future<void> deleteMemo(String name) async {
    final req = await _client.postUrl(_u('/api/memos/delete'));
    req.headers.contentType = ContentType.json;
    req.write(jsonEncode({'name': name}));
    final res = await _send(req);
    if (res.status != 200 && res.status != 404) {
      throw HttpException('Delete failed (${res.status})');
    }
  }

  Future<void> delete(String name) async {
    final req = await _client.postUrl(_u('/api/music/delete'));
    req.headers.contentType = ContentType.json;
    req.write(jsonEncode({'name': name}));
    final res = await _send(req);
    if (res.status != 200 && res.status != 404) {
      throw HttpException('Delete failed (${res.status})');
    }
  }

  /// Streams a track to a sink without text decoding (binary-safe).
  Future<void> downloadTo(String name, IOSink sink) async {
    final req = await _client.getUrl(_u('/api/music/file', {'name': name}));
    if (_cookie != null) req.headers.set(HttpHeaders.cookieHeader, _cookie!);
    final res = await req.close();
    if (res.statusCode != 200) {
      throw HttpException('Download failed (${res.statusCode})');
    }
    await res.pipe(sink);
  }

  void close() => _client.close(force: true);
}

class _Resp {
  _Resp(this.status, this.body);
  final int status;
  final String body;
}
