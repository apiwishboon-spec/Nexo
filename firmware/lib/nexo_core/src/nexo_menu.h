// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Home-menu model for the Cardputer UI.
//
// The menu label and the destination are bound together in one table so that
// reordering the menu can never silently change which page a row opens.
// Pure data (host-testable, no Arduino dependencies).
#include <cstddef>
#include <cstdint>

namespace nexo {

// Pages reachable directly from the home menu. Values are stable identifiers,
// not positions: do not reorder to reorder the menu.
enum class MenuTarget : uint8_t {
    Notes,
    Homework,
    Calculator,
    Timer,
    Memos,
    Settings,
    About,
};

struct MenuEntry {
    const char* label;
    MenuTarget target;
};

// Single source of truth for the home menu (order here is the display order).
inline constexpr MenuEntry kMenuEntries[] = {
    {"Quick Notes", MenuTarget::Notes},
    {"Homework", MenuTarget::Homework},
    {"Calculator", MenuTarget::Calculator},
    {"Timer", MenuTarget::Timer},
    {"Memos", MenuTarget::Memos},
    {"Settings", MenuTarget::Settings},
    {"About", MenuTarget::About},
};

inline constexpr size_t kMenuEntryCount = sizeof(kMenuEntries) / sizeof(kMenuEntries[0]);

}  // namespace nexo
