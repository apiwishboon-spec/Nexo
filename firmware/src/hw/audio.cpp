// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "audio.h"

#include <Audio.h>
#include <LittleFS.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace nexo {
namespace {

// Cardputer-Adv ES8311 codec + microSD wiring.
constexpr uint8_t kEs8311Addr = 0x18;
constexpr uint8_t kSdSck = 40;
constexpr uint8_t kSdMiso = 39;
constexpr uint8_t kSdMosi = 14;
constexpr uint8_t kSdCs = 12;
// ES8311 I2S wiring (official Cardputer-Adv / Stamp-S3A pinmap):
//   G41 = SCLK/BCLK, G43 = LRCK, G42 = DSDIN (speaker data into the codec).
// G46 is the codec's ASDOUT (microphone data) and G8/G9 are the codec I2C pins.
// There is no amplifier-enable GPIO: the NS4150B is driven straight from the
// codec output and is muted in hardware when headphones are inserted.
constexpr uint8_t kI2sBclk = 41;
constexpr uint8_t kI2sLrck = 43;
constexpr uint8_t kI2sDout = 42;
constexpr const char* kBeepPath = "/beep.wav";
constexpr const char* kMusicDir = "/music";
constexpr size_t kMaxTracks = 200;

Audio s_audio;
Preferences s_prefs;
bool s_enabled = true;
uint8_t s_volume = 60;
bool s_sdReady = false;
bool s_beepReady = false;
bool s_codecReady = false;
bool s_autoAdvance = true;
std::vector<std::string> s_tracks;
std::vector<uint32_t> s_sizes;
int s_index = -1;
bool s_playing = false;
TaskHandle_t s_task = nullptr;
volatile uint16_t s_posSec = 0;
volatile uint16_t s_durSec = 0;

// Requests from the UI task to the audio task (ESP32-audioI2S is only touched
// from the audio task; the UI just posts work here). -1 == no request.
volatile int s_req_play = -1;
volatile bool s_req_toggle = false;
volatile bool s_req_beep = false;
volatile bool s_req_stop = false;
volatile int s_req_seek = -1;    // seconds, -1 == none
volatile int s_req_volume = -1;  // percent, -1 == none

// ---- Voice memos -------------------------------------------------------------
constexpr const char* kMemoDir = "/memos";
constexpr uint32_t kMemoSampleRate = 16000;
constexpr size_t kMemoChunk = 240;               // 15 ms per record() call
constexpr uint32_t kMemoMaxMs = 5ul * 60ul * 1000ul;  // cap each memo at 5 min

std::vector<std::string> s_memos;
std::vector<uint32_t> s_memoSizes;
bool s_memoRecording = false;
File s_memoFile;
uint32_t s_memoDataBytes = 0;
uint32_t s_memoStartMs = 0;
int16_t s_memoChunk[kMemoChunk];

// Memo playback rides on the same decoder, so these track what is playing.
int s_memoIndex = -1;
bool s_memoIsPlaying = false;
volatile int s_req_memo_play = -1;       // index, -1 == none
volatile bool s_req_memo_toggle = false;
volatile bool s_req_memo_stop = false;

// Handshake with the decoder task: recording needs port 0 back from it, so the
// UI asks the task to park itself and only touches I2S once it has confirmed.
volatile bool s_park = false;
volatile bool s_parked = false;

uint8_t toCodecVolume(uint8_t percent) { return (uint8_t)((uint32_t)percent * 21u / 100u); }

void codecWrite(uint8_t reg, uint8_t value)
{
    M5.In_I2C.writeRegister8(kEs8311Addr, reg, value, 100000);
}

uint8_t codecRead(uint8_t reg)
{
    return M5.In_I2C.readRegister8(kEs8311Addr, reg, 100000);
}

// Writes the ES8311 register sequence the Cardputer-Adv needs for speaker
// output. This is byte-for-byte the sequence M5Unified uses for this board's
// speaker callback — note there is no amplifier-enable GPIO involved. It must
// run only while the I2S BCLK is already present, so it is called after the
// decoder's setPinout() (see audioBegin).
void codecConfigure()
{
    static const uint8_t kSeq[][2] = {
        {0x00, 0x80},  // RESET / CSM power on
        {0x01, 0xB5},  // clock manager: MCLK = BCLK
        {0x02, 0x18},  // clock manager: MULT_PRE = 3
        {0x0D, 0x01},  // system: power up analog circuitry
        {0x12, 0x00},  // system: power-up DAC
        {0x13, 0x10},  // system: enable output to HP drive
        {0x32, 0xBF},  // DAC volume (0xBF == 0 dB)
        {0x37, 0x08},  // bypass DAC equalizer
    };
    for (const auto& e : kSeq) {
        codecWrite(e[0], e[1]);
        delay(2);
    }
    s_codecReady = true;

    // Diagnostics: read back the codec id/clk/volume registers.
    uint8_t r00 = codecRead(0x00), r01 = codecRead(0x01), r02 = codecRead(0x02);
    uint8_t r32 = codecRead(0x32);
    Serial.printf("[audio] es8311 ready id(0x00)=0x%02X clk(0x01)=0x%02X mul(0x02)=0x%02X vol(0x32)=0x%02X\n",
                  r00, r01, r02, r32);
    Serial.printf("[audio] pins bclk=%d lrck=%d dout=%d (no amp gpio)\n", kI2sBclk, kI2sLrck,
                  kI2sDout);
}


void writeWavHeader(File& f, uint32_t sampleRate, uint32_t dataBytes)
{
    const uint16_t channels = 1, bits = 16;
    const uint32_t byteRate = sampleRate * channels * bits / 8;
    const uint16_t blockAlign = channels * bits / 8;
    const uint32_t riffLen = 36 + dataBytes;
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    memcpy(h + 4, &riffLen, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    uint32_t fmtLen = 16;
    memcpy(h + 16, &fmtLen, 4);
    uint16_t pcm = 1, ch = channels;
    memcpy(h + 20, &pcm, 2);
    memcpy(h + 22, &ch, 2);
    memcpy(h + 24, &sampleRate, 4);
    memcpy(h + 28, &byteRate, 4);
    memcpy(h + 32, &blockAlign, 2);
    memcpy(h + 34, &bits, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &dataBytes, 4);
    f.write(h, sizeof(h));
}

void ensureBeepWav()
{
    if (s_beepReady) return;
    if (!LittleFS.begin(false)) return;
    const uint32_t sampleRate = 16000, ms = 180;
    const uint32_t samples = sampleRate * ms / 1000;
    const uint32_t dataBytes = samples * 2;
    File f = LittleFS.open(kBeepPath, "w");
    if (!f) return;
    writeWavHeader(f, sampleRate, dataBytes);
    for (uint32_t i = 0; i < samples; ++i) {
        double t = (double)i / sampleRate;
        // Short attack/release envelope to avoid clicks.
        double env = 1.0;
        if (i < 320) env = (double)i / 320;
        if (i > samples - 480) env = (double)(samples - i) / 480;
        int16_t s = (int16_t)(sin(2.0 * M_PI * 1000.0 * t) * 12000 * env);
        f.write((const uint8_t*)&s, sizeof(s));
    }
    f.close();
    s_beepReady = true;
}

const char* baseName(const std::string& path)
{
    size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path.c_str() : path.c_str() + slash + 1;
}

void scanTracks()
{
    s_tracks.clear();
    s_sizes.clear();
    File root = SD.open(kMusicDir);
    if (!root) return;
    File entry;
    while ((entry = root.openNextFile()) && s_tracks.size() < kMaxTracks) {
        if (!entry.isDirectory()) {
            std::string name = entry.name();
            // Skip hidden entries: macOS leaves ".DS_Store" and "._<track>.mp3"
            // AppleDouble sidecars that would otherwise spam the player list.
            const char* base = baseName(name);
            if (base[0] == '\0' || base[0] == '.') {
                entry.close();
                continue;
            }
            std::string lower = name;
            for (char& c : lower) c = (char)tolower((unsigned char)c);
            if (lower.size() > 4 &&
                (lower.compare(lower.size() - 4, 4, ".mp3") == 0 ||
                 lower.compare(lower.size() - 4, 4, ".wav") == 0)) {
                if (name.empty() || name[0] != '/') name = "/" + name;
                s_tracks.push_back(name);
                s_sizes.push_back(static_cast<uint32_t>(entry.size()));
            }
        }
        entry.close();
    }
    root.close();
}

}  // namespace

// Dedicated decoder task (core 1). Owns every ESP32-audioI2S call so the
// decoder is fed at ~1 kHz regardless of what the UI/BLE/WiFi loop is doing.
void audioTask(void*)
{
    for (;;) {
        // Recording needs port 0: sit out of I2S until the UI lets us back in.
        if (s_park) {
            s_parked = true;
            vTaskDelay(1);
            continue;
        }
        s_parked = false;

        int vol = s_req_volume;
        if (vol >= 0) {
            s_req_volume = -1;
            s_audio.setVolume(toCodecVolume((uint8_t)vol));
        }

        if (s_req_beep) {
            s_req_beep = false;
            if (s_beepReady) {
                if (s_audio.isRunning()) s_audio.stopSong();
                s_playing = false;
                s_memoIsPlaying = false;
                s_audio.connecttoFS(LittleFS, kBeepPath);
            }
        }

        int req = s_req_play;
        if (req >= 0) {
            s_req_play = -1;
            if (s_sdReady && req < (int)s_tracks.size()) {
                s_audio.stopSong();
                bool ok = s_audio.connecttoFS(SD, s_tracks[req].c_str());
                Serial.printf("[audio] play %d '%s' -> %s running=%d rate=%u\n", req,
                              s_tracks[req].c_str(), ok ? "ok" : "FAIL", (int)s_audio.isRunning(),
                              (unsigned)s_audio.getSampleRate());
                if (ok) {
                    s_index = req;
                    s_playing = true;
                    s_memoIndex = -1;
                    s_memoIsPlaying = false;
                } else {
                    s_playing = false;
                }
            }
        }

        int mreq = s_req_memo_play;
        if (mreq >= 0) {
            s_req_memo_play = -1;
            if (s_sdReady && mreq < (int)s_memos.size()) {
                s_audio.stopSong();
                bool ok = s_audio.connecttoFS(SD, s_memos[mreq].c_str());
                if (ok) {
                    s_memoIndex = mreq;
                    s_memoIsPlaying = true;
                    s_index = -1;  // a memo and a music track never play together
                    s_playing = false;
                } else {
                    s_memoIsPlaying = false;
                }
            }
        }

        if (s_req_memo_toggle) {
            s_req_memo_toggle = false;
            if (s_memoIndex >= 0) {
                s_audio.pauseResume();
                s_memoIsPlaying = !s_memoIsPlaying;
            }
        }

        if (s_req_memo_stop) {
            s_req_memo_stop = false;
            if (s_audio.isRunning()) s_audio.stopSong();
            s_memoIsPlaying = false;
            s_posSec = 0;
            s_durSec = 0;
        }

        if (s_req_toggle) {
            s_req_toggle = false;
            if (s_index >= 0) {
                s_audio.pauseResume();
                s_playing = !s_playing;
            }
        }

        if (s_req_stop) {
            s_req_stop = false;
            if (s_audio.isRunning()) s_audio.stopSong();
            s_playing = false;
            s_posSec = 0;
            s_durSec = 0;
        }

        int seek = s_req_seek;
        if (seek >= 0) {
            s_req_seek = -1;
            if (s_index >= 0) s_audio.setAudioPlayPosition((uint16_t)seek);
        }

        if (s_playing || s_memoIsPlaying) {
            s_audio.loop();
            s_posSec = (uint16_t)s_audio.getAudioCurrentTime();
            s_durSec = (uint16_t)s_audio.getAudioFileDuration();
            vTaskDelay(1);
        } else {
            vTaskDelay(20);
        }
    }
}

void audioBegin()
{
    s_prefs.begin("nexo", true);
    s_enabled = s_prefs.getBool("sndOn", true);
    s_volume = s_prefs.getUChar("sndVol", 60);
    s_prefs.end();
    if (s_volume > 100) s_volume = 100;

    // ESP32-audioI2S owns the I2S port (installed in its constructor). Attach the
    // pins first so BCLK/LRCK are running, *then* configure the ES8311: the codec
    // will not lock onto a clock that appears after it has already powered up.
    // This ordering matches M5Unified's Cardputer-Adv speaker sequence.
    bool pinsOk =
        s_audio.setPinout(kI2sBclk, kI2sLrck, kI2sDout, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE);
    delay(10);
    codecConfigure();
    s_audio.setVolume(toCodecVolume(s_enabled ? s_volume : 0));
    s_audio.setBalance(0);
    ensureBeepWav();
    Serial.printf("[audio] decoder i2s pins=%s vol=%d\n", pinsOk ? "ok" : "FAIL",
                  (int)toCodecVolume(s_enabled ? s_volume : 0));

    // Decoder runs on its own core-1 task so it always gets fed.
    xTaskCreatePinnedToCore(audioTask, "nexo_audio", 8192, nullptr, 3, &s_task, 1);

    // Startup chime through the decoder: proves the whole decode->I2S path.
    if (s_enabled && s_beepReady) s_req_beep = true;
}

void audioLoop() {}

bool audioEnabled() { return s_enabled; }

void audioSetEnabled(bool enabled)
{
    s_enabled = enabled;
    s_req_volume = enabled ? s_volume : 0;
    s_prefs.begin("nexo", false);
    s_prefs.putBool("sndOn", s_enabled);
    s_prefs.end();
}

uint8_t audioVolume() { return s_volume; }

void audioSetVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_volume = percent;
    s_req_volume = s_enabled ? s_volume : 0;
    s_prefs.begin("nexo", false);
    s_prefs.putUChar("sndVol", s_volume);
    s_prefs.end();
}

