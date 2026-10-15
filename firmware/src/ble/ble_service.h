// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// BLE GATT peripheral exposing the Nexo protocol (docs/PROTOCOL.md §1-2).
//
// Threading model: NimBLE invokes the GATT callbacks (onWrite/onConnect/
// onDisconnect) from its own host task. To keep the repository, LittleFS and the
// reassembler single-threaded, those callbacks only copy data into a small
// lock-protected queue / set flags; all real work (reassembly, service handling,
// flash persistence and notifications) runs from loop() on the Arduino task.
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>

#include <deque>
#include <map>
#include <string>

#include "app/nexo_app.h"
#include "nexo_frame.h"

namespace nexo {

struct NexoUuids {
    static const char* service() { return "6e65786f-0001-4e58-8000-4e65786f0000"; }
    static const char* rx() { return "6e65786f-0002-4e58-8000-4e65786f0000"; }
    static const char* tx() { return "6e65786f-0003-4e58-8000-4e65786f0000"; }
    static const char* status() { return "6e65786f-0004-4e58-8000-4e65786f0000"; }
};

// Multi-client peripheral: several centrals (e.g. a Mac and an Android phone)
// may stay connected at once. Each connection keeps its own reassembler and MTU;
// responses are broadcast on the TX characteristic and clients correlate them by
// request id (unknown ids are ignored). Authentication is shared across links and
// only cleared when the last central disconnects.
class BleService : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks {
public:
    void begin(NexoApp* app, const std::string& deviceName);
    void loop();  // drains pending writes, flushes TX and status (main task only)

    bool connected() const { return connCount_ > 0; }

    // Requests a status-characteristic refresh. Safe to call from any task; the
    // actual notify happens from loop().
    void notifyDataChanged();
    void updateStatus();

    // One-shot UI events, set while handling a request in loop() (Arduino task)
    // and consumed by the UI on the same task. Each returns true at most once.
    bool consumePairedEvent();
    bool consumeSyncEvent();

    // Lab tool: rotate the advertised name through rickroll lines. Only used
    // from the hidden "hack" menu.
    void startSpam();
    void stopSpam();
    bool spamActive() const { return spamActive_; }

private:
    void spamTick();
    void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override;
    void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) override;
    void onWrite(NimBLECharacteristic* chr, ble_gap_conn_desc* desc) override;

    void drainWrites();
    void flushTx();
    void refreshStatus();
    void enqueueMessage(const Message& m, uint16_t mtu);
    uint16_t mtuPayload(uint16_t conn) const;

    static constexpr size_t kWriteSlots = 8;
    struct PendingWrite {
        uint16_t conn;
        uint16_t len;
        uint8_t data[kMtuCap + 4];
    };

    NexoApp* app_ = nullptr;
    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* tx_ = nullptr;
    NimBLECharacteristic* status_ = nullptr;
    // One reassembler per connection, keyed by connection handle.
    std::map<uint16_t, Reassembler> reassemblers_;
    std::deque<std::string> outQueue_;

    // Cross-task hand-off (BLE host task -> loop task).
    PendingWrite pending_[kWriteSlots] = {};
    size_t pwHead_ = 0;
    size_t pwTail_ = 0;
    size_t pwCount_ = 0;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    volatile uint8_t connCount_ = 0;
    volatile bool pendingDisconnect_ = false;
    volatile bool advDirty_ = false;
    bool statusDirty_ = true;
    bool pairedEvent_ = false;
    bool syncEvent_ = false;
    uint32_t changeSeq_ = 0;
    uint32_t lastStatusMs_ = 0;
    std::string peerAddr_;
    std::string deviceName_;
    bool spamActive_ = false;
    int spamIndex_ = 0;
    uint32_t spamNext_ = 0;
};

}  // namespace nexo
