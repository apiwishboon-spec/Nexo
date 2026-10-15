// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// BLE chunk framing / reassembly (see docs/PROTOCOL.md §2).
#include <cstdint>
#include <string>
#include <vector>

namespace nexo {

constexpr uint8_t kChunkHeaderLen = 8;
constexpr size_t kMaxMessage = 4096;
constexpr uint16_t kMtuCap = 244;

struct Message {
    uint8_t version = 1;
    uint8_t type = 0;
    uint16_t id = 0;
    std::string payload;
};

// Splits a message into <= mtuPayload-byte chunks (each begins with the header).
bool encodeMessage(const Message& msg, size_t mtuPayload, std::vector<std::string>& out);

// Reassembles chunks coming from one peer. Keyed by (type,id).
class Reassembler {
public:
    // Feed one received BLE packet. Returns true and fills `out` when a complete
    // message has been assembled. Returns false otherwise (including errors,
    // which reset the slot and set lastError()).
    bool feed(const uint8_t* data, size_t len, Message& out);

    uint8_t lastError() const { return lastError_; }

private:
    struct Slot {
        bool active = false;
        uint8_t type = 0;
        uint16_t id = 0;
        uint16_t total = 0;
        uint16_t filled = 0;  // bytes received so far (in-order)
        std::string buf;
        uint64_t lastSeen = 0;
    };
    static constexpr int kSlots = 8;
    Slot slots_[kSlots];
    uint64_t clock_ = 0;
    uint8_t lastError_ = 0;
};

}  // namespace nexo
