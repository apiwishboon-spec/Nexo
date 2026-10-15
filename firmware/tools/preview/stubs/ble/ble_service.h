#pragma once
// Preview stub for the BLE service (visual state only).
namespace nexo {

class BleService {
public:
    bool connected() const { return connected_; }
    void notifyDataChanged() {}
    bool consumePairedEvent() { return false; }
    bool consumeSyncEvent() { return false; }

    // Lab tool: no-ops in the preview.
    void startSpam() { spam_ = true; }
    void stopSpam() { spam_ = false; }
    bool spamActive() const { return spam_; }

    bool connected_ = true;
    bool spam_ = false;
};

}  // namespace nexo
