// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter_test/flutter_test.dart';
import 'package:nexo_app/services/updater.dart';

/// A release payload shaped like GitHub's `releases/latest` response.
Map<String, dynamic> _payload({String tag = 'v2.1.0'}) => {
      'tag_name': tag,
      'body': 'Fixed things.\n\n- one\n- two',
      'html_url': 'https://github.com/apiwishboon-spec/Nexo/releases/tag/$tag',
      'assets': [
        {
          'name': 'Nexo-android-release.apk',
          'browser_download_url': 'https://example.com/app.apk',
          'size': 1234,
        },
        {
          'name': 'Nexo-macos-release.zip',
          'browser_download_url': 'https://example.com/app.zip',
          'size': 5678,
        },
        {
          'name': 'SHA256SUMS',
          'browser_download_url': 'https://example.com/sums',
          'size': 90,
        },
      ],
    };

/// An [Updater] that returns a canned release instead of hitting the network.
class _FakeUpdater extends Updater {
  _FakeUpdater(this.release);

  final AppRelease? release;

  @override
  Future<AppRelease> fetchLatest() async {
    if (release == null) throw Exception('no release');
    return release!;
  }
}

void main() {
  group('AppRelease.fromJson', () {
    test('strips the leading v from the tag', () {
      final r = AppRelease.fromJson(_payload());
      expect(r.tag, 'v2.1.0');
      expect(r.version, '2.1.0');
    });

    test('keeps a tag without a leading v', () {
      final r = AppRelease.fromJson(_payload(tag: '3.0.0'));
      expect(r.version, '3.0.0');
    });

    test('parses assets with names, urls and sizes', () {
      final r = AppRelease.fromJson(_payload());
      expect(r.assets.length, 3);
      final apk = r.assetNamed('Nexo-android-release.apk');
      expect(apk, isNotNull);
      expect(apk!.url, 'https://example.com/app.apk');
      expect(apk.size, 1234);
      expect(r.assetNamed('nope'), isNull);
    });

    test('tolerates a payload with no assets', () {
      final r = AppRelease.fromJson({'tag_name': 'v1.0.0'});
      expect(r.version, '1.0.0');
      expect(r.assets, isEmpty);
      expect(r.notes, '');
    });

    test('platformAsset matches the running host', () {
      final r = AppRelease.fromJson(_payload());
      // These tests run on the macOS host, so the macOS asset is chosen.
      expect(r.platformAsset?.name, 'Nexo-macos-release.zip');
    });
  });

  group('Updater.parseVersion', () {
    test('parses plain and dotted versions', () {
      expect(Updater.parseVersion('2.0.0')?.major, 2);
      expect(Updater.parseVersion('2.1.3')?.patch, 3);
    });

    test('returns null for non-semver input', () {
      expect(Updater.parseVersion('v2.0.0'), isNull); // 'v' prefix not parsed
      expect(Updater.parseVersion('latest'), isNull);
      expect(Updater.parseVersion(''), isNull);
    });
  });

  group('Updater.checkForUpdate', () {
    test('returns the release when it is newer', () async {
      final u = _FakeUpdater(AppRelease.fromJson(_payload(tag: 'v2.1.0')));
      final r = await u.checkForUpdate('2.0.0');
      expect(r?.version, '2.1.0');
    });

    test('returns null when versions are equal', () async {
      final u = _FakeUpdater(AppRelease.fromJson(_payload(tag: 'v2.0.0')));
      expect(await u.checkForUpdate('2.0.0'), isNull);
    });

    test('returns null when the running version is newer', () async {
      final u = _FakeUpdater(AppRelease.fromJson(_payload(tag: 'v2.0.0')));
      expect(await u.checkForUpdate('2.5.0'), isNull);
    });

    test('returns null when the current version is unparseable', () async {
      final u = _FakeUpdater(AppRelease.fromJson(_payload(tag: 'v2.1.0')));
      expect(await u.checkForUpdate('dev'), isNull);
    });

    test('propagates fetch errors', () async {
      final u = _FakeUpdater(null);
      expect(() => u.checkForUpdate('2.0.0'), throwsException);
    });
  });
}
