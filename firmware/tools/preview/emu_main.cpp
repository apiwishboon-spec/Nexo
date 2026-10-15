// Interactive host emulator core for the Nexo firmware UI.
//
// Runs the real src/ui/ui.cpp against the fake M5Cardputer display, and speaks
// a tiny line protocol with the Tk front-end (emu.py):
//
//   in : "press <up|down|left|right|enter|esc|backspace|del|tab|space>"
//        "char <c>"
//        "tick"
//        "quit"
//   out: a length-prefixed P6 PPM of the current 240x135 frame, after every
//        command (4-byte little-endian length, then that many bytes).
#define NEXO_UI_PREVIEW 1
#define PROGMEM

#include <M5Cardputer.h>

#include "glcdfont.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "ui.h"
#include "nexo_calc.h"
#include "hw/audio.h"

static uint32_t nowMs() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}
uint32_t millis() { return nowMs(); }

void FakeDisplay::print(const char* s) {
    int size = textSize_;
    int x = cx_, y = cy_;
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        unsigned char ch = *p;
        for (int col = 0; col < 5; ++col) {
            unsigned char bits = font[ch * 5 + col];
            for (int row = 0; row < 8; ++row) {
                bool on = bits & (1u << row);
                if (on) {
                    for (int sy = 0; sy < size; ++sy)
                        for (int sx = 0; sx < size; ++sx)
                            put(x + col * size + sx, y + row * size + sy, (uint16_t)fg_);
                } else if (hasBg_) {
                    for (int sy = 0; sy < size; ++sy)
                        for (int sx = 0; sx < size; ++sx)
                            put(x + col * size + sx, y + row * size + sy, (uint16_t)bg_);
                }
            }
        }
        if (hasBg_)
            for (int row = 0; row < 8; ++row)
                for (int sy = 0; sy < size; ++sy)
                    for (int sx = 0; sx < size; ++sx)
                        put(x + 5 * size + sx, y + row * size + sy, (uint16_t)bg_);
        x += 6 * size;
    }
    cx_ = x;
}

namespace nexo {
struct UiPreview {
    static void draw(Ui& u) { u.draw(); }
};
}  // namespace nexo

using namespace nexo;

static std::vector<uint8_t> gFrame;

static void sendFrame() {
    const FakeDisplay& d = M5Cardputer.Display;
    char hdr[32];
    int hl = snprintf(hdr, sizeof(hdr), "P6\n%d %d\n255\n", FakeDisplay::W, FakeDisplay::H);
    uint32_t n = (uint32_t)hl + FakeDisplay::W * FakeDisplay::H * 3;
    uint8_t len[4] = {(uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24)};
    fwrite(len, 1, 4, stdout);
    fwrite(hdr, 1, hl, stdout);
    gFrame.resize(FakeDisplay::W * FakeDisplay::H * 3);
    size_t k = 0;
    for (int i = 0; i < FakeDisplay::W * FakeDisplay::H; ++i) {
        uint16_t v = d.fb[i];
        int r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
        gFrame[k++] = (uint8_t)((r5 << 3) | (r5 >> 2));
        gFrame[k++] = (uint8_t)((g6 << 2) | (g6 >> 4));
        gFrame[k++] = (uint8_t)((b5 << 3) | (b5 >> 2));
    }
    fwrite(gFrame.data(), 1, gFrame.size(), stdout);
    fflush(stdout);
}

static void setKey(const std::string& name, FakeKeysState& s) {
    if (name == "up") s.up = true;
    else if (name == "down") s.down = true;
    else if (name == "left") s.left = true;
    else if (name == "right") s.right = true;
    else if (name == "enter") s.enter = true;
    else if (name == "esc") s.esc = true;
    else if (name == "backspace") s.backspace = true;
    else if (name == "del") s.del = true;
    else if (name == "tab") s.tab = true;
    else if (name == "space") s.space = true;
}

int main() {
    nexo::audioBegin();
    nexo::musicBegin();
    NexoApp app;
    BleService ble;
    WifiAccess wifi;
    Ui ui;

    ui.begin(&app, &ble, &wifi);
    ui.loop();
    UiPreview::draw(ui);
    sendFrame();

    std::string line;
    while (std::getline(std::cin, line)) {
        size_t sp = line.find(' ');
        std::string cmd = sp == std::string::npos ? line : line.substr(0, sp);
        std::string arg = sp == std::string::npos ? "" : line.substr(sp + 1);

        if (cmd == "quit") break;

        if (cmd == "press" || cmd == "char") {
            FakeKeysState s;
            if (cmd == "char") s.word = arg.substr(0, 1);
            else setKey(arg, s);
            M5Cardputer.Keyboard.state_ = s;
            M5Cardputer.Keyboard.change_ = true;
            M5Cardputer.Keyboard.pressed_ = true;
            ui.loop();
            UiPreview::draw(ui);
            sendFrame();
            // Release so a following tick does not re-process the key.
            M5Cardputer.Keyboard.change_ = false;
            M5Cardputer.Keyboard.pressed_ = false;
            M5Cardputer.Keyboard.state_ = FakeKeysState{};
        } else if (cmd == "tick" || cmd == "shot") {
            ui.loop();
            UiPreview::draw(ui);
            sendFrame();
        }
    }
    return 0;
}
