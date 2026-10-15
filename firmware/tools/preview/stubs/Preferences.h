#pragma once
// Host-only fake of the Arduino-ESP32 Preferences (NVS) class. Values live in a
// process-local map so previews/emulator behave predictably across a run.
#include <cstdint>
#include <map>
#include <string>

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false) {
        (void)name;
        (void)readOnly;
        return true;
    }
    void end() {}

    bool getBool(const char* key, bool def = false) const {
        auto it = store().find(key);
        return it == store().end() ? def : it->second != 0;
    }
    void putBool(const char* key, bool value) { store()[key] = value ? 1 : 0; }

    uint8_t getUChar(const char* key, uint8_t def = 0) const {
        auto it = store().find(key);
        return it == store().end() ? def : (uint8_t)it->second;
    }
    void putUChar(const char* key, uint8_t value) { store()[key] = value; }

    uint32_t getUInt(const char* key, uint32_t def = 0) const {
        auto it = store().find(key);
        return it == store().end() ? def : (uint32_t)it->second;
    }
    void putUInt(const char* key, uint32_t value) { store()[key] = (long)value; }

private:
    static std::map<std::string, long>& store() {
        static std::map<std::string, long> s;
        return s;
    }
};
