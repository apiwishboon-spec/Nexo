// WebAssembly implementation of the media subsystem for the web emulator.
//
// No ES8311/I2S and no filesystem here: the track list is empty (the Music
// screen shows its real empty state) and beeps are forwarded to the page via
// an EM_JS hook (the site wires window.nexoBeep to a WebAudio oscillator).
#include "hw/audio.h"

#include <cstdint>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, nexoemu_emit_beep, (float f, int ms), {
    if (typeof self !== 'undefined' && self.nexoBeep) self.nexoBeep(f, ms);
});
#else
static void nexoemu_emit_beep(float, int) {}
#endif

namespace nexo {
namespace {
bool s_enabled = true;
uint8_t s_volume = 60;
std::vector<std::string> s_tracks;
int s_index = -1;
bool s_playing = false;
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
    nexoemu_emit_beep(frequency, (int)durationMs);
}

bool musicBegin() { return true; }
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
}
void musicTogglePlay() {
    if (s_index < 0) {
        musicPlay(0);
        return;
    }
    s_playing = !s_playing;
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

// Voice memos: no mic or SD in the browser, so everything is a no-op and the
// Memo screen shows its real empty state.
bool memoBegin() { return false; }
void memoScan() {}
int memoCount() { return 0; }
const char* memoTitle(int) { return ""; }
uint32_t memoSize(int) { return 0; }
bool memoSdReady() { return false; }
bool memoRecording() { return false; }
uint32_t memoSeconds() { return 0; }
bool memoRecordStart() { return false; }
void memoRecordTick() {}
bool memoRecordStop() { return false; }
bool memoDelete(int) { return false; }
void memoPlay(int) {}
void memoTogglePlay() {}
void memoStopPlayback() {}
bool memoPlaying() { return false; }
int memoIndex() { return -1; }
uint32_t memoPositionSec() { return 0; }
uint32_t memoDurationSec() { return 0; }

}  // namespace nexo
