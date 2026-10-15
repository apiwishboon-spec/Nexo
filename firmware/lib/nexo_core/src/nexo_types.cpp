// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_types.h"

#include <cstdio>

namespace nexo {

static bool isHexLower(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

bool isValidId(const std::string& id)
{
    if (id.size() != kIdLen) return false;
    for (char c : id) {
        if (!isHexLower(c)) return false;
    }
    return true;
}

bool isValidDueDay(uint32_t day)
{
    // Rough bound: 1970-01-01 .. 2179-01-01 (~76300 days). 0 = unset.
    return day <= 76400;
}

std::string clampUtf8(const std::string& in, size_t maxBytes)
{
    if (in.size() <= maxBytes) return in;
    size_t end = maxBytes;
    // Back off to a UTF-8 boundary (do not split a multibyte sequence).
    while (end > 0 && (static_cast<unsigned char>(in[end]) & 0xC0) == 0x80) {
        --end;
    }
    return in.substr(0, end);
}

std::string idFromBytes(const uint8_t bytes[8])
{
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(16);
    for (int i = 0; i < 8; ++i) {
        out.push_back(hex[(bytes[i] >> 4) & 0x0F]);
        out.push_back(hex[bytes[i] & 0x0F]);
    }
    return out;
}

std::string trimAscii(const std::string& in)
{
    size_t b = 0;
    size_t e = in.size();
    auto sp = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (b < e && sp(in[b])) ++b;
    while (e > b && sp(in[e - 1])) --e;
    return in.substr(b, e - b);
}

// Howard Hinnant's civil-from-days / days-from-civil algorithms.
static void civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
}

static int64_t daysFromCivil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

std::string dueDayToIso(uint32_t epochDay)
{
    if (epochDay == 0) return std::string();
    int y;
    unsigned m, d;
    // Stored day 1 == 1970-01-01 (day 0 is reserved for "unset").
    civilFromDays(static_cast<int64_t>(epochDay) - 1, y, m, d);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02u", y, m, d);
    return std::string(buf);
}

uint32_t isoToDueDay(const std::string& isoDate)
{
    if (isoDate.size() < 10) return 0;
    int y = 0;
    unsigned m = 0, d = 0;
    if (sscanf(isoDate.c_str(), "%4d-%2u-%2u", &y, &m, &d) != 3) return 0;
    if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
    int64_t days = daysFromCivil(y, m, d);
    if (days < 0 || days > 76400) return 0;
    return static_cast<uint32_t>(days) + 1;  // 1970-01-01 -> 1
}

}  // namespace nexo
