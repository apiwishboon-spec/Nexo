// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_sync.h"

#include <algorithm>

namespace nexo {

namespace {
struct ChangeItem {
    uint8_t kind;  // 1 note, 2 homework
    uint32_t updated;
    size_t index;
};

// Same reasoning as kPageByteBudget in nexo_service.cpp: a page of 20 maximum
// sized notes is far larger than a protocol message, so pages are bounded by
// bytes as well as by record count.
constexpr size_t kPageByteBudget = 3500;

size_t changeItemSize(const ChangeItem& it, const std::vector<Note>& notes,
                      const std::vector<Homework>& hw)
{
    // +1 for the kind byte in SyncDelta::kinds.
    return 1 + (it.kind == 1 ? encodedNoteSize(notes[it.index])
                             : encodedHomeworkSize(hw[it.index]));
}
}  // namespace

SyncDelta SyncEngine::buildDelta(uint32_t since, uint8_t cursor, size_t pageSize)
{
    SyncDelta delta;
    std::vector<ChangeItem> items;
    const auto& notes = repo_.notes();
    for (size_t i = 0; i < notes.size(); ++i) {
        if (notes[i].updated >= since) items.push_back({1, notes[i].updated, i});
    }
    const auto& hw = repo_.homework();
    for (size_t i = 0; i < hw.size(); ++i) {
        if (hw[i].updated >= since) items.push_back({2, hw[i].updated, i});
    }
    std::sort(items.begin(), items.end(), [](const ChangeItem& a, const ChangeItem& b) {
        if (a.updated != b.updated) return a.updated < b.updated;
        if (a.kind != b.kind) return a.kind < b.kind;
        return a.index < b.index;
    });

    // Page boundaries are deterministic for a given data set: walk pageSize and
    // the byte budget page by page until the requested cursor is reached. The
    // client advances the cursor by one per response, so both sides agree on
    // the layout without sending offsets over the wire.
    size_t start = 0;
    for (uint8_t p = 0; p < cursor && start < items.size(); ++p) {
        size_t used = 0, count = 0;
        while (start < items.size() && count < pageSize) {
            const size_t size = changeItemSize(items[start], notes, hw);
            if (count > 0 && used + size > kPageByteBudget) break;
            used += size;
            ++count;
            ++start;
        }
    }

    size_t used = 0, count = 0;
    while (start + count < items.size() && count < pageSize) {
        const size_t size = changeItemSize(items[start + count], notes, hw);
        if (count > 0 && used + size > kPageByteBudget) break;
        used += size;
        ++count;
    }
    const size_t end = start + count;

    for (size_t i = start; i < end; ++i) {
        const ChangeItem& it = items[i];
        delta.kinds.push_back(it.kind);
        if (it.kind == 1) {
            delta.notes.push_back(notes[it.index]);
        } else {
            delta.homework.push_back(hw[it.index]);
        }
    }
    delta.more = end < items.size();
    return delta;
}

SyncResult SyncEngine::applyPush(const SyncPush& push, uint32_t serverEpoch)
{
    SyncResult res;
    res.serverEpoch = serverEpoch;

    size_t ni = 0, hi = 0;
    for (uint8_t kind : push.kinds) {
        if (kind == 1 && ni < push.notes.size()) {
            const Note& in = push.notes[ni++];
            Note stored;
            RepoStatus st = repo_.putNote(in, in.rev, stored);
            if (st == RepoStatus::Conflict) {
                res.conflicts++;
                ConflictEntry e;
                e.kind = 1;
                e.note = stored;
                res.entries.push_back(e);
            } else if (st == RepoStatus::Ok) {
                res.applied++;
            } else {
                res.conflicts++;
                ConflictEntry e;
                e.kind = 1;
                e.note = stored;
                res.entries.push_back(e);
            }
        } else if (kind == 2 && hi < push.homework.size()) {
            const Homework& in = push.homework[hi++];
            Homework stored;
            RepoStatus st = repo_.putHomework(in, in.rev, stored);
            if (st == RepoStatus::Conflict) {
                res.conflicts++;
                ConflictEntry e;
                e.kind = 2;
                e.homework = stored;
                res.entries.push_back(e);
            } else if (st == RepoStatus::Ok) {
                res.applied++;
            } else {
                res.conflicts++;
                ConflictEntry e;
                e.kind = 2;
                e.homework = stored;
                res.entries.push_back(e);
            }
        }
    }
    return res;
}

}  // namespace nexo
