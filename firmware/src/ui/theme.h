// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Material 3-inspired dark palette adapted to the Cardputer's 240x135 display.
#include <cstdint>

namespace nexo {
namespace theme {

// Independent RGB565 conversion (no reliance on the display instance).
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBackground = rgb(18, 19, 22);
constexpr uint16_t kSurface = rgb(30, 32, 37);
constexpr uint16_t kSurfaceVariant = rgb(42, 45, 51);
constexpr uint16_t kPrimary = rgb(168, 199, 250);
constexpr uint16_t kOnPrimary = rgb(6, 32, 32);
constexpr uint16_t kOnSurface = rgb(227, 227, 230);
constexpr uint16_t kOnSurfaceVariant = rgb(154, 160, 166);
constexpr uint16_t kOutline = rgb(58, 61, 68);
constexpr uint16_t kError = rgb(242, 184, 181);
constexpr uint16_t kSuccess = rgb(160, 214, 168);

constexpr int kPad = 6;
constexpr int kRadius = 6;

}  // namespace theme
}  // namespace nexo
