// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "nexo_repository.h"

#include <ArduinoJson.h>

namespace nexo {

static const char* kNotesDoc = "notes.json";
static const char* kHomeworkDoc = "homework.json";
static const char* kSettingsDoc = "settings.json";

static void noteToJson(const Note& n, JsonObject o)
{
    o["id"] = n.id;
    o["title"] = n.title;
    o["body"] = n.body;
    o["created"] = n.created;
    o["updated"] = n.updated;
    o["rev"] = n.rev;
    o["del"] = n.deleted;
}

static void jsonToNote(JsonObjectConst o, Note& n)
{
    n.id = o["id"] | "";
    n.title = o["title"] | "";
    n.body = o["body"] | "";
    n.created = o["created"] | 0u;
    n.updated = o["updated"] | 0u;
    n.rev = o["rev"] | 0u;
    n.deleted = o["del"] | false;
}

static void hwToJson(const Homework& h, JsonObject o)
{
    o["id"] = h.id;
    o["subject"] = h.subject;
    o["desc"] = h.description;
    o["due"] = h.dueEpochDay;
    o["status"] = static_cast<uint8_t>(h.status);
    o["created"] = h.created;
    o["updated"] = h.updated;
    o["rev"] = h.rev;
    o["del"] = h.deleted;
}

static void jsonToHw(JsonObjectConst o, Homework& h)
{
    h.id = o["id"] | "";
    h.subject = o["subject"] | "";
    h.description = o["desc"] | "";
    h.dueEpochDay = o["due"] | 0u;
    uint8_t st = o["status"] | 0;
    h.status = static_cast<HomeworkStatus>(st > 2 ? 2 : st);
    h.created = o["created"] | 0u;
    h.updated = o["updated"] | 0u;
    h.rev = o["rev"] | 0u;
    h.deleted = o["del"] | false;
}

Repository::Repository(IStore& store, std::function<uint32_t()> now,
                       std::function<std::string()> newId)
    : store_(store), now_(std::move(now)), newId_(std::move(newId))
{
}

bool Repository::begin()
{
    std::string s;
    if (store_.load(kNotesDoc, s) && !s.empty()) {
        JsonDocument doc;
        if (deserializeJson(doc, s) == DeserializationError::Ok && doc.is<JsonArray>()) {
            notes_.clear();
            for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
                Note n;
                jsonToNote(o, n);
                if (isValidId(n.id)) notes_.push_back(std::move(n));
            }
        }
    }
    if (store_.load(kHomeworkDoc, s) && !s.empty()) {
        JsonDocument doc;
        if (deserializeJson(doc, s) == DeserializationError::Ok && doc.is<JsonArray>()) {
            homework_.clear();
            for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
                Homework h;
                jsonToHw(o, h);
                if (isValidId(h.id)) homework_.push_back(std::move(h));
            }
        }
    }
    if (store_.load(kSettingsDoc, s) && !s.empty()) {
        JsonDocument doc;
        if (deserializeJson(doc, s) == DeserializationError::Ok && doc.is<JsonObject>()) {
            JsonObjectConst o = doc.as<JsonObjectConst>();
            settings_.deviceName = o["deviceName"] | "Nexo";
            settings_.apSsid = o["apSsid"] | "Nexo-AP";
            settings_.apEnabled = o["apEnabled"] | false;
            settings_.volume = o["volume"] | 60;
            settings_.soundEnabled = o["soundEnabled"] | true;
            settings_.brightness = o["brightness"] | 100;
            settings_.screenTimeoutSec = o["screenTimeoutSec"] | 30;
            settings_.autoAdvanceMusic = o["autoAdvanceMusic"] | true;
            settings_.timezoneOffsetMin = o["tzOffsetMin"] | 0;
            if (o["pin"].is<const char*>()) {
                settings_.pin = o["pin"].as<const char*>();
                settingsPinSet_ = true;
            }
        }
    }
    return true;
}

bool Repository::validateNote(const Note& n) const
{
    if (!isValidId(n.id)) return false;
    if (n.title.size() > kMaxTitleLen) return false;
    if (n.body.size() > kMaxBodyLen) return false;
    return true;
}

bool Repository::validateHomework(const Homework& h) const
{
    if (!isValidId(h.id)) return false;
    if (h.subject.size() > kMaxSubjectLen) return false;
    if (h.description.size() > kMaxDescLen) return false;
    if (!isValidDueDay(h.dueEpochDay)) return false;
    if (static_cast<uint8_t>(h.status) > 2) return false;
    return true;
}

