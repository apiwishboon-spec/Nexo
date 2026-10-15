// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
// Nexo Cardputer firmware entry point.
#include <M5Cardputer.h>
#include <esp_mac.h>

#include "app/device_bridge.h"
#include "app/nexo_app.h"
#include "ble/ble_service.h"
#include "hw/audio.h"
#include "ui/ui.h"
#include "wifi/wifi_access.h"

using namespace nexo;

namespace {
NexoApp gApp;
BleService gBle;
WifiAccess gWifi;
Ui gUi;
DeviceBridge gBridge;

std::string advertisingName()
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    char buf[24];
    snprintf(buf, sizeof(buf), "%s-%02X%02X", gApp.settings().deviceName.c_str(), mac[4],
             mac[5]);
    return std::string(buf);
}
}  // namespace

void setup()
{
    auto cfg = M5.config();
    // We own I2S via the media module (ES8311 codec + ESP32-audioI2S), so keep
    // M5Unified from claiming the internal speaker/mic.
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(128);
    Serial.begin(115200);
    setCpuFrequencyMhz(160);

    Serial.println("[nexo] booting");
    gApp.setDeviceHooks(&gBridge);
    gApp.begin();
    audioBegin();
    musicBegin();
    gBridge.begin(&gApp, &gWifi);

    std::string name = advertisingName();
    gBle.begin(&gApp, name);
    Serial.printf("[nexo] BLE advertising as %s\n", name.c_str());

    gWifi.begin(&gApp);
    gWifi.setEnabled(gApp.settings().apEnabled);

    gUi.begin(&gApp, &gBle, &gWifi);
}

void loop()
{
    gApp.tick();
    gBle.loop();
    gWifi.loop();
    gUi.loop();
    audioLoop();
    gBridge.tick();
    // Feed the I2S decoder tightly while a track plays so it does not underrun.
    delay(musicPlaying() ? 1 : 10);
}
