// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_app.h"

#include <Arduino.h>
#include <esp_random.h>

#include "nexo_types.h"

namespace nexo {

static std::string randomId()
{
    uint8_t bytes[8];
    for (int i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(esp_random() & 0xFF);
    return idFromBytes(bytes);
}

bool NexoApp::begin()
{
    store_.beginMount();
    clock_.begin();

    repo_ = std::make_unique<Repository>(store_, [this]() { return clock_.now(); },
                                          []() { return randomId(); });
    repo_->begin();
    sync_ = std::make_unique<SyncEngine>(*repo_);
    service_ = std::make_unique<NexoService>(*repo_, *sync_,
                                             [this]() { return clock_.now(); }, hooks_);
    lastPersist_ = millis();
    ready_ = true;
    return true;
}

void NexoApp::tick()
{
    if (!ready_) return;
    uint32_t m = millis();
    if (m - lastPersist_ > 60000u) {
        clock_.persist();
        // Prune tombstones older than 30 days.
        repo_->pruneTombstones(clock_.now(), 30u * 24u * 3600u);
        lastPersist_ = m;
    }
}

}  // namespace nexo