Note* Repository::findNote(const std::string& id)
{
    for (auto& n : notes_)
        if (n.id == id) return &n;
    return nullptr;
}

Homework* Repository::findHomework(const std::string& id)
{
    for (auto& h : homework_)
        if (h.id == id) return &h;
    return nullptr;
}

bool Repository::getNote(const std::string& id, Note& out) const
{
    for (const auto& n : notes_) {
        if (n.id == id) {
            out = n;
            return true;
        }
    }
    return false;
}

bool Repository::getHomework(const std::string& id, Homework& out) const
{
    for (const auto& h : homework_) {
        if (h.id == id) {
            out = h;
            return true;
        }
    }
    return false;
}

RepoStatus Repository::putNote(const Note& in, uint32_t baseRev, Note& stored)
{
    Note n = in;
    n.title = clampUtf8(n.title, kMaxTitleLen);
    n.body = clampUtf8(n.body, kMaxBodyLen);
    if (n.id.empty()) n.id = newId_();
    if (!validateNote(n)) return RepoStatus::BadRequest;

    uint32_t ts = now_();
    Note* existing = findNote(n.id);
    if (!existing) {
        // Create.
        n.rev = 1;
        n.created = n.created ? n.created : ts;
        n.updated = ts;
        notes_.push_back(n);
        if (!persistNotes()) return RepoStatus::Storage;
        stored = n;
        return RepoStatus::Ok;
    }
    if (baseRev != 0 && existing->rev != baseRev) {
        stored = *existing;
        return RepoStatus::Conflict;
    }
    if (baseRev == 0) {
        // Client thinks this is new but it exists. Idempotent if identical.
        if (existing->deleted == n.deleted && existing->title == n.title &&
            existing->body == n.body) {
            stored = *existing;
            return RepoStatus::Ok;
        }
        stored = *existing;
        return RepoStatus::Conflict;
    }
    existing->title = n.title;
    existing->body = n.body;
    existing->deleted = n.deleted;
    existing->updated = ts;
    existing->rev = existing->rev + 1;
    if (!persistNotes()) return RepoStatus::Storage;
    stored = *existing;
    return RepoStatus::Ok;
}

RepoStatus Repository::putHomework(const Homework& in, uint32_t baseRev, Homework& stored)
{
    Homework h = in;
    h.subject = clampUtf8(h.subject, kMaxSubjectLen);
    h.description = clampUtf8(h.description, kMaxDescLen);
    if (h.id.empty()) h.id = newId_();
    if (!validateHomework(h)) return RepoStatus::BadRequest;

    uint32_t ts = now_();
    Homework* existing = findHomework(h.id);
    if (!existing) {
        h.rev = 1;
        h.created = h.created ? h.created : ts;
        h.updated = ts;
        homework_.push_back(h);
        if (!persistHomework()) return RepoStatus::Storage;
        stored = h;
        return RepoStatus::Ok;
    }
    if (baseRev != 0 && existing->rev != baseRev) {
        stored = *existing;
        return RepoStatus::Conflict;
    }
    if (baseRev == 0) {
        if (existing->deleted == h.deleted && existing->subject == h.subject &&
            existing->description == h.description && existing->dueEpochDay == h.dueEpochDay &&
            existing->status == h.status) {
            stored = *existing;
            return RepoStatus::Ok;
        }
        stored = *existing;
        return RepoStatus::Conflict;
    }
    existing->subject = h.subject;
    existing->description = h.description;
    existing->dueEpochDay = h.dueEpochDay;
    existing->status = h.status;
    existing->deleted = h.deleted;
    existing->updated = ts;
    existing->rev = existing->rev + 1;
    if (!persistHomework()) return RepoStatus::Storage;
    stored = *existing;
    return RepoStatus::Ok;
}

RepoStatus Repository::deleteNote(const std::string& id, uint32_t baseRev, Note& stored)
{
    Note* existing = findNote(id);
    if (!existing) return RepoStatus::NotFound;
    if (baseRev != 0 && existing->rev != baseRev) {
        stored = *existing;
        return RepoStatus::Conflict;
    }
    existing->deleted = true;
    existing->updated = now_();
    existing->rev = existing->rev + 1;
    if (!persistNotes()) return RepoStatus::Storage;
    stored = *existing;
    return RepoStatus::Ok;
}

RepoStatus Repository::deleteHomework(const std::string& id, uint32_t baseRev, Homework& stored)
{
    Homework* existing = findHomework(id);
    if (!existing) return RepoStatus::NotFound;
    if (baseRev != 0 && existing->rev != baseRev) {
        stored = *existing;
        return RepoStatus::Conflict;
    }
    existing->deleted = true;
    existing->updated = now_();
    existing->rev = existing->rev + 1;
    if (!persistHomework()) return RepoStatus::Storage;
    stored = *existing;
    return RepoStatus::Ok;
}