void audioBeep(float frequency, uint32_t durationMs)
{
    (void)frequency;
    (void)durationMs;
    if (!s_enabled || !s_beepReady) return;
    s_req_beep = true;
}

bool musicBegin()
{
    SPI.begin(kSdSck, kSdMiso, kSdMosi);
    if (!SD.begin(kSdCs)) return false;
    s_sdReady = true;
    // Drop-in location for tracks: put .mp3/.wav files in /music on the SD card.
    if (!SD.exists(kMusicDir)) SD.mkdir(kMusicDir);
    scanTracks();
    return true;
}

int musicCount() { return (int)s_tracks.size(); }

const char* musicTitle(int index)
{
    if (index < 0 || index >= (int)s_tracks.size()) return "";
    return baseName(s_tracks[index]);
}

uint32_t musicTrackSize(int index)
{
    if (index < 0 || index >= (int)s_sizes.size()) return 0;
    return s_sizes[index];
}

int musicIndex() { return s_index; }

bool musicPlaying() { return s_playing; }

uint16_t musicPositionSec() { return s_posSec; }

uint16_t musicDurationSec() { return s_durSec; }

void musicPlay(int index)
{
    if (!s_sdReady || index < 0 || index >= (int)s_tracks.size()) {
        Serial.printf("[audio] play rejected (sdReady=%d index=%d count=%d)\n", s_sdReady, index,
                      (int)s_tracks.size());
        return;
    }
    s_index = index;   // optimistic so the UI highlights immediately
    s_playing = true;  // confirmed/cleared by the audio task
    s_req_play = index;
}

