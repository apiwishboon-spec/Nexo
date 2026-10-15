// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Binary tag-based codec for Nexo records (see docs/PROTOCOL.md §4).
#include <cstdint>
#include <string>
#include <vector>

#include "nexo_types.h"

namespace nexo {

// Field tags
enum Tag : uint8_t {
    kTagBaseRev = 0x01,
    kTagRev = 0x02,
    kTagCreated = 0x03,
    kTagUpdated = 0x04,
    kTagFlags = 0x05,
    kTagStatus = 0x06,
    kTagDueDay = 0x07,
    kTagId = 0x10,
    kTagTitle = 0x11,
    kTagBody = 0x12,
    kTagSubject = 0x11,
    kTagDesc = 0x12,
    kTagDeviceName = 0x10,
    kTagPin = 0x11,
    kTagApSsid = 0x12,
    kTagApPassword = 0x13,
    kTagApEnabled = 0x20,
    // v2 extended settings (fixed-width, applied live + persisted).
    kTagVolume = 0x21,
    kTagSoundOn = 0x22,
    kTagBrightness = 0x23,
    kTagScreenTimeout = 0x24,
    kTagAutoAdvance = 0x25,
    kTagTzOffset = 0x26,
    kTagEnd = 0x80,
};

// DeviceInfo tag namespace (distinct record type, tags are local to it).
enum DeviceTag : uint8_t {
    kDevProto = 0x01,
    kDevBattery = 0x02,
    kDevCharging = 0x03,
    kDevUptime = 0x04,
    kDevFreeHeap = 0x05,
    kDevSdPresent = 0x06,
    kDevSdFree = 0x07,
    kDevSdTotal = 0x08,
    kDevMusicCount = 0x09,
    kDevVolume = 0x0A,
    kDevSoundOn = 0x0B,
    kDevHpIn = 0x0C,
    kDevApOn = 0x0D,
    kDevRssi = 0x0E,
    kDevScreenTimeout = 0x0F,
    kDevName = 0x10,
    kDevFwVer = 0x11,
    kDevMac = 0x12,
    kDevApSsid = 0x13,
    kDevApPass = 0x14,
    kDevIp = 0x15,
    kDevEnd = 0x80,
};

// Little-endian writer / reader with bounds checking.
class ByteWriter {
public:
    void u8(uint8_t v) { buf_.push_back(static_cast<char>(v)); }
    void u16(uint16_t v)
    {
        buf_.push_back(static_cast<char>(v & 0xFF));
        buf_.push_back(static_cast<char>((v >> 8) & 0xFF));
    }
    void u32(uint32_t v)
    {
        for (int i = 0; i < 4; ++i) buf_.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
    void bytes(const uint8_t* p, size_t n) { buf_.append(reinterpret_cast<const char*>(p), n); }
    void str16(const std::string& s)
    {
        u16(static_cast<uint16_t>(s.size()));
        buf_.append(s);
    }
    const std::string& data() const { return buf_; }
    std::string& data() { return buf_; }

private:
    std::string buf_;
};

class ByteReader {
public:
    ByteReader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    explicit ByteReader(const std::string& s)
        : p_(reinterpret_cast<const uint8_t*>(s.data())), n_(s.size())
    {
    }

    bool ok() const { return ok_; }
    size_t remaining() const { return n_ - i_; }

    bool u8(uint8_t& v)
    {
        if (i_ + 1 > n_) return fail();
        v = p_[i_++];
        return true;
    }
    bool u16(uint16_t& v)
    {
        if (i_ + 2 > n_) return fail();
        v = static_cast<uint16_t>(p_[i_]) | (static_cast<uint16_t>(p_[i_ + 1]) << 8);
        i_ += 2;
        return true;
    }
    bool u32(uint32_t& v)
    {
        if (i_ + 4 > n_) return fail();
        v = 0;
        for (int k = 0; k < 4; ++k) v |= static_cast<uint32_t>(p_[i_ + k]) << (8 * k);
        i_ += 4;
        return true;
    }
    bool str16(std::string& out)
    {
        uint16_t len = 0;
        if (!u16(len)) return false;
        if (i_ + len > n_) return fail();
        out.assign(reinterpret_cast<const char*>(p_ + i_), len);
        i_ += len;
        return true;
    }
    bool fixed(std::string& out, size_t len)
    {
        if (i_ + len > n_) return fail();
        out.assign(reinterpret_cast<const char*>(p_ + i_), len);
        i_ += len;
        return true;
    }

