#pragma once
// Preview stub: shadows src/wifi/wifi_access.h.
#include <string>

namespace nexo {

class WifiAccess {
public:
    bool enabled() const { return enabled_; }
    std::string ipString() const { return "192.168.4.1"; }
    std::string apPassword() const { return "nexoA1B2C3"; }
    void setEnabled(bool e) { enabled_ = e; }
    bool enabled_ = true;

private:
};

}  // namespace nexo