RepoStatus Repository::applySettings(const Settings& in, bool includePin)
{
    if (in.deviceName.size() > kMaxNameLen) return RepoStatus::BadRequest;
    if (in.apSsid.size() > kMaxNameLen) return RepoStatus::BadRequest;
    settings_.deviceName = clampUtf8(in.deviceName, kMaxNameLen);
    settings_.apSsid = clampUtf8(in.apSsid, kMaxNameLen);
    settings_.apEnabled = in.apEnabled;
    settings_.volume = in.volume > 100 ? 100 : in.volume;
    settings_.soundEnabled = in.soundEnabled;
    settings_.brightness = in.brightness > 100 ? 100 : in.brightness;
    settings_.screenTimeoutSec = in.screenTimeoutSec;
    settings_.autoAdvanceMusic = in.autoAdvanceMusic;
    settings_.timezoneOffsetMin = in.timezoneOffsetMin;
    if (includePin && !in.pin.empty()) {
        if (in.pin.size() < kMinPinLen || in.pin.size() > kMaxPinLen)
            return RepoStatus::BadRequest;
        settings_.pin = in.pin;
        settingsPinSet_ = true;
    }
    if (!persistSettings()) return RepoStatus::Storage;
    return RepoStatus::Ok;
}

uint32_t Repository::maxUpdated() const
{
    uint32_t m = 0;
    for (const auto& n : notes_)
        if (n.updated > m) m = n.updated;
    for (const auto& h : homework_)
        if (h.updated > m) m = h.updated;
    return m;
}

void Repository::pruneTombstones(uint32_t now, uint32_t maxAgeSeconds)
{
    bool changedN = false, changedH = false;
    for (size_t i = 0; i < notes_.size();) {
        if (notes_[i].deleted && now > notes_[i].updated &&
            now - notes_[i].updated > maxAgeSeconds) {
            notes_.erase(notes_.begin() + i);
            changedN = true;
        } else {
            ++i;
        }
    }
    for (size_t i = 0; i < homework_.size();) {
        if (homework_[i].deleted && now > homework_[i].updated &&
            now - homework_[i].updated > maxAgeSeconds) {
            homework_.erase(homework_.begin() + i);
            changedH = true;
        } else {
            ++i;
        }
    }
    if (changedN) persistNotes();
    if (changedH) persistHomework();
}

bool Repository::persistNotes()
{
    if (deferPersist_) {
        notesDirty_ = true;
        return true;
    }
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& n : notes_) {
        JsonObject o = arr.add<JsonObject>();
        noteToJson(n, o);
    }
    std::string out;
    serializeJson(doc, out);
    return store_.save(kNotesDoc, out);
}

bool Repository::persistHomework()
{
    if (deferPersist_) {
        homeworkDirty_ = true;
        return true;
    }
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& h : homework_) {
        JsonObject o = arr.add<JsonObject>();
        hwToJson(h, o);
    }
    std::string out;
    serializeJson(doc, out);
    return store_.save(kHomeworkDoc, out);
}

bool Repository::persistSettings()
{
    if (deferPersist_) {
        settingsDirty_ = true;
        return true;
    }
    JsonDocument doc;
    JsonObject o = doc.to<JsonObject>();
    o["deviceName"] = settings_.deviceName;
    o["apSsid"] = settings_.apSsid;
    o["apEnabled"] = settings_.apEnabled;
    o["volume"] = settings_.volume;
    o["soundEnabled"] = settings_.soundEnabled;
    o["brightness"] = settings_.brightness;
    o["screenTimeoutSec"] = settings_.screenTimeoutSec;
    o["autoAdvanceMusic"] = settings_.autoAdvanceMusic;
    o["tzOffsetMin"] = settings_.timezoneOffsetMin;
    o["pin"] = settings_.pin;  // stored, not returned over the read path
    std::string out;
    serializeJson(doc, out);
    return store_.save(kSettingsDoc, out);
}

bool Repository::endBatch()
{
    deferPersist_ = false;
    bool ok = true;
    if (notesDirty_) {
        notesDirty_ = false;
        ok = persistNotes() && ok;
    }
    if (homeworkDirty_) {
        homeworkDirty_ = false;
        ok = persistHomework() && ok;
    }
    if (settingsDirty_) {
        settingsDirty_ = false;
        ok = persistSettings() && ok;
    }
    return ok;
}

}  // namespace nexo
