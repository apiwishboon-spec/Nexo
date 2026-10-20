// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_codec.h"

namespace nexo {

static void writeStr16(ByteWriter& w, uint8_t tag, const std::string& s)
{
    w.u8(tag);
    w.str16(s);
}

// ---- Note ------------------------------------------------------------------
void encodeNote(ByteWriter& w, const Note& n, uint32_t baseRev)
{
    w.u8(kTagBaseRev);
    w.u32(baseRev);
    w.u8(kTagRev);
    w.u32(n.rev);
    w.u8(kTagCreated);
    w.u32(n.created);
    w.u8(kTagUpdated);
    w.u32(n.updated);
    w.u8(kTagFlags);
    w.u8(n.deleted ? 0x01 : 0x00);
    writeStr16(w, kTagId, n.id);
    writeStr16(w, kTagTitle, clampUtf8(n.title, kMaxTitleLen));
    writeStr16(w, kTagBody, clampUtf8(n.body, kMaxBodyLen));
    w.u8(kTagEnd);
}

bool decodeNote(ByteReader& r, Note& out, uint32_t& baseRev)
{
    baseRev = 0;
    out = Note{};
    for (;;) {
        uint8_t tag = 0;
        if (!r.u8(tag)) return false;
        if (tag == kTagEnd) return true;
        switch (tag) {
            case kTagBaseRev: {
                uint32_t v;
                if (!r.u32(v)) return false;
                baseRev = v;
                break;
            }
            case kTagRev: {
                if (!r.u32(out.rev)) return false;
                break;
            }
            case kTagCreated: {
                if (!r.u32(out.created)) return false;
                break;
            }
            case kTagUpdated: {
                if (!r.u32(out.updated)) return false;
                break;
            }
            case kTagFlags: {
                uint8_t f;
                if (!r.u8(f)) return false;
                out.deleted = (f & 0x01) != 0;
                break;
            }
            case kTagId: {
                if (!r.str16(out.id)) return false;
                break;
            }
            case kTagTitle: {
                if (!r.str16(out.title)) return false;
                break;
            }
            case kTagBody: {
                if (!r.str16(out.body)) return false;
                break;
            }
            default: {
                if (!r.skipTag(tag)) return false;
            }
        }
    }
}

// ---- Homework --------------------------------------------------------------
void encodeHomework(ByteWriter& w, const Homework& h, uint32_t baseRev)
{
    w.u8(kTagBaseRev);
    w.u32(baseRev);
    w.u8(kTagRev);
    w.u32(h.rev);
    w.u8(kTagCreated);
    w.u32(h.created);
    w.u8(kTagUpdated);
    w.u32(h.updated);
    w.u8(kTagFlags);
    w.u8(h.deleted ? 0x01 : 0x00);
    w.u8(kTagStatus);
    w.u8(static_cast<uint8_t>(h.status));
    w.u8(kTagDueDay);
    w.u32(h.dueEpochDay);
    writeStr16(w, kTagId, h.id);
    writeStr16(w, kTagSubject, clampUtf8(h.subject, kMaxSubjectLen));
    writeStr16(w, kTagDesc, clampUtf8(h.description, kMaxDescLen));
    w.u8(kTagEnd);
}

size_t encodedNoteSize(const Note& n)
{
    // Fixed tags/values (48) + str16 id/title/body payloads.
    return 48 + kIdLen + clampUtf8(n.title, kMaxTitleLen).size() +
           clampUtf8(n.body, kMaxBodyLen).size();
}

size_t encodedHomeworkSize(const Homework& h)
{
    // Fixed tags/values (55) + str16 id/subject/description payloads.
    return 55 + kIdLen + clampUtf8(h.subject, kMaxSubjectLen).size() +
           clampUtf8(h.description, kMaxDescLen).size();
}

bool decodeHomework(ByteReader& r, Homework& out, uint32_t& baseRev)
{
    baseRev = 0;
    out = Homework{};
    for (;;) {
        uint8_t tag = 0;
        if (!r.u8(tag)) return false;
        if (tag == kTagEnd) return true;
        switch (tag) {
            case kTagBaseRev: {
                uint32_t v;
                if (!r.u32(v)) return false;
                baseRev = v;
                break;
            }
            case kTagRev: {
                if (!r.u32(out.rev)) return false;
                break;
            }
            case kTagCreated: {
                if (!r.u32(out.created)) return false;
                break;
            }
            case kTagUpdated: {
                if (!r.u32(out.updated)) return false;
                break;
            }
            case kTagFlags: {
                uint8_t f;
                if (!r.u8(f)) return false;
                out.deleted = (f & 0x01) != 0;
                break;
            }
            case kTagStatus: {
                uint8_t s;
                if (!r.u8(s)) return false;
                uint8_t clamped = s > 2 ? 2 : s;
                out.status = static_cast<HomeworkStatus>(clamped);
                break;
            }
            case kTagDueDay: {
                if (!r.u32(out.dueEpochDay)) return false;
                break;
            }
            case kTagId: {
                if (!r.str16(out.id)) return false;
                break;
            }
            case kTagSubject: {
                if (!r.str16(out.subject)) return false;
                break;
            }
            case kTagDesc: {
                if (!r.str16(out.description)) return false;
                break;
            }
            default: {
                if (!r.skipTag(tag)) return false;
            }
        }
    }
}

// ---- Settings --------------------------------------------------------------
void encodeSettings(ByteWriter& w, const Settings& s, bool includePin)
{
    writeStr16(w, kTagDeviceName, clampUtf8(s.deviceName, kMaxNameLen));
    writeStr16(w, kTagApSsid, clampUtf8(s.apSsid, kMaxNameLen));
    if (includePin) writeStr16(w, kTagPin, clampUtf8(s.pin, kMaxPinLen));
    w.u8(kTagApEnabled);
    w.u8(s.apEnabled ? 1 : 0);
    w.u8(kTagVolume);
    w.u8(s.volume > 100 ? 100 : s.volume);
    w.u8(kTagSoundOn);
    w.u8(s.soundEnabled ? 1 : 0);
    w.u8(kTagBrightness);
    w.u8(s.brightness > 100 ? 100 : s.brightness);
    w.u8(kTagScreenTimeout);
    w.u16(s.screenTimeoutSec);
    w.u8(kTagAutoAdvance);
    w.u8(s.autoAdvanceMusic ? 1 : 0);
    w.u8(kTagTzOffset);
    w.u16(static_cast<uint16_t>(s.timezoneOffsetMin));
    w.u8(kTagEnd);
}

bool decodeSettings(ByteReader& r, Settings& out)
{
    for (;;) {
        uint8_t tag = 0;
        if (!r.u8(tag)) return false;
        if (tag == kTagEnd) return true;
        switch (tag) {
            case kTagDeviceName: {
                if (!r.str16(out.deviceName)) return false;
                break;
            }
            case kTagApSsid: {
                if (!r.str16(out.apSsid)) return false;
                break;
            }
            case kTagPin: {
                std::string pin;
                if (!r.str16(pin)) return false;
                if (!pin.empty()) out.pin = clampUtf8(pin, kMaxPinLen);
                break;
            }
            case kTagApPassword: {
                std::string ap;
                if (!r.str16(ap)) return false;  // accepted but unused (AP pass is derived)
                break;
            }
            case kTagApEnabled: {
                uint8_t v;
                if (!r.u8(v)) return false;
                out.apEnabled = v != 0;
                break;
            }
            case kTagVolume: {
                uint8_t v;
                if (!r.u8(v)) return false;
                out.volume = v > 100 ? 100 : v;
                break;
            }
            case kTagSoundOn: {
                uint8_t v;
                if (!r.u8(v)) return false;
                out.soundEnabled = v != 0;
                break;
            }
            case kTagBrightness: {
                uint8_t v;
                if (!r.u8(v)) return false;
                out.brightness = v > 100 ? 100 : v;
                break;
            }
            case kTagScreenTimeout: {
                if (!r.u16(out.screenTimeoutSec)) return false;
                break;
            }
            case kTagAutoAdvance: {
                uint8_t v;
                if (!r.u8(v)) return false;
                out.autoAdvanceMusic = v != 0;
                break;
            }
            case kTagTzOffset: {
                uint16_t v;
                if (!r.u16(v)) return false;
                out.timezoneOffsetMin = static_cast<int16_t>(v);
                break;
            }
            default: {
                if (!r.skipTag(tag)) return false;
            }
        }
    }
}

// ---- v2: DeviceInfo --------------------------------------------------------
std::string encodeDeviceInfo(const DeviceInfo& d)
{
    ByteWriter w;
    w.u8(kDevProto);
    w.u8(d.protocolVersion);
    w.u8(kDevBattery);
    w.u8(d.batteryPercent);
    w.u8(kDevCharging);
    w.u8(d.charging ? 1 : 0);
    w.u8(kDevUptime);
    w.u32(d.uptimeSec);
    w.u8(kDevFreeHeap);
    w.u32(d.freeHeapKiB);
    w.u8(kDevSdPresent);
    w.u8(d.sdPresent ? 1 : 0);
    w.u8(kDevSdFree);
    w.u32(d.sdFreeMb);
    w.u8(kDevSdTotal);
    w.u32(d.sdTotalMb);
    w.u8(kDevMusicCount);
    w.u16(d.musicCount);
    w.u8(kDevVolume);
    w.u8(d.volume);
    w.u8(kDevSoundOn);
    w.u8(d.soundEnabled ? 1 : 0);
    w.u8(kDevHpIn);
    w.u8(d.headphoneIn ? 1 : 0);
    w.u8(kDevApOn);
    w.u8(d.apEnabled ? 1 : 0);
    w.u8(kDevRssi);
    w.u8(static_cast<uint8_t>(d.rssi));
    w.u8(kDevScreenTimeout);
    w.u16(d.screenTimeoutSec);
    writeStr16(w, kDevName, clampUtf8(d.deviceName, kMaxNameLen));
    writeStr16(w, kDevFwVer, d.firmwareVersion);
    writeStr16(w, kDevMac, d.mac);
    writeStr16(w, kDevApSsid, clampUtf8(d.apSsid, kMaxNameLen));
    writeStr16(w, kDevApPass, d.apPassword);
    writeStr16(w, kDevIp, d.ip);
    w.u8(kDevEnd);
    return w.data();
}

// ---- v2: Music -------------------------------------------------------------
std::string encodeMusicBatch(const std::vector<MusicTrack>& tracks)
{
    ByteWriter w;
    w.u16(static_cast<uint16_t>(tracks.size()));
    for (const auto& t : tracks) {
        writeStr16(w, 0x10, clampUtf8(t.name, kMaxTrackNameLen));
        w.u32(t.size);
    }
    return w.data();
}

std::string encodeMusicStatus(const MusicStatus& st)
{
    ByteWriter w;
    w.u16(static_cast<uint16_t>(static_cast<int16_t>(st.index)));  // two's complement
    w.u8(st.playing ? 1 : 0);
    w.u16(st.positionSec);
    w.u16(st.durationSec);
    w.u8(st.volume);
    return w.data();
}

bool decodeMusicControl(ByteReader& r, MusicAction& action, int16_t& index, uint16_t& arg)
{
    uint8_t a = 0;
    uint16_t idx = 0;
    uint16_t a2 = 0;
    if (!r.u8(a) || !r.u16(idx) || !r.u16(a2)) return false;
    action = static_cast<MusicAction>(a);
    index = static_cast<int16_t>(idx);
    arg = a2;
    return true;
}

// ---- Batches ---------------------------------------------------------------
std::string encodeNoteBatch(const NoteBatch& b)
{
    ByteWriter w;
    w.u16(static_cast<uint16_t>(b.items.size()));
    for (const auto& n : b.items) encodeNote(w, n, 0);
    w.u8(b.more ? 1 : 0);
    return w.data();
}

std::string encodeHomeworkBatch(const HomeworkBatch& b)
{
    ByteWriter w;
    w.u16(static_cast<uint16_t>(b.items.size()));
    for (const auto& h : b.items) encodeHomework(w, h, 0);
    w.u8(b.more ? 1 : 0);
    return w.data();
}

bool decodeNoteBatch(ByteReader& r, NoteBatch& out)
{
    out.items.clear();
    uint16_t count = 0;
    if (!r.u16(count)) return false;
    for (uint16_t i = 0; i < count; ++i) {
        Note n;
        uint32_t base;
        if (!decodeNote(r, n, base)) return false;
        out.items.push_back(std::move(n));
    }
    uint8_t more = 0;
    if (!r.u8(more)) return false;
    out.more = more != 0;
    return true;
}

bool decodeHomeworkBatch(ByteReader& r, HomeworkBatch& out)
{
    out.items.clear();
    uint16_t count = 0;
    if (!r.u16(count)) return false;
    for (uint16_t i = 0; i < count; ++i) {
        Homework h;
        uint32_t base;
        if (!decodeHomework(r, h, base)) return false;
        out.items.push_back(std::move(h));
    }
    uint8_t more = 0;
    if (!r.u8(more)) return false;
    out.more = more != 0;
    return true;
}

// ---- Sync ------------------------------------------------------------------
std::string encodeSyncDelta(const SyncDelta& d)
{
    ByteWriter w;
    w.u16(static_cast<uint16_t>(d.kinds.size()));
    size_t ni = 0, hi = 0;
    for (uint8_t kind : d.kinds) {
        w.u8(kind);
        if (kind == 1 && ni < d.notes.size()) {
            encodeNote(w, d.notes[ni++], 0);
        } else if (kind == 2 && hi < d.homework.size()) {
            encodeHomework(w, d.homework[hi++], 0);
        }
    }
    w.u32(d.serverEpoch);
    w.u8(d.more ? 1 : 0);
    return w.data();
}

std::string encodeSyncPush(const SyncPush& p)
{
    ByteWriter w;
    w.u16(static_cast<uint16_t>(p.kinds.size()));
    size_t ni = 0, hi = 0;
    for (uint8_t kind : p.kinds) {
        w.u8(kind);
        if (kind == 1 && ni < p.notes.size()) {
            encodeNote(w, p.notes[ni], p.notes[ni].rev);
            ++ni;
        } else if (kind == 2 && hi < p.homework.size()) {
            encodeHomework(w, p.homework[hi], p.homework[hi].rev);
            ++hi;
        }
    }
    return w.data();
}

bool decodeSyncPush(ByteReader& r, SyncPush& out)
{
    out.kinds.clear();
    out.notes.clear();
    out.homework.clear();
    uint16_t count = 0;
    if (!r.u16(count)) return false;
    for (uint16_t i = 0; i < count; ++i) {
        uint8_t kind = 0;
        if (!r.u8(kind)) return false;
        out.kinds.push_back(kind);
        if (kind == 1) {
            Note n;
            uint32_t base;
            if (!decodeNote(r, n, base)) return false;
            n.rev = base;  // baseRev travelled in the rev field
            out.notes.push_back(std::move(n));
        } else if (kind == 2) {
            Homework h;
            uint32_t base;
            if (!decodeHomework(r, h, base)) return false;
            h.rev = base;
            out.homework.push_back(std::move(h));
        }
    }
    return true;
}

std::string encodeSyncResult(const SyncResult& res)
{
    ByteWriter w;
    w.u16(res.applied);
    w.u16(res.conflicts);
    w.u32(res.serverEpoch);
    size_t ni = 0, hi = 0;
    for (const auto& e : res.entries) {
        w.u8(e.kind);
        if (e.kind == 1) {
            encodeNote(w, e.note, 0);
            ++ni;
        } else if (e.kind == 2) {
            encodeHomework(w, e.homework, 0);
            ++hi;
        }
    }
    return w.data();
}

}  // namespace nexo
