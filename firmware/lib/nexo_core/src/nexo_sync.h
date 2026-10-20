// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Synchronization engine: builds pull deltas and applies pushes with conflict
// detection. See docs/PROTOCOL.md §7.
#include <cstdint>

#include "nexo_codec.h"
#include "nexo_repository.h"

namespace nexo {

class SyncEngine {
public:
    explicit SyncEngine(Repository& repo) : repo_(repo) {}

    // Returns records (incl. tombstones) with updated >= since, sorted by
    // updated, split into deterministic pages using `pageSize` and a byte
    // budget; `cursor` selects the page number.
    SyncDelta buildDelta(uint32_t since, uint8_t cursor, size_t pageSize);

    // Applies a client push. Records carry baseRev in their rev field.
    SyncResult applyPush(const SyncPush& push, uint32_t serverEpoch);

private:
    Repository& repo_;
};

}  // namespace nexo
