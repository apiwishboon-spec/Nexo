#pragma once
// Preview stub for the BLE service (visual state only).
namespace nexo {

class BleService {
public:
    bool connected() const { return connected_; }
    void notifyDataChanged() {}
    bool consumePairedEvent() { return false; }
    bool consumeSyncEvent() { return false; }
    bool connected_ = true;
};

}  // namespace nexo
