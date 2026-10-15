// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Hardware hook interface for the protocol service. The portable core (which
// also builds on the host for tests) never touches the board directly; the
// firmware supplies these callbacks so v2 ops like GET_DEVICE_INFO, SET_TIME,
// REBOOT, FIND_DEVICE and MUSIC_CONTROL can reach the hardware.
#include <functional>
#include <string>
#include <vector>

#include "nexo_repository.h"
#include "nexo_types.h"

namespace nexo {

class DeviceHooks {
public:
    virtual ~DeviceHooks() = default;

    // Telemetry snapshot. Must always return a value.
    virtual DeviceInfo info() { return DeviceInfo{}; }

    // Called after a successful PUT_SETTINGS so the firmware can apply live
    // values (volume, brightness, sound) that aren't persisted-only.
    virtual void onSettingsChanged(const Settings& /*s*/) {}

    // Push an absolute epoch (device has no RTC) + timezone offset.
    virtual void setTime(uint32_t /*epoch*/, int16_t /*tzOffsetMin*/) {}

    virtual void reboot() {}
    virtual void factoryReset() {}
    virtual void find() {}  // beep / flash so the user can locate the device

    // Music library.
    virtual std::vector<MusicTrack> listMusic() { return {}; }
    virtual MusicStatus nowPlaying() { return MusicStatus{}; }
    virtual RepoStatus musicControl(MusicAction /*action*/, int16_t /*index*/, uint16_t /*arg*/)
    {
        return RepoStatus::BadRequest;
    }
    virtual RepoStatus deleteMusic(const std::string& /*name*/) { return RepoStatus::NotFound; }
};

}  // namespace nexo
