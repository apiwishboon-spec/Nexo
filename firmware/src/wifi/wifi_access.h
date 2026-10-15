// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Optional local Wi-Fi access point + lightweight browser UI. Uses the same
// repository as the on-device UI and BLE (no cloud, no CDNs).
#include <FS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "app/nexo_app.h"

namespace nexo {

class WifiAccess {
public:
    void begin(NexoApp* app);
    void loop();

    void setEnabled(bool on);
    bool enabled() const { return enabled_; }
    String ipString() const;
    std::string apPassword() const { return apPassword_; }

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
};

}  // namespace nexo