    // Skip a value of unknown length given its tag range.
    bool skipTag(uint8_t tag)
    {
        if (tag >= 0x80) return true;  // terminator handled by caller
        if (tag >= 0x10 && tag < 0x20) {
            uint16_t len = 0;
            if (!u16(len)) return false;
            return skip(len);
        }
        if (tag >= 0x20) {
            // one-byte length
            uint8_t len = 0;
            if (!u8(len)) return false;
            return skip(len);
        }
        // 0x01..0x0F fixed widths
        switch (tag) {
            case kTagBaseRev:
            case kTagRev:
            case kTagCreated:
            case kTagUpdated:
            case kTagDueDay: {
                uint32_t tmp;
                return u32(tmp);
            }
            case kTagFlags:
            case kTagStatus: {
                uint8_t tmp;
                return u8(tmp);
            }
            default: {
                uint8_t tmp;
                return u8(tmp);
            }
        }
    }

    bool skip(size_t n)
    {
        if (i_ + n > n_) return fail();
        i_ += n;
        return true;
    }

private:
    bool fail()
    {
        ok_ = false;
        return false;
    }
    const uint8_t* p_;
    size_t n_;
    size_t i_ = 0;
    bool ok_ = true;
};

// ---- Record encoding -------------------------------------------------------
void encodeNote(ByteWriter& w, const Note& n, uint32_t baseRev);
bool decodeNote(ByteReader& r, Note& out, uint32_t& baseRev);

void encodeHomework(ByteWriter& w, const Homework& h, uint32_t baseRev);
bool decodeHomework(ByteReader& r, Homework& out, uint32_t& baseRev);

void encodeSettings(ByteWriter& w, const Settings& s, bool includePin);
bool decodeSettings(ByteReader& r, Settings& out);

// ---- v2: device info + music ----------------------------------------------
std::string encodeDeviceInfo(const DeviceInfo& info);
std::string encodeMusicBatch(const std::vector<MusicTrack>& tracks);
std::string encodeMusicStatus(const MusicStatus& st);
bool decodeMusicControl(ByteReader& r, MusicAction& action, int16_t& index, uint16_t& arg);

// ---- Container encoding ----------------------------------------------------
struct NoteBatch {
    std::vector<Note> items;
    bool more = false;
};
struct HomeworkBatch {
    std::vector<Homework> items;
    bool more = false;
};

std::string encodeNoteBatch(const NoteBatch& b);
std::string encodeHomeworkBatch(const HomeworkBatch& b);

struct SyncDelta {
    // kind: 1 = note, 2 = homework
    std::vector<uint8_t> kinds;
    std::vector<Note> notes;
    std::vector<Homework> homework;
    uint32_t serverEpoch = 0;
    bool more = false;
};

struct ConflictEntry {
    uint8_t kind = 0;
    Note note;
    Homework homework;
};

struct SyncPush {
    std::vector<uint8_t> kinds;
    std::vector<Note> notes;
    std::vector<Homework> homework;
};

struct SyncResult {
    uint16_t applied = 0;
    uint16_t conflicts = 0;
    uint32_t serverEpoch = 0;
    std::vector<ConflictEntry> entries;
};

std::string encodeSyncDelta(const SyncDelta& d);
std::string encodeSyncPush(const SyncPush& p);
std::string encodeSyncResult(const SyncResult& r);

bool decodeNoteBatch(ByteReader& r, NoteBatch& out);
bool decodeHomeworkBatch(ByteReader& r, HomeworkBatch& out);
bool decodeSyncPush(ByteReader& r, SyncPush& out);

}  // namespace nexo
