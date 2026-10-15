// WebAssembly front-end for the Nexo UI emulator.
//
// Compiles the real src/ui/ui.cpp against the fake M5Cardputer display and
// exposes a tiny C API to JavaScript (see website/assets/js/emulator.js):
//
//   nexoemu_init()          - construct app/ble/wifi/ui and render frame 0
//   nexoemu_press(name)     - press a key ("up", "enter", "esc", ...)
//   nexoemu_char(c)         - type one character
//   nexoemu_tick()          - advance (drain timers) without a key
//   nexoemu_frame()         - pointer to the RGB888 240x135 framebuffer
//   nexoemu_width()/height()/stride()
//
// Build with firmware/tools/preview/build_wasm.sh (Emscripten).
#define NEXO_UI_PREVIEW 1
#define PROGMEM

#include <M5Cardputer.h>

#include "glcdfont.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

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

static NexoApp* gApp = nullptr;
static BleService* gBle = nullptr;
static WifiAccess* gWifi = nullptr;
static Ui* gUi = nullptr;
static uint8_t* gFrame = nullptr;

static void renderFrame() {
    if (!gFrame) gFrame = (uint8_t*)malloc(FakeDisplay::W * FakeDisplay::H * 3);
    const FakeDisplay& d = M5Cardputer.Display;
    size_t k = 0;
    for (int i = 0; i < FakeDisplay::W * FakeDisplay::H; ++i) {
        uint16_t v = d.fb[i];
        int r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
        gFrame[k++] = (uint8_t)((r5 << 3) | (r5 >> 2));
        gFrame[k++] = (uint8_t)((g6 << 2) | (g6 >> 4));
        gFrame[k++] = (uint8_t)((b5 << 3) | (b5 >> 2));
    }
}

static void step() {
    gUi->loop();
    UiPreview::draw(*gUi);
    renderFrame();
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

extern "C" {

void nexoemu_init() {
    nexo::audioBegin();
    nexo::musicBegin();
    if (!gApp) {
        gApp = new NexoApp();
        gBle = new BleService();
        gWifi = new WifiAccess();
        gUi = new Ui();
        gUi->begin(gApp, gBle, gWifi);
    }
    step();
}

void nexoemu_press(const char* name) {
    if (!gUi) nexoemu_init();
    FakeKeysState s;
    setKey(name ? name : "", s);
    M5Cardputer.Keyboard.state_ = s;
    M5Cardputer.Keyboard.change_ = true;
    M5Cardputer.Keyboard.pressed_ = true;
    step();
    M5Cardputer.Keyboard.change_ = false;
    M5Cardputer.Keyboard.pressed_ = false;
    M5Cardputer.Keyboard.state_ = FakeKeysState{};
}

void nexoemu_char(const char* c) {
    if (!gUi) nexoemu_init();
    FakeKeysState s;
    s.word = (c && *c) ? std::string(1, *c) : std::string();
    M5Cardputer.Keyboard.state_ = s;
    M5Cardputer.Keyboard.change_ = true;
    M5Cardputer.Keyboard.pressed_ = true;
    step();
    M5Cardputer.Keyboard.change_ = false;
    M5Cardputer.Keyboard.pressed_ = false;
    M5Cardputer.Keyboard.state_ = FakeKeysState{};
}

void nexoemu_tick() {
    if (!gUi) nexoemu_init();
    step();
}

uint8_t* nexoemu_frame() {
    if (!gFrame) renderFrame();
    return gFrame;
}

int nexoemu_width() { return FakeDisplay::W; }
int nexoemu_height() { return FakeDisplay::H; }

}  // extern "C"
