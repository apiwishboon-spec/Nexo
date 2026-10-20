// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
// Host-side render test for the Cardputer UI animations.
//
// Compiles the real src/ui/ui.cpp against the fake M5Cardputer stubs and proves
// that animation frames repaint incrementally instead of clearing the whole
// panel. Regression guard for the whole-screen flashing report.
#define NEXO_UI_PREVIEW 1
#define PROGMEM

#include <M5Cardputer.h>

#include <cstdio>

#include "ui.h"
#include "hw/audio.h"

static uint32_t g_now = 0;
uint32_t millis() { return g_now; }

// Only needed to satisfy the link; this test never inspects the framebuffer.
void FakeDisplay::print(const char*) {}

namespace nexo {

struct UiPreview {
    using Screen = Ui::Screen;
    static void homeCharacter(Ui& u) {
        u.screen_ = Ui::Screen::Home;
        u.homePane_ = 1;
        u.homeChar_ = 0;
        u.dirty_ = true;
    }
    static int bob(Ui& u) { return u.petBob_; }
    static void memo(Ui& u) {
        u.screen_ = Ui::Screen::Memo;
        u.memoSel_ = 0;
        u.memoScroll_ = 0;
        u.dirty_ = true;
    }
};

}  // namespace nexo

using namespace nexo;

namespace {

int failures = 0;

void runFor(Ui& ui, uint32_t ms) {
    const uint32_t end = g_now + ms;
    for (; g_now <= end; ++g_now) ui.loop();
}

void expectNoFullClear(const char* label, unsigned long clears) {
    if (clears != 0) {
        std::fprintf(stderr, "FAIL: %s performed %lu full-screen clear(s)\n", label, clears);
        ++failures;
    } else {
        std::printf("ok: %s performed 0 full-screen clears\n", label);
    }
}

void expectRedraw(const char* label, unsigned long rects) {
    if (rects == 0) {
        std::fprintf(stderr, "FAIL: %s did not redraw anything\n", label);
        ++failures;
    } else {
        std::printf("ok: %s repainted %lu rect(s)\n", label, rects);
    }
}

}  // namespace

int main() {
    NexoApp app;
    BleService ble;
    WifiAccess wifi;
    Ui ui;
    ui.begin(&app, &ble, &wifi);

    // ---- Home character animation -----------------------------------------
    UiPreview::homeCharacter(ui);
    runFor(ui, 1000);  // settle: initial full draw is expected here
    unsigned long fs = fillScreenCount();
    unsigned long fr = fillRectCount();
    runFor(ui, 5000);
    unsigned long homeClears = fillScreenCount() - fs;
    unsigned long homeRects = fillRectCount() - fr;
    expectNoFullClear("home character animation", homeClears);
    expectRedraw("home character animation", homeRects);

    // The character must actually bob. It used to change pose only every 14 s,
    // which looked static. Allow 3 s for at least one pose change.
    const int startBob = UiPreview::bob(ui);
    bool moved = false;
    for (const uint32_t end = g_now + 3000; g_now <= end; ++g_now) {
        ui.loop();
        if (UiPreview::bob(ui) != startBob) moved = true;
    }
    if (!moved) {
        std::fprintf(stderr, "FAIL: home character did not move within 3 s\n");
        ++failures;
    } else {
        std::printf("ok: home character bobs within 3 s\n");
    }

    // ---- Memo recording animation -----------------------------------------
    ui.begin(&app, &ble, &wifi);
    UiPreview::memo(ui);
    memoRecordStart();
    runFor(ui, 1000);  // settle
    fs = fillScreenCount();
    fr = fillRectCount();
    runFor(ui, 3000);
    unsigned long memoClears = fillScreenCount() - fs;
    unsigned long memoRects = fillRectCount() - fr;
    memoRecordStop();
    expectNoFullClear("memo recording animation", memoClears);
    expectRedraw("memo recording animation", memoRects);

    if (failures) {
        std::fprintf(stderr, "%d render check(s) failed\n", failures);
        return 1;
    }
    std::printf("all render checks passed\n");
    return 0;
}
