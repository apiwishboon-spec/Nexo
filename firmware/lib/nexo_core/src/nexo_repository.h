// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// In-memory CRUD repository with JSON persistence through IStore.
#include <functional>
#include <string>
#include <vector>

#include "nexo_storage.h"
#include "nexo_types.h"

namespace nexo {

// Protocol status codes returned by mutating operations.
enum class RepoStatus : uint8_t {
    Ok = 0x00,
    Unknown = 0x01,
    BadRequest = 0x02,
    NotFound = 0x03,
    Conflict = 0x05,
    Storage = 0x07,
};

class Repository {
public:
    Repository(IStore& store, std::function<uint32_t()> now,
               std::function<std::string()> newId);

    // Loads all documents; returns false on unrecoverable failure.
    bool begin();

    const std::vector<Note>& notes() const { return notes_; }
    const std::vector<Homework>& homework() const { return homework_; }
    const Settings& settings() const { return settings_; }
    Settings& settingsMutable() { return settings_; }

    bool getNote(const std::string& id, Note& out) const;
    bool getHomework(const std::string& id, Homework& out) const;

    // baseRev == 0 means "create". Returns Conflict and fills `current` when the
    // server revision does not match.
    RepoStatus putNote(const Note& in, uint32_t baseRev, Note& stored);
    RepoStatus putHomework(const Homework& in, uint32_t baseRev, Homework& stored);

    RepoStatus deleteNote(const std::string& id, uint32_t baseRev, Note& stored);
    RepoStatus deleteHomework(const std::string& id, uint32_t baseRev, Homework& stored);

    RepoStatus applySettings(const Settings& in, bool includePin);

    // Counts including tombstones; deleted counts separately.
    size_t noteCount() const { return notes_.size(); }
    size_t homeworkCount() const { return homework_.size(); }
    uint32_t maxUpdated() const;

    // Drops tombstones older than maxAgeSeconds relative to `now`.
    void pruneTombstones(uint32_t now, uint32_t maxAgeSeconds);

    bool persistNotes();
    bool persistHomework();
    bool persistSettings();

    // Batches persistence: while active, persist* calls mark documents dirty
    // instead of writing, and endBatch() flushes them once. Used to turn a sync
    // push of N records into a single LittleFS write instead of N.
    void beginBatch() { deferPersist_ = true; }
    bool endBatch();

private:
    bool validateNote(const Note& n) const;
    bool validateHomework(const Homework& h) const;
    Note* findNote(const std::string& id);
    Homework* findHomework(const std::string& id);

    IStore& store_;
    std::function<uint32_t()> now_;
    std::function<std::string()> newId_;
    std::vector<Note> notes_;
    std::vector<Homework> homework_;
    Settings settings_;
    bool settingsPinSet_ = false;
    bool deferPersist_ = false;
    bool notesDirty_ = false;
    bool homeworkDirty_ = false;
    bool settingsDirty_ = false;
};

}  // namespace nexo
