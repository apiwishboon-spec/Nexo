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

    size_t start = static_cast<size_t>(cursor) * pageSize;
    size_t end = std::min(items.size(), start + pageSize);
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
