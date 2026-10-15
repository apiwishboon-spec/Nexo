// Host implementation of the media subsystem for the preview/emulator builds.
//
// Unlike the on-device build there is no ES8311/I2S here, so:
//   * beeps are printed to stderr (the Tk front-end rings its bell), and
//   * music is scanned from a real folder and played with the host's own
//     player (`afplay` on macOS), so the Music screen is fully testable and
//     audible without an SD card.
//
// Folder resolution order:
//   1. $NEXO_MUSIC_DIR (if set)
//   * ./dist/sd/music, ../dist/sd/music, ../../dist/sd/music, ./music
// The emulator is launched from the repo root, so `dist/sd/music` is the one
// the "Nexo Emulator.command" uses - drop files there (or on the real SD card).
#include "hw/audio.h"

#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace nexo {
namespace {

bool s_enabled = true;
uint8_t s_volume = 60;
std::string s_dir;
std::vector<std::string> s_tracks;
int s_index = -1;
bool s_playing = false;
pid_t s_pid = -1;

bool hasAudioFile(const std::string& lower)
{
    static const char* exts[] = {".mp3", ".wav", ".m4a", ".aac", ".flac", ".ogg"};
    for (const char* e : exts) {
        size_t n = strlen(e);
        if (lower.size() > n && lower.compare(lower.size() - n, n, e) == 0) return true;
    }
    return false;
}

bool dirExists(const std::string& path)
{
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    closedir(d);
    return true;
}

void resolveDir()
{
    const char* env = getenv("NEXO_MUSIC_DIR");
    const char* candidates[] = {"dist/sd/music", "../dist/sd/music",
                                "../../dist/sd/music", "music"};
    if (env && *env && dirExists(env)) {
        s_dir = env;
        return;
    }
    for (const char* c : candidates) {
        if (dirExists(c)) {
            s_dir = c;
            return;
        }
    }
    s_dir = ".";
}

void scanTracks()
{
    s_tracks.clear();
    resolveDir();
    DIR* d = opendir(s_dir.c_str());
    if (!d) return;
    while (struct dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (!name.empty() && name[0] == '.') continue;  // hide .DS_Store / ._ sidecars
        std::string lower = name;
        for (char& c : lower) c = (char)tolower((unsigned char)c);
        if (hasAudioFile(lower)) s_tracks.push_back(name);
    }
    closedir(d);
}

void stopPlayback()
{
    if (s_pid > 0) {
        kill(s_pid, SIGTERM);
        waitpid(s_pid, nullptr, WNOHANG);
        s_pid = -1;
    }
}

void startPlayback(const std::string& path)
{
    stopPlayback();
    pid_t p = fork();
    if (p == 0) {
        execlp("afplay", "afplay", path.c_str(), (char*)nullptr);
        _exit(127);
    }
    if (p > 0) s_pid = p;
}

}  // namespace

void audioBegin() {}
void audioLoop() {}

bool audioEnabled() { return s_enabled; }
void audioSetEnabled(bool enabled) { s_enabled = enabled; }

uint8_t audioVolume() { return s_volume; }
void audioSetVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    s_volume = percent;
}

void audioBeep(float frequency, uint32_t durationMs) {
    if (!s_enabled) return;
    std::fprintf(stderr, "[beep %.0f %u]\n", frequency, durationMs);
}

bool musicBegin() {
    scanTracks();
    return true;
}
int musicCount() { return (int)s_tracks.size(); }
const char* musicTitle(int index) {
    return (index >= 0 && index < (int)s_tracks.size()) ? s_tracks[index].c_str() : "";
}
int musicIndex() { return s_index; }
bool musicPlaying() { return s_playing; }

void musicPlay(int index) {
    if (index < 0 || index >= (int)s_tracks.size()) return;
    s_index = index;
    s_playing = true;
    std::string full = s_dir + "/" + s_tracks[index];
    std::fprintf(stderr, "[music play %d %s]\n", index, s_tracks[index].c_str());
    if (s_enabled) startPlayback(full);
}
void musicTogglePlay() {
    if (s_index < 0) {
        musicPlay(0);
        return;
    }
    s_playing = !s_playing;
    if (s_playing) {
        startPlayback(s_dir + "/" + s_tracks[s_index]);
    } else {
        stopPlayback();
    }
    std::fprintf(stderr, "[music %s %d]\n", s_playing ? "resume" : "pause", s_index);
}
void musicNext() {
    if (s_tracks.empty()) return;
    int n = (int)s_tracks.size();
    musicPlay((s_index < 0) ? 0 : (s_index + 1) % n);
}
void musicPrev() {
    if (s_tracks.empty()) return;
    int n = (int)s_tracks.size();
    musicPlay((s_index < 0) ? 0 : (s_index - 1 + n) % n);
}

}  // namespace nexo
