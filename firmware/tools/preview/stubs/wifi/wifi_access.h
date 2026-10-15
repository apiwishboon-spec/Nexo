#pragma once
// Preview stub: shadows src/wifi/wifi_access.h.
#include <cstdint>
#include <string>
#include <vector>

namespace nexo {

class WifiAccess {
public:
    struct ApInfo {
        std::string ssid;
        int8_t rssi = 0;
        uint8_t channel = 0;
        bool secure = false;
    };

    bool enabled() const { return enabled_; }
    std::string ipString() const { return "192.168.4.1"; }
    std::string apPassword() const { return "nexoA1B2C3"; }
    void setEnabled(bool e) { enabled_ = e; }

    // Lab tools: no-ops in the preview.
    void scanStart() {}
    bool scanRunning() const { return false; }
    const std::vector<ApInfo>& scanResults() const { return empty_; }
    void startBeaconSpam() {}
    void stopBeaconSpam() {}
    bool beaconSpamActive() const { return false; }
    uint32_t spamFrames() const { return 0; }

    bool enabled_ = true;

private:
    std::vector<ApInfo> empty_;
};

}  // namespace nexo
