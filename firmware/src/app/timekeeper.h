// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Monotonic best-effort epoch clock. The Cardputer has no RTC; without Wi-Fi/NTP
// we keep a persisted base epoch and add elapsed uptime so `updated` timestamps
// stay monotonic and comparable across reboots.
#include <Preferences.h>

#include <cstdint>

namespace nexo {

class TimeKeeper {
public:
    void begin();
    uint32_t now() const;
    // Overrides the clock with a network/real epoch (e.g. after NTP).
    void setEpoch(uint32_t epoch);
    bool synced() const { return synced_; }
    // Persists the current value; call periodically.
    void persist();

private:
    Preferences prefs_;
    uint32_t base_ = 1600000000u;  // 2020-09-13 fallback
    uint32_t baseMillis_ = 0;
    bool synced_ = false;
};

}  // namespace nexo
