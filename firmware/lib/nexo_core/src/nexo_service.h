// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Protocol dispatcher: maps request Messages to responses. Pure logic, no BLE.
#include <deque>
#include <functional>
#include <string>

#include "nexo_device.h"
#include "nexo_frame.h"
#include "nexo_protocol.h"
#include "nexo_repository.h"
#include "nexo_sync.h"

namespace nexo {

class NexoService {
public:
    NexoService(Repository& repo, SyncEngine& sync, std::function<uint32_t()> now,
                DeviceHooks* hooks = nullptr);

    // Handles a request and produces a response Message (type | 0x80).
    Message handle(const Message& req);

    bool authenticated() const { return authenticated_; }
    void resetAuth() { authenticated_ = false; }

    // Called on disconnect.
    void onDisconnect() { authenticated_ = false; }

private:
    Message makeResponse(const Message& req, uint8_t status, const std::string& payload = "");
    bool checkAuthLocked(uint32_t now);
    void registerFailure(uint32_t now);

    Repository& repo_;
    SyncEngine& sync_;
    std::function<uint32_t()> now_;
    DeviceHooks* hooks_ = nullptr;

    bool authenticated_ = false;
    uint8_t failCount_ = 0;
    uint32_t lockUntil_ = 0;

    // Duplicate-request protection: last N (type,id) -> serialized response.
    struct Cached {
        uint8_t type;
        uint16_t id;
        std::string response;
    };
    std::deque<Cached> cache_;
};

}  // namespace nexo