void musicTogglePlay()
{
    if (s_index < 0) {
        if (!s_tracks.empty()) musicPlay(0);
        return;
    }
    s_req_toggle = true;
}

void musicStop() { s_req_stop = true; }

void musicSeek(uint16_t sec)
{
    if (s_index >= 0) s_req_seek = (int)sec;
}

void musicNext()
{
    if (s_tracks.empty()) return;
    int next = (s_index < 0) ? 0 : (s_index + 1) % (int)s_tracks.size();
    musicPlay(next);
}

void musicPrev()
{
    if (s_tracks.empty()) return;
    int n = (int)s_tracks.size();
    int prev = (s_index < 0) ? 0 : (s_index - 1 + n) % n;
    musicPlay(prev);
}

// Deletes a track from /music by base name; rescans the library afterwards.
bool musicDelete(const char* name)
{
    if (!s_sdReady || !name || !*name) return false;
    std::string path = std::string(kMusicDir) + "/" + baseName(name);
    if (!SD.exists(path.c_str())) return false;
    bool ok = SD.remove(path.c_str());
    if (ok) {
        if (s_index >= 0) {
            musicStop();
            s_index = -1;
        }
        scanTracks();
    }
    return ok;
}

void musicRescan() { scanTracks(); }

void musicSetAutoAdvance(bool on) { s_autoAdvance = on; }

