// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "ble_service.h"

#include <Arduino.h>

#include <cstring>

#include "nexo_protocol.h"

namespace nexo {

// Lab-only: rotating advertised names for the BLE rickroll spammer.
static const char* const kBleSpamNames[] = {
    "Never Gonna Give You Up", "Rickroll",     "Never Gonna Let You Down",
    "Get Rickrolled",          "And Desert You", "Never Gonna Run Around",
};
static const int kBleSpamNameCount = (int)(sizeof(kBleSpamNames) / sizeof(kBleSpamNames[0]));

static bool isMutatingOp(uint8_t op)
{
    switch (op) {
        case OP_PUT_NOTE:
        case OP_DELETE_NOTE:
        case OP_PUT_HOMEWORK:
        case OP_DELETE_HOMEWORK:
        case OP_SYNC_PUSH:
            return true;
        default:
            return false;
    }
}

void BleService::begin(NexoApp* app, const std::string& deviceName)
{
    app_ = app;
    deviceName_ = deviceName;
    NimBLEDevice::init(deviceName);
    // P6 (+6 dBm) instead of P9: lower peak current avoids brown-out resets on
    // battery while still giving ample range for a phone on the same desk.
    NimBLEDevice::setPower(ESP_PWR_LVL_P6);
    NimBLEDevice::setMTU(247);

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(this);

    NimBLEService* svc = server_->createService(NexoUuids::service());

    NimBLECharacteristic* rx = svc->createCharacteristic(
        NexoUuids::rx(), NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rx->setCallbacks(this);

    tx_ = svc->createCharacteristic(
        NexoUuids::tx(), NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);

    status_ = svc->createCharacteristic(
        NexoUuids::status(), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    uint8_t zero[20] = {0};
    status_->setValue(zero, sizeof(zero));
    updateStatus();

    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NexoUuids::service());
    adv->setName(deviceName);
    adv->setScanResponse(true);
    adv->start();
}

uint16_t BleService::mtuPayload(uint16_t conn) const
{
    uint16_t mtu = 23;
    if (server_) {
        uint16_t peer = server_->getPeerMTU(conn);
        if (peer >= 23) mtu = peer;
    }
    uint16_t payload = mtu - 3;
    if (payload > kMtuCap) payload = kMtuCap;
    return payload;
}

void BleService::onConnect(NimBLEServer* server, ble_gap_conn_desc* desc)
{
    const uint16_t conn = desc->conn_handle;
    peerAddr_ = NimBLEAddress(desc->peer_id_addr).toString();
    ++connCount_;
    statusDirty_ = true;
    // Multi-client: keep advertising so a second central can connect too. NimBLE
    // stops advertising on its own only once NIMBLE_MAX_CONNECTIONS is reached.
    // Ask for a snappy-but-stable connection interval (15-30 ms, 4 s timeout).
    server->updateConnParams(conn, 12, 24, 0, 400);
    // The NimBLE host stops advertising once a connection is established, so
    // restart it here to let a *second* companion app discover and connect.
    advDirty_ = true;
    Serial.printf("[ble] connect handle=%u peers=%u\n", conn, (unsigned)connCount_);
}

void BleService::onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc)
{
    (void)server;
    const uint16_t conn = desc ? desc->conn_handle : 0;
    if (connCount_ > 0) --connCount_;
    Serial.printf("[ble] disconnect handle=%u peers=%u\n", conn, (unsigned)connCount_);
    // Only tear down the shared session (auth) once every central is gone.
    if (connCount_ == 0) {
        peerAddr_.clear();
        pendingDisconnect_ = true;
    }
    // Restart advertising (NimBLE also does this via advertiseOnDisconnect).
    advDirty_ = true;
}

void BleService::onWrite(NimBLECharacteristic* chr, ble_gap_conn_desc* desc)
{
    // Runs on the NimBLE host task: only copy the bytes, do no real work here.
    NimBLEAttValue v = chr->getValue();
    size_t n = v.length();
    if (n == 0) return;
    if (n > sizeof(pending_[0].data)) n = sizeof(pending_[0].data);
    const uint16_t conn = desc ? desc->conn_handle : 0;

    portENTER_CRITICAL(&mux_);
    if (pwCount_ < kWriteSlots) {
        PendingWrite& p = pending_[pwHead_];
        p.conn = conn;
        p.len = static_cast<uint16_t>(n);
        memcpy(p.data, v.data(), n);
        pwHead_ = (pwHead_ + 1) % kWriteSlots;
        ++pwCount_;
    }
    portEXIT_CRITICAL(&mux_);
}

void BleService::drainWrites()
{
    for (;;) {
        PendingWrite local;
        bool have = false;
        portENTER_CRITICAL(&mux_);
        if (pwCount_ > 0) {
            local = pending_[pwTail_];
            pwTail_ = (pwTail_ + 1) % kWriteSlots;
            --pwCount_;
            have = true;
        }
        portEXIT_CRITICAL(&mux_);
        if (!have) break;

        // Each connection has its own reassembler so concurrent frames from two
        // centrals cannot corrupt each other.
        auto it = reassemblers_.find(local.conn);
        if (it == reassemblers_.end()) it = reassemblers_.emplace(local.conn, Reassembler()).first;
        Message msg;
        if (!it->second.feed(local.data, local.len, msg)) continue;
        // Repository access + LittleFS writes happen here, on the main task.
        Message resp = app_->service().handle(msg);
        enqueueMessage(resp, mtuPayload(local.conn));
        // Latch UI events for a successful authentication / sync.
        if (!resp.payload.empty() &&
            static_cast<uint8_t>(resp.payload[0]) == ST_OK) {
            if (msg.type == OP_AUTH) pairedEvent_ = true;
            if (msg.type == OP_SYNC_PUSH || msg.type == OP_SYNC_PULL)
                syncEvent_ = true;
        }
        if (isMutatingOp(msg.type)) notifyDataChanged();
    }
}

void BleService::enqueueMessage(const Message& m, uint16_t mtu)
{
    std::vector<std::string> chunks;
    if (!encodeMessage(m, mtu, chunks)) return;
    for (auto& c : chunks) outQueue_.push_back(std::move(c));
}

void BleService::flushTx()
{
    if (connCount_ == 0 || !tx_) return;
    // Push a few notification chunks per loop (NimBLE 1.4's notify() is fire and
    // forget, so we pace it rather than checking a return value). Notifications
    // are broadcast to every subscribed central; clients match them by id.
    for (int i = 0; i < 4 && !outQueue_.empty(); ++i) {
        std::string& chunk = outQueue_.front();
        tx_->setValue(reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size());
        tx_->notify();
        outQueue_.pop_front();
    }
}

void BleService::notifyDataChanged()
{
    ++changeSeq_;
    statusDirty_ = true;
}

bool BleService::consumePairedEvent()
{
    const bool v = pairedEvent_;
    pairedEvent_ = false;
    return v;
}

bool BleService::consumeSyncEvent()
{
    const bool v = syncEvent_;
    syncEvent_ = false;
    return v;
}

void BleService::updateStatus()
{
    if (!status_) return;
    uint8_t buf[20] = {0};
    buf[0] = kProtocolVersion;
    buf[1] = (app_ && app_->service().authenticated()) ? 1 : 0;
    buf[2] = connected() ? 1 : 0;
    if (app_) {
        uint32_t n = static_cast<uint32_t>(app_->repo().noteCount());
        uint32_t h = static_cast<uint32_t>(app_->repo().homeworkCount());
        uint32_t up = millis() / 1000u;
        memcpy(buf + 4, &n, 4);
        memcpy(buf + 8, &h, 4);
        memcpy(buf + 12, &up, 4);
    }
    memcpy(buf + 16, &changeSeq_, 4);
    status_->setValue(buf, sizeof(buf));
    if (connected()) status_->notify();
    lastStatusMs_ = millis();
}

void BleService::refreshStatus()
{
    if (!status_) {
        statusDirty_ = false;
        return;
    }
    // Coalesce bursts of changes into at most ~5 notifications per second.
    if (connected() && millis() - lastStatusMs_ < 200u) return;
    statusDirty_ = false;
    updateStatus();
}

void BleService::loop()
{
    if (spamActive_) spamTick();
    if (pendingDisconnect_) {
        pendingDisconnect_ = false;
        if (app_) app_->service().onDisconnect();
        statusDirty_ = true;
    }
    if (advDirty_) {
        advDirty_ = false;
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        if (adv && !adv->isAdvertising()) adv->start();
    }
    if (connected()) {
        drainWrites();
        flushTx();
    }
    if (statusDirty_) refreshStatus();
}

void BleService::startSpam()
{
    if (spamActive_) return;
    spamActive_ = true;
    spamIndex_ = 0;
    spamNext_ = millis();
    Serial.println("[lab] ble spam start");
}

void BleService::stopSpam()
{
    if (!spamActive_) return;
    spamActive_ = false;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) {
        adv->stop();
        adv->setName(deviceName_);
        adv->start();
    }
    Serial.println("[lab] ble spam stop");
}

void BleService::pauseForBulk()
{
    if (bulkPaused_) return;
    bulkPaused_ = true;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) {
        adv->stop();
    }
    // Drop any active connections to free radio for Wi-Fi bulk transfer.
    if (server_ && connCount_ > 0) {
        server_->disconnect(0);
        pendingDisconnect_ = true;
    }
}

void BleService::resumeAfterBulk()
{
    if (!bulkPaused_) return;
    bulkPaused_ = false;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) {
        adv->setName(deviceName_);
        adv->start();
    }
    spamActive_ = false;
}

void BleService::spamTick()
{
    uint32_t now = millis();
    if ((int32_t)(now - spamNext_) < 0) return;
    spamNext_ = now + 500;
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (!adv) return;
    adv->stop();
    adv->setName(kBleSpamNames[spamIndex_]);
    adv->start();
    if (++spamIndex_ >= kBleSpamNameCount) spamIndex_ = 0;
}

}  // namespace nexo
