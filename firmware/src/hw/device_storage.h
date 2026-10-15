// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// LittleFS-backed IStore. Writes are atomic (temp file + rename).
#include <LittleFS.h>

#include "nexo_storage.h"

namespace nexo {

class LittleFsStore : public IStore {
public:
    bool beginMount();

    bool load(const std::string& name, std::string& out) override;
    bool save(const std::string& name, const std::string& data) override;

private:
    std::string pathFor(const std::string& name) const;
    bool mounted_ = false;
};

}  // namespace nexo
