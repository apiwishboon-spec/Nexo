#pragma once
// Host-only fake of the M5Cardputer global. Provides just enough of the
// M5GFX API used by ui.cpp, backed by an in-memory 240x135 RGB565 framebuffer.
#include <cstdint>
#include <cstdio>
#include <string>

uint32_t millis();

class FakeDisplay {
public:
    static const int W = 240;
    static const int H = 135;

    uint16_t fb[W * H] = {0};

    int width() const { return W; }
    int height() const { return H; }

    void put(int x, int y, uint16_t c) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        fb[y * W + x] = c;
    }

    void fillScreen(uint32_t c) {
        uint16_t v = (uint16_t)c;
        for (int i = 0; i < W * H; ++i) fb[i] = v;
    }

    void fillRect(int x, int y, int w, int h, uint32_t c) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) put(x + i, y + j, (uint16_t)c);
    }

    void drawRect(int x, int y, int w, int h, uint32_t c) {
        for (int i = 0; i < w; ++i) { put(x + i, y, (uint16_t)c); put(x + i, y + h - 1, (uint16_t)c); }
        for (int j = 0; j < h; ++j) { put(x, y + j, (uint16_t)c); put(x + w - 1, y + j, (uint16_t)c); }
    }

    void fillCircle(int cx, int cy, int r, uint32_t c) {
        for (int y = -r; y <= r; ++y)
            for (int x = -r; x <= r; ++x)
                if (x * x + y * y <= r * r) put(cx + x, cy + y, (uint16_t)c);
    }

private:
    static void plotCircle(int cx, int cy, int r, uint32_t c, int sx, int sy, FakeDisplay& d) {
        int x = 0, y = r, e = 3 - 2 * r;
        auto P = [&](int px, int py) {
            int dx = px - cx, dy = py - cy;
            if (dx * sx >= 0 && dy * sy >= 0) d.put(px, py, (uint16_t)c);
        };
        while (x <= y) {
            P(cx + x, cy + y); P(cx - x, cy + y); P(cx + x, cy - y); P(cx - x, cy - y);
            P(cx + y, cy + x); P(cx - y, cy + x); P(cx + y, cy - x); P(cx - y, cy - x);
            if (e < 0) e += 4 * x + 6; else { e += 4 * (x - y) + 10; --y; }
            ++x;
        }
    }

public:
    void fillRoundRect(int x, int y, int w, int h, int r, uint32_t c) {
        if (r <= 0) { fillRect(x, y, w, h, c); return; }
        if (r > w / 2) r = w / 2;
        if (r > h / 2) r = h / 2;
        fillRect(x + r, y, w - 2 * r, h, c);
        fillRect(x, y + r, r, h - 2 * r, c);
        fillRect(x + w - r, y + r, r, h - 2 * r, c);
        fillCircle(x + r, y + r, r, c);
        fillCircle(x + w - r - 1, y + r, r, c);
        fillCircle(x + r, y + h - r - 1, r, c);
        fillCircle(x + w - r - 1, y + h - r - 1, r, c);
    }

    void drawRoundRect(int x, int y, int w, int h, int r, uint32_t c) {
        if (r <= 0) { drawRect(x, y, w, h, c); return; }
        if (r > w / 2) r = w / 2;
        if (r > h / 2) r = h / 2;
        for (int i = x + r; i < x + w - r; ++i) { put(i, y, (uint16_t)c); put(i, y + h - 1, (uint16_t)c); }
        for (int j = y + r; j < y + h - r; ++j) { put(x, j, (uint16_t)c); put(x + w - 1, j, (uint16_t)c); }
        plotCircle(x + r, y + r, r, c, -1, -1, *this);
        plotCircle(x + w - r - 1, y + r, r, c, 1, -1, *this);
        plotCircle(x + r, y + h - r - 1, r, c, -1, 1, *this);
        plotCircle(x + w - r - 1, y + h - r - 1, r, c, 1, 1, *this);
    }

    void setTextSize(uint8_t s) { textSize_ = s ? s : 1; }
    void setTextColor(uint32_t c) { fg_ = (uint16_t)c; hasBg_ = false; }
    void setTextColor(uint32_t c, uint32_t bg) { fg_ = (uint16_t)c; bg_ = (uint16_t)bg; hasBg_ = true; }
    void setCursor(int x, int y) { cx_ = x; cy_ = y; }
    void print(const char* s);

    bool getSwapBytes() const { return swap_; }
    void setSwapBytes(bool s) { swap_ = s; }

    void setBrightness(uint8_t b) {
        brightness_ = b;
        std::fprintf(stderr, "[brightness %u]\n", (unsigned)b);
    }
    void sleep() {
        sleeping_ = true;
        std::fprintf(stderr, "[display off]\n");
    }
    void wakeup() {
        sleeping_ = false;
        std::fprintf(stderr, "[display on]\n");
    }
    bool sleeping() const { return sleeping_; }

    void pushImage(int x, int y, int w, int h, const uint16_t* data) {
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) put(x + i, y + j, data[j * w + i]);
    }

private:
    int cx_ = 0, cy_ = 0;
    uint8_t textSize_ = 1;
    uint16_t fg_ = 0xFFFF, bg_ = 0;
    bool hasBg_ = false, swap_ = false;
    uint8_t brightness_ = 128;
    bool sleeping_ = false;
};

struct FakePower {
    int level = 82;
    int charging = 0;
    int getBatteryLevel() const { return level; }
    int isCharging() const { return charging; }
};

struct FakeKeysState {
    bool up = false, down = false, left = false, right = false;
    bool enter = false, esc = false, backspace = false, del = false, tab = false, space = false;
    std::string word;
};

class FakeKeyboard {
public:
    bool isChange() const { return change_; }
    bool isPressed() const { return pressed_; }
    FakeKeysState keysState() const { return state_; }
    bool change_ = false, pressed_ = false;
    FakeKeysState state_;
};

struct FakeSpeaker {
    uint8_t volume = 200;
    void setVolume(uint8_t v) { volume = v; }
    bool tone(float freq, uint32_t ms) {
        std::fprintf(stderr, "[beep %.0f %u]\n", freq, ms);
        return true;
    }
    void stop() {}
    bool isPlaying() const { return false; }
};

struct M5CardputerClass {
    FakeDisplay Display;
    FakePower Power;
    FakeKeyboard Keyboard;
    FakeSpeaker Speaker;
    void update() {}
};

inline M5CardputerClass M5Cardputer;