// ---------------------------------------------------------------------------
// Voice memos (16 kHz mono WAVs under /memos).
//
// The decoder and M5's mic both want I2S port 0, so a recording hands the port
// over: park the decoder task, release its legacy driver, start the mic, and on
// the way back reinstall the decoder, re-attach BCLK/LRCK (the codec derives
// MCLK from BCLK) and re-apply the DAC register sequence the mic's codec reset
// cleared.
// ---------------------------------------------------------------------------
namespace {

void memoParkAudioTask()
{
    s_park = true;
    uint32_t start = millis();
    while (!s_parked && millis() - start < 1000) vTaskDelay(1);
}

void memoUnparkAudioTask() { s_park = false; }

void memoHandbackI2S()
{
    // Port 0 is free again: reinstall the decoder's driver and its pins.
    s_audio.setI2SCommFMT_LSB(false);
    s_audio.setPinout(kI2sBclk, kI2sLrck, kI2sDout, I2S_PIN_NO_CHANGE, I2S_PIN_NO_CHANGE);
    delay(10);
    codecConfigure();  // the mic's codec write reset the DAC path
    s_audio.setVolume(toCodecVolume(s_enabled ? s_volume : 0));
}

}  // namespace

void memoScan()
{
    s_memos.clear();
    s_memoSizes.clear();
    File root = SD.open(kMemoDir);
    if (!root) return;
    File entry;
    while ((entry = root.openNextFile()) && s_memos.size() < kMaxTracks) {
        if (!entry.isDirectory()) {
            std::string name = entry.name();
            const char* base = baseName(name);
            if (base[0] == '\0' || base[0] == '.') {
                entry.close();
                continue;
            }
            std::string lower = name;
            for (char& c : lower) c = (char)tolower((unsigned char)c);
            if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".wav") == 0) {
                if (name.empty() || name[0] != '/') name = std::string(kMemoDir) + "/" + name;
                s_memos.push_back(name);
                s_memoSizes.push_back(static_cast<uint32_t>(entry.size()));
            }
        }
        entry.close();
    }
    root.close();
}

