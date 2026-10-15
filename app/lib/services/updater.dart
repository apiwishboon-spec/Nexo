// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'dart:convert';
import 'dart:io';

import 'package:pub_semver/pub_semver.dart';

/// The GitHub repository that publishes Nexo releases.
const String kUpdateRepo = 'apiwishboon-spec/Nexo';

/// The device firmware image attached to every release.
const String kFirmwareAsset = 'Nexo-m5stack-cardputer.bin';

/// An asset attached to a GitHub release.
class UpdateAsset {
  const UpdateAsset({required this.name, required this.url, required this.size});

  final String name;
  final String url;
  final int size;
}

/// A published release we can update to.
class AppRelease {
  const AppRelease({
    required this.tag,
    required this.version,
    required this.name,
    required this.isDraft,
    required this.isPrerelease,
    required this.notes,
    required this.htmlUrl,
    required this.assets,
  });

  final String tag;

  /// The version without any leading `v` (e.g. `2.0.0`).
  final String version;
  final String name;
  final bool isDraft;
  final bool isPrerelease;
  final String notes;
  final String htmlUrl;
  final List<UpdateAsset> assets;

  /// Parses a GitHub release payload (works for both `/releases` and
  /// `/releases/latest`).
  factory AppRelease.fromJson(Map<String, dynamic> json) {
    final tag = (json['tag_name'] as String? ?? '').trim();
    final assets = <UpdateAsset>[];
    for (final a in (json['assets'] as List? ?? const [])) {
      final m = a as Map<String, dynamic>;
      assets.add(UpdateAsset(
        name: m['name'] as String? ?? '',
        url: m['browser_download_url'] as String? ?? '',
        size: (m['size'] as num?)?.toInt() ?? 0,
      ));
    }
    return AppRelease(
      tag: tag,
      version: tag.startsWith('v') ? tag.substring(1) : tag,
      name: (json['name'] as String? ?? '').trim(),
      isDraft: json['draft'] as bool? ?? false,
      isPrerelease: json['prerelease'] as bool? ?? false,
      notes: json['body'] as String? ?? '',
      htmlUrl: json['html_url'] as String? ?? '',
      assets: assets,
    );
  }

  UpdateAsset? assetNamed(String name) {
    for (final a in assets) {
      if (a.name == name) return a;
    }
    return null;
  }

  /// The firmware image attached to this release, if any.
  UpdateAsset? get firmwareAsset => assetNamed(kFirmwareAsset);

  /// The asset that matches the current platform, if any.
  UpdateAsset? get platformAsset {
    if (Platform.isAndroid) return assetNamed('Nexo-android-release.apk');
    if (Platform.isMacOS) return assetNamed('Nexo-macos-release.zip');
    return null;
  }
}

/// Talks to the GitHub Releases API and downloads release assets.
class Updater {
  Updater({HttpClient? client}) : _client = client ?? HttpClient();

  final HttpClient _client;

  static const Map<String, String> _headers = {
    'Accept': 'application/vnd.github+json',
    'X-GitHub-Api-Version': '2022-11-28',
  };

  /// Fetches the newest published (non-draft, non-prerelease) release.
  Future<AppRelease> fetchLatest() async {
    final req = await _client.getUrl(
      Uri.parse('https://api.github.com/repos/$kUpdateRepo/releases/latest'),
    );
    _applyHeaders(req);

    final res = await req.close();
    final body = await res.transform(utf8.decoder).join();
    if (res.statusCode != 200) {
      throw HttpException('GitHub returned ${res.statusCode}');
    }
    final json = jsonDecode(body) as Map<String, dynamic>;
    return AppRelease.fromJson(json);
  }

  /// Fetches published releases, newest first, skipping drafts and prereleases.
  Future<List<AppRelease>> fetchReleases({int perPage = 30}) async {
    final req = await _client.getUrl(Uri.parse(
      'https://api.github.com/repos/$kUpdateRepo/releases?per_page=$perPage',
    ));
    _applyHeaders(req);

    final res = await req.close();
    final body = await res.transform(utf8.decoder).join();
    if (res.statusCode != 200) {
      throw HttpException('GitHub returned ${res.statusCode}');
    }
    final list = jsonDecode(body) as List;
    return list
        .map((e) => AppRelease.fromJson(e as Map<String, dynamic>))
        .where((r) => !r.isDraft && !r.isPrerelease)
        .toList();
  }

  void _applyHeaders(HttpClientRequest req) {
    req.headers.set(HttpHeaders.acceptHeader, _headers['Accept']!);
    req.headers.set('X-GitHub-Api-Version', _headers['X-GitHub-Api-Version']!);
    req.headers.set(HttpHeaders.userAgentHeader, 'Nexo-Updater');
  }

  /// Reads the running app's own version (e.g. `2.0.0`).
  static Version? parseVersion(String version) {
    try {
      return Version.parse(version);
    } catch (_) {
      return null;
    }
  }

  /// Returns the available release if it is strictly newer than [current], else
  /// null.
  Future<AppRelease?> checkForUpdate(String current) async {
    final latest = await fetchLatest();
    final cur = parseVersion(current);
    final next = parseVersion(latest.version);
    if (cur == null || next == null) return null;
    return next > cur ? latest : null;
  }

  /// Streams [asset] to [dest], reporting progress in the range 0..1.
  Future<void> download(
    UpdateAsset asset,
    File dest, {
    void Function(double progress)? onProgress,
  }) async {
    final req = await _client.getUrl(Uri.parse(asset.url));
    req.headers.set(HttpHeaders.userAgentHeader, 'Nexo-Updater');
    final res = await req.close();
    if (res.statusCode != 200) {
      throw HttpException('Download failed (${res.statusCode})');
    }
    final total = res.contentLength > 0 ? res.contentLength : asset.size;
    final sink = dest.openWrite();
    var received = 0;
    try {
      await for (final chunk in res) {
        sink.add(chunk);
        received += chunk.length;
        if (total > 0) onProgress?.call((received / total).clamp(0.0, 1.0));
      }
    } finally {
      await sink.flush();
      await sink.close();
    }
  }

  void close() => _client.close(force: true);
}
