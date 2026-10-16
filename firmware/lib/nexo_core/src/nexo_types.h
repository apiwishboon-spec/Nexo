// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Nexo portable core types. No Arduino dependencies: builds on device and host.
#include <cstdint>
#include <string>
#include <vector>

namespace nexo {

// ---- Limits / validation bounds -------------------------------------------
constexpr size_t kIdLen = 16;             // 16 hex chars (8 random bytes)
constexpr size_t kMaxTitleLen = 64;
constexpr size_t kMaxBodyLen = 4096;
constexpr size_t kMaxSubjectLen = 48;
constexpr size_t kMaxDescLen = 512;
constexpr size_t kMaxNameLen = 32;
constexpr size_t kMaxPinLen = 16;
constexpr size_t kMinPinLen = 4;
constexpr size_t kMaxTrackNameLen = 96;
constexpr uint16_t kProtocolVersion = 2;
constexpr const char* kNexoVersion = "2.2.8";

// Homework status
enum class HomeworkStatus : uint8_t {
    NotStarted = 0,
    InProgress = 1,
    Done = 2,
};

struct Note {
    std::string id;
    std::string title;
    std::string body;
    uint32_t created = 0;
    uint32_t updated = 0;
    uint32_t rev = 0;
    bool deleted = false;
};

struct Homework {
    std::string id;
    std::string subject;
    std::string description;
    uint32_t dueEpochDay = 0;  // days since 1970-01-01 UTC; 0 = unset
    HomeworkStatus status = HomeworkStatus::NotStarted;
    uint32_t created = 0;
    uint32_t updated = 0;
    uint32_t rev = 0;
    bool deleted = false;
};

struct Settings {
    std::string deviceName = "Nexo";
    std::string pin = "123456";
    std::string apSsid = "Nexo-AP";
    bool apEnabled = false;
    uint8_t volume = 60;               // 0..100
    bool soundEnabled = true;
    uint8_t brightness = 100;          // 0..100
    uint16_t screenTimeoutSec = 30;    // 0 = never
    bool autoAdvanceMusic = true;
    int16_t timezoneOffsetMin = 0;     // minutes east of UTC
    uint8_t protocolVersion = kProtocolVersion;
};

// Snapshot of live device telemetry for GET_DEVICE_INFO. Filled by the firmware
// (hardware) side; the portable core just serializes it.
struct DeviceInfo {
    std::string deviceName;
    std::string firmwareVersion;
    std::string mac;
    std::string apSsid;
    std::string apPassword;
    std::string ip;
    uint8_t protocolVersion = kProtocolVersion;
    uint8_t batteryPercent = 0xFF;  // 0xFF = unknown
    bool charging = false;
    uint32_t uptimeSec = 0;
    uint32_t freeHeapKiB = 0;
    bool sdPresent = false;
    uint32_t sdFreeMb = 0;
    uint32_t sdTotalMb = 0;
    uint16_t musicCount = 0;
    uint8_t volume = 0;
    bool soundEnabled = true;
    bool headphoneIn = false;
    bool apEnabled = false;
    int8_t rssi = 0;
    uint16_t screenTimeoutSec = 0;
};

// One entry in the device music library.
struct MusicTrack {
    std::string name;
    uint32_t size = 0;
};

// Live playback state.
struct MusicStatus {
    int16_t index = -1;  // -1 = nothing selected
    bool playing = false;
    uint16_t positionSec = 0;
    uint16_t durationSec = 0;
    uint8_t volume = 0;
};

enum MusicAction : uint8_t {
    MUSIC_TOGGLE = 1,
    MUSIC_PLAY_INDEX = 2,
    MUSIC_NEXT = 3,
    MUSIC_PREV = 4,
    MUSIC_STOP = 5,
    MUSIC_SEEK = 6,
    MUSIC_SET_VOLUME = 7,
};

// ---- Helpers ---------------------------------------------------------------
bool isValidId(const std::string& id);
bool isValidDueDay(uint32_t day);

// Truncates to a maximum byte length on UTF-8 boundaries.
std::string clampUtf8(const std::string& in, size_t maxBytes);

// Generates 16 hex chars using the supplied 8 random bytes (stable across host
// and device once the source of entropy is provided by the caller).
std::string idFromBytes(const uint8_t bytes[8]);

// Escapes/unescapes are handled by the JSON layer; these helpers are for the
// binary protocol string fields.
std::string trimAscii(const std::string& in);

// Civil date <-> epoch day (days since 1970-01-01 UTC). 0 = unset for due dates.
std::string dueDayToIso(uint32_t epochDay);       // returns "" for 0
uint32_t isoToDueDay(const std::string& isoDate);  // returns 0 for empty/invalid

}  // namespace nexo