bool memoBegin()
{
    if (!s_sdReady) return false;
    if (!SD.exists(kMemoDir)) SD.mkdir(kMemoDir);
    memoScan();
    return true;
}

int memoCount() { return (int)s_memos.size(); }

const char* memoTitle(int index)
{
    if (index < 0 || index >= (int)s_memos.size()) return "";
    return baseName(s_memos[index]);
}

uint32_t memoSize(int index)
{
    if (index < 0 || index >= (int)s_memoSizes.size()) return 0;
    return s_memoSizes[index];
}

bool memoSdReady() { return s_sdReady; }
bool memoRecording() { return s_memoRecording; }

uint32_t memoSeconds()
{
    if (!s_memoRecording) return 0;
    return (millis() - s_memoStartMs) / 1000;
}

bool memoRecordStart()
{
    if (s_memoRecording || !s_sdReady) return false;

    // Park the decoder first: it must not touch port 0 while we take it over.
    memoParkAudioTask();
    if (s_audio.isRunning()) s_audio.stopSong();
    s_playing = false;
    s_memoIsPlaying = false;
    s_index = -1;
    s_memoIndex = -1;
    s_posSec = 0;
    s_durSec = 0;

    // Only one driver may own a port: drop the decoder's legacy driver so the
    // mic's channel can be installed there.
    i2s_driver_uninstall((i2s_port_t)I2S_NUM_0);

    if (!M5Cardputer.Mic.begin()) {
        Serial.println("[audio] mic begin failed, keeping I2S for the decoder");
        memoHandbackI2S();
        memoUnparkAudioTask();
        return false;
    }

    char path[48];
    path[0] = '\0';
    for (int n = 0; n < 10000; ++n) {
        snprintf(path, sizeof(path), "%s/memo_%04d.wav", kMemoDir, n);
        if (!SD.exists(path)) break;
        path[0] = '\0';
    }
    s_memoFile = path[0] ? SD.open(path, FILE_WRITE) : File();
    if (!s_memoFile) {
        Serial.println("[audio] cannot create memo file");
        M5Cardputer.Mic.end();
        memoHandbackI2S();
        memoUnparkAudioTask();
        return false;
    }

    // Placeholder header; the real sizes are patched in at memoRecordStop().
    writeWavHeader(s_memoFile, kMemoSampleRate, 0);
    s_memoDataBytes = 0;
    s_memoStartMs = millis();
    s_memoRecording = true;
    s_req_memo_stop = false;
    Serial.printf("[audio] recording %s\n", path);
    return true;
}

