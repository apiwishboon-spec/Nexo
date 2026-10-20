// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
// Host-side navigation test for the Cardputer home menu.
//
// Compiles the real src/ui/ui.cpp against the fake M5Cardputer stubs and
// verifies that selecting each home-menu row opens the intended screen.
// Regression guard for issue #1 (Settings/About/Memos opened the wrong page).
#define NEXO_UI_PREVIEW 1
#define PROGMEM

#include <M5Cardputer.h>

#include <cstdio>
#include <string>

#include "ui.h"
#include "hw/audio.h"

uint32_t millis() { return 0; }

// Only needed to satisfy the link; this test never renders a frame.
void FakeDisplay::print(const char*) {}

namespace nexo {

// Friend of Ui (see NEXO_UI_PREVIEW in ui.h). Exposes the private navigation
// state so the test can drive openMenu() and inspect the destination.
struct UiPreview {
    using Screen = Ui::Screen;
    static void home(Ui& u, int sel) {
        u.screen_ = Ui::Screen::Home;
        u.sel_ = sel;
    }
    static void open(Ui& u) { u.openMenu(); }
    static Ui::Screen screen(const Ui& u) { return u.screen_; }
};

}  // namespace nexo

using namespace nexo;

namespace {

int failures = 0;

void expectScreen(const char* label, int sel, UiPreview::Screen want, Ui& ui) {
    UiPreview::home(ui, sel);
    UiPreview::open(ui);
    if (UiPreview::screen(ui) != want) {
        std::fprintf(stderr, "FAIL: %s (row %d) opened the wrong screen\n", label, sel);
        ++failures;
    } else {
        std::printf("ok: %s -> row %d\n", label, sel);
    }
}

}  // namespace

int main() {
    NexoApp app;
    BleService ble;
    WifiAccess wifi;
    Ui ui;
    ui.begin(&app, &ble, &wifi);

    using S = UiPreview::Screen;
    expectScreen("Quick Notes", 0, S::Notes, ui);
    expectScreen("Homework", 1, S::Homework, ui);
    expectScreen("Calculator", 2, S::Calculator, ui);
    expectScreen("Timer", 3, S::Timer, ui);
    expectScreen("Memos", 4, S::Memo, ui);
    expectScreen("Settings", 5, S::Settings, ui);
    expectScreen("About", 6, S::About, ui);

    if (failures) {
        std::fprintf(stderr, "%d menu navigation check(s) failed\n", failures);
        return 1;
    }
    std::printf("all %d menu navigation checks passed\n", 7);
    return 0;
}
