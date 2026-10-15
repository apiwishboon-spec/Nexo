// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "device_storage.h"

namespace nexo {

static const char* kDir = "/nexo";

bool LittleFsStore::beginMount()
{
    if (!LittleFS.begin(true)) {  // true = format on failure (data partition)
        mounted_ = false;
        return false;
    }
    if (!LittleFS.exists(kDir)) {
        LittleFS.mkdir(kDir);
    }
    mounted_ = true;
    return true;
}

std::string LittleFsStore::pathFor(const std::string& name) const
{
    return std::string(kDir) + "/" + name;
}

bool LittleFsStore::load(const std::string& name, std::string& out)
{
    if (!mounted_) return false;
    std::string path = pathFor(name);
    if (!LittleFS.exists(path.c_str())) return false;
    File f = LittleFS.open(path.c_str(), "r");
    if (!f) return false;
    out.clear();
    out.reserve(f.size());
    while (f.available()) {
        out.push_back(static_cast<char>(f.read()));
    }
    f.close();
    return true;
}

bool LittleFsStore::save(const std::string& name, const std::string& data)
{
    if (!mounted_) return false;
    std::string path = pathFor(name);
    std::string tmp = path + ".tmp";
    {
        File f = LittleFS.open(tmp.c_str(), "w");
        if (!f) return false;
        size_t written = f.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
        f.flush();
        f.close();
        if (written != data.size()) {
            LittleFS.remove(tmp.c_str());
            return false;
        }
    }
    // Atomic-ish replace: remove target then rename temp over it.
    if (LittleFS.exists(path.c_str())) {
        LittleFS.remove(path.c_str());
    }
    return LittleFS.rename(tmp.c_str(), path.c_str());
}

}  // namespace nexo