void memoRecordTick()
{
    if (!s_memoRecording) return;
    if (M5Cardputer.Mic.record(s_memoChunk, kMemoChunk, kMemoSampleRate)) {
        size_t bytes = kMemoChunk * sizeof(int16_t);
        s_memoFile.write((const uint8_t*)s_memoChunk, bytes);
        s_memoDataBytes += (uint32_t)bytes;
    }
    if (millis() - s_memoStartMs >= kMemoMaxMs) memoRecordStop();
}

bool memoRecordStop()
{
    if (!s_memoRecording) return false;
    s_memoRecording = false;

    if (s_memoFile) {
        s_memoFile.flush();
        if (s_memoFile.seek(0)) writeWavHeader(s_memoFile, kMemoSampleRate, s_memoDataBytes);
        s_memoFile.close();
    }

    M5Cardputer.Mic.end();  // releases port 0 and powers the codec down
    memoHandbackI2S();
    memoUnparkAudioTask();
    memoScan();
    Serial.printf("[audio] memo saved (%u bytes)\n", (unsigned)s_memoDataBytes);
    return true;
}

bool memoDelete(int index)
{
    if (!s_sdReady || index < 0 || index >= (int)s_memos.size()) return false;
    if (s_memoRecording) return false;
    if (s_memoIsPlaying && s_memoIndex == index) memoStopPlayback();
    const std::string& path = s_memos[index];
    if (!SD.exists(path.c_str())) return false;
    if (!SD.remove(path.c_str())) return false;
    if (s_memoIndex == index) {
        s_memoIndex = -1;
        s_memoIsPlaying = false;
    } else if (s_memoIndex > index) {
        s_memoIndex--;
    }
    memoScan();
    return true;
}

void memoPlay(int index)
{
    if (!s_sdReady || s_memoRecording) return;
    if (index < 0 || index >= (int)s_memos.size()) return;
    s_memoIndex = index;
    s_memoIsPlaying = true;  // optimistic; cleared if the decoder cannot open it
    s_req_memo_play = index;
}

void memoTogglePlay()
{
    if (s_memoIndex < 0) {
        if (!s_memos.empty()) memoPlay(0);
        return;
    }
    s_req_memo_toggle = true;
}

void memoStopPlayback() { s_req_memo_stop = true; }

bool memoPlaying() { return s_memoIsPlaying; }
int memoIndex() { return s_memoIndex; }
uint32_t memoPositionSec() { return s_posSec; }
uint32_t memoDurationSec() { return s_durSec; }


// Called from the decoder task when a file finishes -> advance to next track.
void audioOnEof()
{
    if (s_memoIsPlaying) {  // a memo is a single file; never auto-advance it
        s_memoIsPlaying = false;
        return;
    }
    if (s_tracks.empty()) return;
    s_playing = false;
    if (!s_autoAdvance) return;
    s_req_play = (s_index < 0) ? 0 : (s_index + 1) % (int)s_tracks.size();
}

}  // namespace nexo

// Weak symbol the ESP32-audioI2S decoder calls when an MP3 reaches EOF.
void audio_eof_mp3(const char* info)
{
    (void)info;
    nexo::audioOnEof();
}
