// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter_test/flutter_test.dart';
import 'package:nexo_app/models/models.dart';
import 'package:nexo_app/state/sync_batching.dart';

Note _note(String id, {int body = 0}) =>
    Note(id: id, title: 't', body: 'x' * body, rev: 1, dirty: true);

void main() {
  test('batchSyncPush splits long records into frames that fit', () {
    final notes = List.generate(8, (i) => _note('000000000000000$i', body: 1000));
    final batches = batchSyncPush(notes, const []);

    expect(batches.length, 4);
    expect(batches.expand((b) => b.notes).length, 8);
    for (final b in batches) {
      expect(b.notes.length, 2);
    }
  });

  test('batchSyncPush keeps one oversized record in its own batch', () {
    final notes = [_note('0000000000000001', body: 4000), _note('0000000000000002')];
    final batches = batchSyncPush(notes, const []);

    expect(batches.length, 2);
    expect(batches.first.notes.single.id, '0000000000000001');
    expect(batches.last.notes.single.id, '0000000000000002');
  });

  test('batchSyncPush preserves empty input', () {
    expect(batchSyncPush(const [], const []), isEmpty);
  });
}
