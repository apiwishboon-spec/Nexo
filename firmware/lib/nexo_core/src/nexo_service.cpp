// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_service.h"

#include "nexo_codec.h"

namespace nexo {

static const size_t kPageSize = 20;  // records per LIST_* page
// Keep a page comfortably below kMaxMessage so large notes cannot make the
// whole response unreadable (and therefore unsyncable).
static const size_t kPageByteBudget = 3500;
static const uint8_t kMaxFailBeforeLock = 5;
static const uint32_t kLockSeconds = 30;

NexoService::NexoService(Repository& repo, SyncEngine& sync, std::function<uint32_t()> now,
                         DeviceHooks* hooks)
    : repo_(repo), sync_(sync), now_(std::move(now)), hooks_(hooks)
{
}

Message NexoService::makeResponse(const Message& req, uint8_t status, const std::string& payload)
{
    Message resp;
    resp.version = kProtocolVersion;
    resp.type = static_cast<uint8_t>(req.type | kResponseBit);
    resp.id = req.id;
    resp.payload.push_back(static_cast<char>(status));
    resp.payload += payload;
    // Cache for duplicate protection.
    cache_.push_back({req.type, req.id, resp.payload});
    while (cache_.size() > 64) cache_.pop_front();
    return resp;
}

bool NexoService::checkAuthLocked(uint32_t now)
{
    return lockUntil_ != 0 && now < lockUntil_;
}

void NexoService::registerFailure(uint32_t now)
{
    if (failCount_ < 255) failCount_++;
    if (failCount_ >= kMaxFailBeforeLock) {
        lockUntil_ = now + kLockSeconds;
        failCount_ = 0;
    }
}

Message NexoService::handle(const Message& req)
{
    // Duplicate-request replay.
    for (const auto& c : cache_) {
        if (c.type == req.type && c.id == req.id) {
            Message resp;
            resp.version = kProtocolVersion;
            resp.type = static_cast<uint8_t>(req.type | kResponseBit);
            resp.id = req.id;
            resp.payload = c.response;
            return resp;
        }
    }

    if (req.version != kProtocolVersion) {
        return makeResponse(req, ST_VERSION, "unsupported protocol version");
    }

    uint32_t now = now_();
    const uint8_t op = req.type;

    // Auth throttle applies to every op except the unauthenticated probes.
    if (op != OP_PING && op != OP_GET_INFO) {
        if (checkAuthLocked(now)) return makeResponse(req, ST_LOCKED, "auth locked");
    }
    // Unauthenticated-allowlisted operations.
    const bool open = (op == OP_PING || op == OP_GET_INFO || op == OP_AUTH);
    if (!open && !authenticated_) return makeResponse(req, ST_UNAUTHORIZED, "pin required");

    ByteReader r(req.payload);
    switch (op) {
        case OP_PING: {
            ByteWriter w;
            w.u32(now);
            return makeResponse(req, ST_OK, w.data());
        }
        case OP_GET_INFO: {
            ByteWriter w;
            w.u8(kProtocolVersion);
            w.str16(repo_.settings().deviceName);
            w.u32(static_cast<uint32_t>(repo_.noteCount()));
            w.u32(static_cast<uint32_t>(repo_.homeworkCount()));
            return makeResponse(req, ST_OK, w.data());
        }
        case OP_GET_DEVICE_INFO: {
            DeviceInfo info = hooks_ ? hooks_->info() : DeviceInfo{};
            if (info.deviceName.empty()) info.deviceName = repo_.settings().deviceName;
            info.protocolVersion = kProtocolVersion;
            if (info.volume == 0) info.volume = repo_.settings().volume;
            info.screenTimeoutSec = repo_.settings().screenTimeoutSec;
            return makeResponse(req, ST_OK, encodeDeviceInfo(info));
        }
        case OP_SET_TIME: {
            uint32_t epoch = 0;
            uint16_t tz = 0;
            if (!r.u32(epoch) || !r.u16(tz)) return makeResponse(req, ST_BAD_REQUEST);
            if (hooks_) hooks_->setTime(epoch, static_cast<int16_t>(tz));
            return makeResponse(req, ST_OK);
        }
        case OP_REBOOT: {
            if (hooks_) hooks_->reboot();
            return makeResponse(req, ST_OK);
        }
        case OP_FACTORY_RESET: {
            if (hooks_) hooks_->factoryReset();
            return makeResponse(req, ST_OK);
        }
        case OP_FIND_DEVICE: {
            if (hooks_) hooks_->find();
            return makeResponse(req, ST_OK);
        }
        case OP_AUTH: {
            uint8_t len = 0;
            std::string pin;
            if (!r.u8(len) || !r.fixed(pin, len)) return makeResponse(req, ST_BAD_REQUEST);
            if (pin == repo_.settings().pin) {
                authenticated_ = true;
                failCount_ = 0;
                lockUntil_ = 0;
                return makeResponse(req, ST_OK);
            }
            registerFailure(now);
            return makeResponse(req, ST_UNAUTHORIZED, "bad pin");
        }
        case OP_LIST_NOTES: {
            uint16_t offset = 0, limit = 0;
            if (!r.u16(offset) || !r.u16(limit)) return makeResponse(req, ST_BAD_REQUEST);
            if (limit == 0 || limit > 100) limit = 100;
            NoteBatch batch;
            size_t seen = 0, emitted = 0, used = 3;  // count + more flag
            for (const auto& n : repo_.notes()) {
                if (n.deleted) continue;
                if (seen++ < offset) continue;
                if (emitted >= limit) {
                    batch.more = true;
                    break;
                }
                const size_t size = encodedNoteSize(n);
                if (emitted > 0 && used + size > kPageByteBudget) {
                    batch.more = true;
                    break;
                }
                batch.items.push_back(n);
                used += size;
                ++emitted;
            }
            return makeResponse(req, ST_OK, encodeNoteBatch(batch));
        }
        case OP_GET_NOTE: {
            std::string id;
            if (!r.fixed(id, kIdLen) || !isValidId(id)) return makeResponse(req, ST_BAD_REQUEST);
            Note n;
            if (!repo_.getNote(id, n) || n.deleted) return makeResponse(req, ST_NOT_FOUND);
            ByteWriter w;
            encodeNote(w, n, 0);
            return makeResponse(req, ST_OK, w.data());
        }
        case OP_PUT_NOTE: {
            Note in;
            uint32_t base = 0;
            if (!decodeNote(r, in, base)) return makeResponse(req, ST_BAD_REQUEST);
            Note stored;
            RepoStatus st = repo_.putNote(in, base, stored);
            ByteWriter w;
            encodeNote(w, stored, 0);
            if (st == RepoStatus::Ok) return makeResponse(req, ST_OK, w.data());
            if (st == RepoStatus::Conflict) return makeResponse(req, ST_CONFLICT, w.data());
            if (st == RepoStatus::Storage) return makeResponse(req, ST_STORAGE);
            return makeResponse(req, ST_BAD_REQUEST);
        }
        case OP_DELETE_NOTE: {
            std::string id;
            uint32_t base = 0;
            if (!r.fixed(id, kIdLen) || !r.u32(base) || !isValidId(id))
                return makeResponse(req, ST_BAD_REQUEST);
            Note stored;
            RepoStatus st = repo_.deleteNote(id, base, stored);
            if (st == RepoStatus::Ok) return makeResponse(req, ST_OK, id);
            if (st == RepoStatus::Conflict) {
                ByteWriter w;
                encodeNote(w, stored, 0);
                return makeResponse(req, ST_CONFLICT, w.data());
            }
            if (st == RepoStatus::NotFound) return makeResponse(req, ST_NOT_FOUND);
            return makeResponse(req, ST_STORAGE);
        }
        case OP_LIST_HOMEWORK: {
            uint16_t offset = 0, limit = 0;
            if (!r.u16(offset) || !r.u16(limit)) return makeResponse(req, ST_BAD_REQUEST);
            if (limit == 0 || limit > 100) limit = 100;
            HomeworkBatch batch;
            size_t seen = 0, emitted = 0, used = 3;  // count + more flag
            for (const auto& h : repo_.homework()) {
                if (h.deleted) continue;
                if (seen++ < offset) continue;
                if (emitted >= limit) {
                    batch.more = true;
                    break;
                }
                const size_t size = encodedHomeworkSize(h);
                if (emitted > 0 && used + size > kPageByteBudget) {
                    batch.more = true;
                    break;
                }
                batch.items.push_back(h);
                used += size;
                ++emitted;
            }
            return makeResponse(req, ST_OK, encodeHomeworkBatch(batch));
        }
        case OP_GET_HOMEWORK: {
            std::string id;
            if (!r.fixed(id, kIdLen) || !isValidId(id)) return makeResponse(req, ST_BAD_REQUEST);
            Homework h;
            if (!repo_.getHomework(id, h) || h.deleted) return makeResponse(req, ST_NOT_FOUND);
            ByteWriter w;
            encodeHomework(w, h, 0);
            return makeResponse(req, ST_OK, w.data());
        }
        case OP_PUT_HOMEWORK: {
            Homework in;
            uint32_t base = 0;
            if (!decodeHomework(r, in, base)) return makeResponse(req, ST_BAD_REQUEST);
            Homework stored;
            RepoStatus st = repo_.putHomework(in, base, stored);
            ByteWriter w;
            encodeHomework(w, stored, 0);
            if (st == RepoStatus::Ok) return makeResponse(req, ST_OK, w.data());
            if (st == RepoStatus::Conflict) return makeResponse(req, ST_CONFLICT, w.data());
            if (st == RepoStatus::Storage) return makeResponse(req, ST_STORAGE);
            return makeResponse(req, ST_BAD_REQUEST);
        }
        case OP_DELETE_HOMEWORK: {
            std::string id;
            uint32_t base = 0;
            if (!r.fixed(id, kIdLen) || !r.u32(base) || !isValidId(id))
                return makeResponse(req, ST_BAD_REQUEST);
            Homework stored;
            RepoStatus st = repo_.deleteHomework(id, base, stored);
            if (st == RepoStatus::Ok) return makeResponse(req, ST_OK, id);
            if (st == RepoStatus::Conflict) {
                ByteWriter w;
                encodeHomework(w, stored, 0);
                return makeResponse(req, ST_CONFLICT, w.data());
            }
            if (st == RepoStatus::NotFound) return makeResponse(req, ST_NOT_FOUND);
            return makeResponse(req, ST_STORAGE);
        }
        case OP_GET_SETTINGS: {
            ByteWriter w;
            encodeSettings(w, repo_.settings(), false);
            return makeResponse(req, ST_OK, w.data());
        }
        case OP_PUT_SETTINGS: {
            Settings in = repo_.settings();
            if (!decodeSettings(r, in)) return makeResponse(req, ST_BAD_REQUEST);
            bool includePin = !in.pin.empty();
            RepoStatus st = repo_.applySettings(in, includePin);
            if (st != RepoStatus::Ok) return makeResponse(req, ST_BAD_REQUEST);
            if (hooks_) hooks_->onSettingsChanged(repo_.settings());
            ByteWriter w;
            encodeSettings(w, repo_.settings(), false);
            return makeResponse(req, ST_OK, w.data());
        }
        case OP_LIST_MUSIC: {
            std::vector<MusicTrack> tracks = hooks_ ? hooks_->listMusic() : std::vector<MusicTrack>{};
            return makeResponse(req, ST_OK, encodeMusicBatch(tracks));
        }
        case OP_MUSIC_CONTROL: {
            MusicAction action = MUSIC_TOGGLE;
            int16_t index = -1;
            uint16_t arg = 0;
            if (!decodeMusicControl(r, action, index, arg)) return makeResponse(req, ST_BAD_REQUEST);
            RepoStatus st = hooks_ ? hooks_->musicControl(action, index, arg)
                                   : RepoStatus::BadRequest;
            if (st == RepoStatus::NotFound) return makeResponse(req, ST_NOT_FOUND);
            if (st != RepoStatus::Ok) return makeResponse(req, ST_BAD_REQUEST);
            MusicStatus nowPlaying = hooks_ ? hooks_->nowPlaying() : MusicStatus{};
            return makeResponse(req, ST_OK, encodeMusicStatus(nowPlaying));
        }
        case OP_NOW_PLAYING: {
            MusicStatus st = hooks_ ? hooks_->nowPlaying() : MusicStatus{};
            return makeResponse(req, ST_OK, encodeMusicStatus(st));
        }
        case OP_DELETE_MUSIC: {
            std::string name;
            if (!r.str16(name) || name.empty()) return makeResponse(req, ST_BAD_REQUEST);
            RepoStatus st = hooks_ ? hooks_->deleteMusic(name) : RepoStatus::NotFound;
            if (st == RepoStatus::NotFound) return makeResponse(req, ST_NOT_FOUND);
            if (st != RepoStatus::Ok) return makeResponse(req, ST_STORAGE);
            return makeResponse(req, ST_OK);
        }
        case OP_SYNC_PULL: {
            uint32_t since = 0;
            uint8_t cursor = 0;
            if (!r.u32(since) || !r.u8(cursor)) return makeResponse(req, ST_BAD_REQUEST);
            SyncDelta d = sync_.buildDelta(since, cursor, kPageSize);
            d.serverEpoch = now;
            return makeResponse(req, ST_OK, encodeSyncDelta(d));
        }
        case OP_SYNC_PUSH: {
            SyncPush push;
            if (!decodeSyncPush(r, push)) return makeResponse(req, ST_BAD_REQUEST);
            // Apply the whole batch, then flush persistence once (one flash write).
            repo_.beginBatch();
            SyncResult res = sync_.applyPush(push, now);
            repo_.endBatch();
            return makeResponse(req, ST_OK, encodeSyncResult(res));
        }
        case OP_SYNC_SUMMARY: {
            ByteWriter w;
            w.u32(static_cast<uint32_t>(repo_.noteCount()));
            w.u32(static_cast<uint32_t>(repo_.homeworkCount()));
            w.u32(repo_.maxUpdated());
            return makeResponse(req, ST_OK, w.data());
        }
        default:
            return makeResponse(req, ST_UNKNOWN, "unknown opcode");
    }
}

}  // namespace nexo
