// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_frame.h"

namespace nexo {

bool encodeMessage(const Message& msg, size_t mtuPayload, std::vector<std::string>& out)
{
    out.clear();
    if (msg.payload.size() > kMaxMessage) return false;
    if (mtuPayload <= kChunkHeaderLen) return false;
    const size_t chunkData = mtuPayload - kChunkHeaderLen;
    const size_t total = msg.payload.size();
    size_t offset = 0;
    // Always emit at least one chunk, even for empty payloads.
    do {
        size_t n = total - offset;
        if (n > chunkData) n = chunkData;
        std::string chunk;
        chunk.reserve(kChunkHeaderLen + n);
        chunk.push_back(static_cast<char>(msg.version));
        chunk.push_back(static_cast<char>(msg.type));
        chunk.push_back(static_cast<char>(msg.id & 0xFF));
        chunk.push_back(static_cast<char>((msg.id >> 8) & 0xFF));
        chunk.push_back(static_cast<char>(offset & 0xFF));
        chunk.push_back(static_cast<char>((offset >> 8) & 0xFF));
        chunk.push_back(static_cast<char>(total & 0xFF));
        chunk.push_back(static_cast<char>((total >> 8) & 0xFF));
        chunk.append(msg.payload, offset, n);
        out.push_back(std::move(chunk));
        offset += n;
    } while (offset < total);
    return true;
}

bool Reassembler::feed(const uint8_t* data, size_t len, Message& out)
{
    lastError_ = 0;
    ++clock_;
    if (len < kChunkHeaderLen) {
        lastError_ = 0x02;
        return false;
    }
    const uint8_t version = data[0];
    const uint8_t type = data[1];
    const uint16_t id = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
    const uint16_t offset = static_cast<uint16_t>(data[4]) | (static_cast<uint16_t>(data[5]) << 8);
    const uint16_t total = static_cast<uint16_t>(data[6]) | (static_cast<uint16_t>(data[7]) << 8);
    const size_t chunkLen = len - kChunkHeaderLen;

    if (total > kMaxMessage) {
        lastError_ = 0x06;  // TOO_LARGE
        return false;
    }
    if (static_cast<size_t>(offset) + chunkLen > total) {
        lastError_ = 0x02;
        return false;
    }

    // Locate the slot for this (type,id).
    int idx = -1;
    int freeIdx = -1;
    for (int i = 0; i < kSlots; ++i) {
        if (slots_[i].active) {
            if (slots_[i].type == type && slots_[i].id == id && slots_[i].total == total) {
                idx = i;
                break;
            }
        } else if (freeIdx < 0) {
            freeIdx = i;
        }
    }

    if (idx < 0) {
        if (offset != 0) {
            // Continuation for an unknown/expired transfer.
            lastError_ = 0x03;  // NOT_FOUND
            return false;
        }
        if (freeIdx < 0) {
            // Reclaim the least-recently-used slot to stay bounded.
            uint64_t oldest = UINT64_MAX;
            for (int i = 0; i < kSlots; ++i) {
                if (slots_[i].lastSeen < oldest) {
                    oldest = slots_[i].lastSeen;
                    freeIdx = i;
                }
            }
        }
        Slot& s = slots_[freeIdx];
        s.active = true;
        s.type = type;
        s.id = id;
        s.total = total;
        s.filled = 0;
        s.buf.assign(total, '\0');
        idx = freeIdx;
    }

    Slot& s = slots_[idx];
    s.lastSeen = clock_;

    if (offset != s.filled) {
        // Ordered transport expected; a gap means we could not assemble reliably.
        s.active = false;
        lastError_ = 0x02;
        return false;
    }
    s.buf.replace(offset, chunkLen, reinterpret_cast<const char*>(data + kChunkHeaderLen), chunkLen);
    s.filled = static_cast<uint16_t>(offset + chunkLen);

    if (s.filled == s.total) {
        Message m;
        m.version = version;
        m.type = type;
        m.id = id;
        m.payload = std::move(s.buf);
        s.active = false;
        s.buf.clear();
        out = std::move(m);
        return true;
    }
    return false;
}

}  // namespace nexo
