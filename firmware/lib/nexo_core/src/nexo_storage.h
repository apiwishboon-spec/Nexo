// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Persistence interface: implemented by LittleFS on device and by an in-memory
// or filesystem store in host tests.
#include <string>

namespace nexo {

class IStore {
public:
    virtual ~IStore() = default;
    // Loads a named document. Returns true and fills `out` when present.
    virtual bool load(const std::string& name, std::string& out) = 0;
    // Atomically persists a named document.
    virtual bool save(const std::string& name, const std::string& data) = 0;
};

}  // namespace nexo
