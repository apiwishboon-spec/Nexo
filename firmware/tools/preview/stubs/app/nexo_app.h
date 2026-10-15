#pragma once
// Preview stub: shadows src/app/nexo_app.h so ui.cpp builds on the host with
// no Arduino / ArduinoJson / filesystem. Not the real app.
#include <cstdint>
#include <string>
#include <vector>

#include "nexo_types.h"

namespace nexo {

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
    std::vector<Note> notes_;
    std::vector<Homework> hw_;

    const std::vector<Note>& notes() const { return notes_; }
    const std::vector<Homework>& homework() const { return hw_; }

    RepoStatus putNote(const Note&, uint32_t, Note&) { return RepoStatus::Ok; }
    RepoStatus putHomework(const Homework&, uint32_t, Homework&) { return RepoStatus::Ok; }
    void deleteNote(const std::string&, uint32_t, Note&) {}
    void persistSettings() {}
};

class NexoApp {
public:
    Repository& repo() { return repo_; }
    Settings& settings() { return settings_; }

private:
    Repository repo_;
    Settings settings_;
};

}  // namespace nexo
