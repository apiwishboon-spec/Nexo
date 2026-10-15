// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Media subsystem for the Cardputer-Adv.
//
// The Adv drives an ES8311 codec over I2S. We hand I2S to ESP32-audioI2S (which
// also decodes MP3), so M5Unified's internal speaker/mic are disabled and this
// module owns codec setup. Timer/flash tones are played from a short WAV that is
// generated on the fly into LittleFS; music is streamed from the microSD card.
#include <cstdint>

namespace nexo {

void audioBegin();
void audioLoop();
bool audioEnabled();
void audioSetEnabled(bool enabled);
uint8_t audioVolume();  // 0..100
void audioSetVolume(uint8_t percent);
void audioBeep(float frequency, uint32_t durationMs);

// microSD music library.
bool musicBegin();  // mounts the SD card and scans for audio; false if no card
int musicCount();
const char* musicTitle(int index);
uint32_t musicTrackSize(int index);
int musicIndex();
bool musicPlaying();
uint16_t musicPositionSec();
uint16_t musicDurationSec();
void musicPlay(int index);
void musicTogglePlay();
void musicStop();
void musicSeek(uint16_t sec);
void musicNext();
void musicPrev();
bool musicDelete(const char* name);
void musicRescan();
void musicSetAutoAdvance(bool on);

// Voice memos: the ES8311 microphone captured into 16 kHz mono WAVs under
// /memos on the microSD card.
//
// Recording takes I2S port 0 away from the decoder for its duration (the
// legacy ESP32-audioI2S driver is released, M5's mic installs its own, and
// the handover back runs codecConfigure()). Playback goes through the normal
// decoder, so a memo is only playable while the recorder is idle.
bool memoBegin();  // mkdir + scan /memos; false when there is no SD card
void memoScan();
int memoCount();
const char* memoTitle(int index);
uint32_t memoSize(int index);
bool memoSdReady();
bool memoRecording();
uint32_t memoSeconds();  // elapsed seconds of the current recording
bool memoRecordStart();
void memoRecordTick();   // pull one mic chunk to the card (call from loop)
bool memoRecordStop();   // finalise the WAV and hand I2S back to the decoder
bool memoDelete(int index);

void memoPlay(int index);
void memoTogglePlay();
void memoStopPlayback();
bool memoPlaying();
int memoIndex();
uint32_t memoPositionSec();
uint32_t memoDurationSec();

}  // namespace nexo
