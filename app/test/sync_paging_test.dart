// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import 'package:flutter_test/flutter_test.dart';
import 'package:nexo_app/models/models.dart';
import 'package:nexo_app/protocol/records.dart';
import 'package:nexo_app/state/sync_paging.dart';

SyncDelta _delta(int epoch, bool more, List<Note> notes) =>
    (notes: notes, homework: const <Homework>[], serverEpoch: epoch, more: more);

void main() {
  test('pullAllPages keeps the same since and fetches every page', () async {
    final calls = <int>[];
    final applied = <String>[];
    var pages = 0;

    final epoch = await pullAllPages(
      since: 7,
      fetch: (since, cursor) async {
        calls.add(cursor);
        expect(since, 7, reason: 'since must not change between pages');
        pages++;
        if (cursor == 0) {
          return _delta(1000, true, [Note(id: 'a')]);
        }
        if (cursor == 1) {
          return _delta(1001, true, [Note(id: 'b')]);
        }
        return _delta(1002, false, [Note(id: 'c')]);
      },
      apply: (d) => applied.addAll(d.notes.map((n) => n.id)),
    );

    expect(calls, [0, 1, 2]);
    expect(pages, 3);
    expect(applied, ['a', 'b', 'c']);
    expect(epoch, 1000, reason: 'keep the first snapshot for the next sync');
  });

  test('pullAllPages stops after a single page when there is no more', () async {
    final applied = <String>[];
    final epoch = await pullAllPages(
      since: 0,
      fetch: (since, cursor) async => _delta(500, false, [Note(id: 'x')]),
      apply: (d) => applied.addAll(d.notes.map((n) => n.id)),
    );

    expect(applied, ['x']);
    expect(epoch, 500);
  });
}
