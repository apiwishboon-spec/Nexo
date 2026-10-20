// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
import '../models/models.dart';
import '../protocol/byte_io.dart';
import '../protocol/records.dart';

typedef SyncPushBatch = ({List<Note> notes, List<Homework> homework});

/// Splits dirty records into messages that fit in one protocol frame.
///
/// The firmware rejects payloads over its message limit, so a push of many
/// long notes must be sent in several requests instead of one oversized one.
List<SyncPushBatch> batchSyncPush(
  List<Note> notes,
  List<Homework> homework, {
  int maxBytes = 3000,
}) {
  final batches = <SyncPushBatch>[];
  var batchNotes = <Note>[];
  var batchHomework = <Homework>[];
  var used = 2; // record count

  void flush() {
    if (batchNotes.isEmpty && batchHomework.isEmpty) return;
    batches.add((notes: batchNotes, homework: batchHomework));
    batchNotes = <Note>[];
    batchHomework = <Homework>[];
    used = 2;
  }

  for (final n in notes) {
    final w = ByteWriter();
    encodeNote(w, n, n.rev);
    final size = 1 + w.toBytes().length; // kind byte + record
    if ((batchNotes.isNotEmpty || batchHomework.isNotEmpty) &&
        used + size > maxBytes) {
      flush();
    }
    batchNotes.add(n);
    used += size;
  }

  for (final h in homework) {
    final w = ByteWriter();
    encodeHomework(w, h, h.rev);
    final size = 1 + w.toBytes().length;
    if ((batchNotes.isNotEmpty || batchHomework.isNotEmpty) &&
        used + size > maxBytes) {
      flush();
    }
    batchHomework.add(h);
    used += size;
  }

  flush();
  return batches;
}
