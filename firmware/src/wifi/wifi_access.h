// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Optional local Wi-Fi access point + lightweight browser UI. Uses the same
// repository as the on-device UI and BLE (no cloud, no CDNs).
#include <FS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>
#include <vector>

#include "app/nexo_app.h"

namespace nexo {

class WifiAccess {
public:
    // One nearby network from a scan (Lab tools).
    struct ApInfo {
        std::string ssid;
        int8_t rssi = 0;
        uint8_t channel = 0;
        bool secure = false;
    };

    void begin(NexoApp* app);
    void loop();

    void setEnabled(bool on);
    bool enabled() const { return enabled_; }
    String ipString() const;
    std::string apPassword() const { return apPassword_; }

    // ---- Lab tools (hidden "hack" menu) ------------------------------------
    // Async AP scan. Poll scanRunning() until false, then read scanResults().
    void scanStart();
    bool scanRunning() const { return scanning_; }
    const std::vector<ApInfo>& scanResults() const { return scanResults_; }

    // Fake-beacon broadcast using rickroll SSIDs. loop() drives the frame pump.
    void startBeaconSpam();
    void stopBeaconSpam();
    bool beaconSpamActive() const { return spamming_; }
    uint32_t spamFrames() const { return spamFrames_; }

private:
    void applyEnabled();
    void setupRoutes();
    bool requireAuth();
    bool tokenMatches();
    void sendJson(int code, const std::string& body);
    void handleRoot();
    void handleLogin();
    void handleInfo();
    void handleNotesList();
    void handleNoteUpsert();
    void handleNoteDelete();
    void handleHomeworkList();
    void handleHomeworkUpsert();
    void handleHomeworkDelete();

    // Music over Wi-Fi (metadata + control mirror the BLE ops; bulk file
    // transfer only happens here).
    void handleMusicList();
    void handleMusicNow();
    void handleMusicControl();
    void handleMusicDelete();
    void handleMusicDownload();
    void handleMusicUploadDone();
    void handleMusicUploadStream();

    // Voice memos (list + download the 16 kHz WAVs from the SD card).
    void handleMemoList();
    void handleMemoDownload();
    void handleMemoDelete();

    // Over-the-air firmware update (raw image streamed to the OTA partition).
    void handleOtaDone();
    void handleOtaStream();

    NexoApp* app_ = nullptr;
    WebServer* server_ = nullptr;
    bool enabled_ = false;
    bool started_ = false;
    std::string token_;
    std::string apPassword_;

    // In-flight music upload state.
    File uploadFile_;
    bool uploadAuth_ = false;
    bool uploadOk_ = false;
    String uploadName_;

    // In-flight OTA state.
    bool otaAuth_ = false;
    bool otaStarted_ = false;
    bool otaOk_ = false;
    size_t otaWritten_ = 0;

    // Lab: scan + beacon spam state.
    void labTick();
    void sendBeacon();

    bool scanning_ = false;
    bool scanRestoreAp_ = false;
    std::vector<ApInfo> scanResults_;

    bool spamming_ = false;
    uint32_t spamFrames_ = 0;
    uint32_t spamNext_ = 0;
    int spamSsidIndex_ = 0;
    uint8_t spamChannel_ = 1;
    uint8_t spamBssid_[6] = {0};
};

}  // namespace nexo
