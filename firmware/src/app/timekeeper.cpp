// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "timekeeper.h"

#include <Arduino.h>

namespace nexo {

void TimeKeeper::begin()
{
    prefs_.begin("nexo", false);
    base_ = prefs_.getUInt("epoch", 1600000000u);
    baseMillis_ = millis();
    prefs_.end();
}

uint32_t TimeKeeper::now() const
{
    uint32_t elapsed = (millis() - baseMillis_) / 1000u;
    return base_ + elapsed;
}

void TimeKeeper::setEpoch(uint32_t epoch)
{
    base_ = epoch;
    baseMillis_ = millis();
    synced_ = true;
}

void TimeKeeper::persist()
{
    prefs_.begin("nexo", false);
    prefs_.putUInt("epoch", now());
    prefs_.end();
}

}  // namespace nexo
