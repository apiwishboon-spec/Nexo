// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import '../protocol/records.dart';

/// Fetches every page of a sync delta.
///
/// [since] is a fixed lower bound for the whole pull: every page must be
/// requested with the same value, otherwise the server-side result set shrinks
/// after the first response and later pages are skipped. The returned value is
/// the epoch of the *first* response, so records changed while paging are
/// fetched again on the next sync instead of being lost.
Future<int> pullAllPages({
  required int since,
  required Future<SyncDelta> Function(int since, int cursor) fetch,
  required void Function(SyncDelta delta) apply,
  int maxPages = 1000,
}) async {
  var cursor = 0;
  var epoch = since;
  var first = true;
  while (true) {
    final delta = await fetch(since, cursor);
    if (first) {
      epoch = delta.serverEpoch;
      first = false;
    }
    apply(delta);
    if (!delta.more) break;
    cursor++;
    if (cursor >= maxPages) break;
  }
  return epoch;
}
