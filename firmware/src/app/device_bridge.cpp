// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "device_bridge.h"

#include <LittleFS.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <esp_mac.h>

#include "hw/audio.h"

namespace nexo {

void DeviceBridge::begin(NexoApp* app, WifiAccess* wifi)
{
    app_ = app;
    wifi_ = wifi;
    if (app_) {
        Settings& s = app_->settings();
        audioSetVolume(s.volume);
        audioSetEnabled(s.soundEnabled);
        musicSetAutoAdvance(s.autoAdvanceMusic);
        uint8_t b = s.brightness > 100 ? 100 : s.brightness;
        M5Cardputer.Display.setBrightness((uint8_t)((uint16_t)b * 255u / 100u));
    }
}

DeviceInfo DeviceBridge::info()
{
    DeviceInfo d;
    d.protocolVersion = kProtocolVersion;
    d.firmwareVersion = kNexoVersion;

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    d.mac = macStr;

    if (app_) {
        d.deviceName = app_->settings().deviceName;
        d.apSsid = app_->settings().apSsid;
        d.volume = app_->settings().volume;
        d.soundEnabled = app_->settings().soundEnabled;
        d.apEnabled = app_->settings().apEnabled;
        d.screenTimeoutSec = app_->settings().screenTimeoutSec;
    }

    int32_t bat = M5.Power.getBatteryLevel();
    d.batteryPercent = (bat < 0) ? 0xFF : (uint8_t)(bat > 100 ? 100 : bat);
    d.charging = (bool)M5.Power.isCharging();

    d.uptimeSec = millis() / 1000u;
    d.freeHeapKiB = ESP.getFreeHeap() / 1024u;
    d.musicCount = (uint16_t)musicCount();
    d.headphoneIn = (digitalRead(17) == LOW);

    if (SD.totalBytes() > 0) {
        d.sdPresent = true;
        d.sdTotalMb = (uint32_t)(SD.totalBytes() / (1024u * 1024u));
        d.sdFreeMb = (uint32_t)((SD.totalBytes() - SD.usedBytes()) / (1024u * 1024u));
    }

    if (wifi_) {
        d.ip = std::string(wifi_->ipString().c_str());
        d.apPassword = wifi_->apPassword();
    }
    return d;
}

void DeviceBridge::onSettingsChanged(const Settings& s)
{
    audioSetVolume(s.volume);
    audioSetEnabled(s.soundEnabled);
    musicSetAutoAdvance(s.autoAdvanceMusic);

    uint8_t b = s.brightness > 100 ? 100 : s.brightness;
    M5Cardputer.Display.setBrightness((uint8_t)((uint16_t)b * 255u / 100u));

    if (wifi_) wifi_->setEnabled(s.apEnabled);
}

void DeviceBridge::setTime(uint32_t epoch, int16_t tzOffsetMin)
{
    if (!app_) return;
    app_->clock().setEpoch(epoch);
    app_->settings().timezoneOffsetMin = tzOffsetMin;
    app_->repo().persistSettings();
}

void DeviceBridge::reboot() { ESP.restart(); }

void DeviceBridge::factoryReset()
{
    LittleFS.remove("/settings.json");
    LittleFS.remove("/notes.json");
    LittleFS.remove("/homework.json");
    Preferences p;
    p.begin("nexo", false);
    p.clear();
    p.end();
    delay(200);
    ESP.restart();
}

void DeviceBridge::find()
{
    findUntil_ = millis() + 6000;
    lastBeep_ = 0;  // beep immediately on next tick
}

void DeviceBridge::tick()
{
    if (findUntil_ == 0) return;
    uint32_t now = millis();
    if (now > findUntil_) {
        findUntil_ = 0;
        return;
    }
    if (now - lastBeep_ >= 500) {
        lastBeep_ = now;
        audioBeep(1200, 120);
    }
}

std::vector<MusicTrack> DeviceBridge::listMusic()
{
    std::vector<MusicTrack> out;
    int n = musicCount();
    out.reserve(n > 0 ? n : 0);
    for (int i = 0; i < n; ++i) {
        MusicTrack t;
        t.name = musicTitle(i);
        t.size = musicTrackSize(i);
        out.push_back(std::move(t));
    }
    return out;
}

MusicStatus DeviceBridge::nowPlaying()
{
    MusicStatus st;
    st.index = (int16_t)musicIndex();
    st.playing = musicPlaying();
    st.positionSec = musicPositionSec();
    st.durationSec = musicDurationSec();
    st.volume = audioVolume();
    return st;
}

RepoStatus DeviceBridge::musicControl(MusicAction action, int16_t index, uint16_t arg)
{
    switch (action) {
        case MUSIC_TOGGLE:
            musicTogglePlay();
            return RepoStatus::Ok;
        case MUSIC_PLAY_INDEX:
            if (index < 0 || index >= musicCount()) return RepoStatus::NotFound;
            musicPlay(index);
            return RepoStatus::Ok;
        case MUSIC_NEXT:
            musicNext();
            return RepoStatus::Ok;
        case MUSIC_PREV:
            musicPrev();
            return RepoStatus::Ok;
        case MUSIC_STOP:
            musicStop();
            return RepoStatus::Ok;
        case MUSIC_SEEK:
            musicSeek(arg);
            return RepoStatus::Ok;
        case MUSIC_SET_VOLUME:
            audioSetVolume((uint8_t)(arg > 100 ? 100 : arg));
            return RepoStatus::Ok;
        default:
            return RepoStatus::BadRequest;
    }
}

RepoStatus DeviceBridge::deleteMusic(const std::string& name)
{
    if (name.empty()) return RepoStatus::BadRequest;
    return musicDelete(name.c_str()) ? RepoStatus::Ok : RepoStatus::NotFound;
}

}  // namespace nexo
