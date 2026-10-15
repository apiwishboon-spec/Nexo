// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Application container: owns storage, clock, repository, sync engine and the
// protocol service. Shared by the UI, the BLE server and the Wi-Fi access layer.
#include <memory>

#include "hw/device_storage.h"
#include "nexo_device.h"
#include "nexo_repository.h"
#include "nexo_service.h"
#include "nexo_sync.h"
#include "timekeeper.h"

namespace nexo {

class NexoApp {
public:
    bool begin();
    void tick();  // periodic housekeeping (clock persistence, tombstones)

    // Device hardware bridge used by v2 protocol ops (info/time/music/actions).
    // Must be set before begin().
    void setDeviceHooks(DeviceHooks* hooks) { hooks_ = hooks; }

    Repository& repo() { return *repo_; }
    SyncEngine& sync() { return *sync_; }
    NexoService& service() { return *service_; }
    TimeKeeper& clock() { return clock_; }
    Settings& settings() { return repo_->settingsMutable(); }

    uint32_t now() { return clock_.now(); }

private:
    LittleFsStore store_;
    TimeKeeper clock_;
    DeviceHooks* hooks_ = nullptr;
    std::unique_ptr<Repository> repo_;
    std::unique_ptr<SyncEngine> sync_;
    std::unique_ptr<NexoService> service_;
    uint32_t lastPersist_ = 0;
    bool ready_ = false;
};

}  // namespace nexo
