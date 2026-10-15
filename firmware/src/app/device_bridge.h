// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Bridges the portable DeviceHooks interface to the Cardputer hardware: battery,
// storage, Wi-Fi, the media module and device actions. Owned by main.cpp.
#include "app/nexo_app.h"
#include "nexo_device.h"
#include "wifi/wifi_access.h"

namespace nexo {

class DeviceBridge : public DeviceHooks {
public:
    void begin(NexoApp* app, WifiAccess* wifi);

    DeviceInfo info() override;
    void onSettingsChanged(const Settings& s) override;
    void setTime(uint32_t epoch, int16_t tzOffsetMin) override;
    void reboot() override;
    void factoryReset() override;
    void find() override;

    std::vector<MusicTrack> listMusic() override;
    MusicStatus nowPlaying() override;
    RepoStatus musicControl(MusicAction action, int16_t index, uint16_t arg) override;
    RepoStatus deleteMusic(const std::string& name) override;

    // Polled from the main loop; drives the repeating "find device" beacon.
    void tick();

private:
    NexoApp* app_ = nullptr;
    WifiAccess* wifi_ = nullptr;
    uint32_t findUntil_ = 0;
    uint32_t lastBeep_ = 0;
};

}  // namespace nexo
