// Host UI preview harness for the Nexo firmware.
//
// Compile the real src/ui/ui.cpp on the host against a fake M5Cardputer display
// (see stubs/) and render every screen to a PPM using the exact M5GFX glcd font.
#define NEXO_UI_PREVIEW 1
#define PROGMEM

#include <M5Cardputer.h>

#include "glcdfont.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ui.h"
#include "hw/audio.h"

uint32_t millis() { return 0; }

// --- text rendering ---------------------------------------------------------
void FakeDisplay::print(const char* s) {
    const int size = textSize_;
    int x = cx_;
    const int y = cy_;
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
        if (hasBg_) {
            for (int row = 0; row < 8; ++row)
                for (int sy = 0; sy < size; ++sy)
                    for (int sx = 0; sx < size; ++sx)
                        put(x + 5 * size + sx, y + row * size + sy, (uint16_t)bg_);
        }
        x += 6 * size;
    }
    cx_ = x;
}

namespace nexo {

// Friend of Ui (see NEXO_UI_PREVIEW in ui.h): pokes private state so we can
// render each screen deterministically without faking the keyboard/loop.
struct UiPreview {
    static void draw(Ui& u) { u.draw(); }
    static void battery(Ui& u, int lvl, bool chg) {
        u.batteryLevel_ = lvl;
        u.batteryCharging_ = chg;
    }
    static void splash(Ui& u) { u.screen_ = Ui::Screen::Splash; }
    static void home(Ui& u, int sel) {
        u.screen_ = Ui::Screen::Home;
        u.sel_ = sel;
    }
    static void notes(Ui& u, std::vector<Note> ns, int sel) {
        u.screen_ = Ui::Screen::Notes;
        u.noteList_ = std::move(ns);
        u.noteSel_ = sel;
        u.noteScroll_ = 0;
    }
    static void noteView(Ui& u, std::vector<Note> ns, int sel) {
        notes(u, std::move(ns), sel);
        u.screen_ = Ui::Screen::NoteView;
    }
    static void noteEdit(Ui& u, std::vector<std::string> f, int field) {
        u.screen_ = Ui::Screen::NoteEdit;
        u.fields_ = std::move(f);
        u.field_ = field;
    }
    static void homework(Ui& u, std::vector<Homework> hw, int sel) {
        u.screen_ = Ui::Screen::Homework;
        u.hwList_ = std::move(hw);
        u.hwSel_ = sel;
        u.hwScroll_ = 0;
    }
    static void hwEdit(Ui& u, std::vector<std::string> f, int field) {
        u.screen_ = Ui::Screen::HwEdit;
        u.fields_ = std::move(f);
        u.field_ = field;
    }
    static void calculator(Ui& u, const std::string& expr, const std::string& res) {
        u.screen_ = Ui::Screen::Calculator;
        u.calcExpr_ = expr;
        u.calcResult_ = res;
    }
    static void timer(Ui& u, int seconds, bool stopwatch) {
        u.screen_ = Ui::Screen::Timer;
        u.timerSeconds_ = seconds;
        u.stopwatch_ = stopwatch;
        u.timerRunning_ = false;
        u.stopwatchAccum_ = 0;
    }
    static void settings(Ui& u, int sel) {
        clearToast(u);
        u.screen_ = Ui::Screen::Settings;
        u.settingsSel_ = sel;
    }
    static void about(Ui& u) { u.screen_ = Ui::Screen::About; }
    static void music(Ui& u, int sel) {
        u.screen_ = Ui::Screen::Music;
        u.musicSel_ = sel;
        u.musicScroll_ = 0;
    }
    static void clearToast(Ui& u) {
        u.toast_.clear();
        u.toastUntil_ = 0;
    }
    static void homeChar(Ui& u, int chr) {
        clearToast(u);
        u.screen_ = Ui::Screen::Home;
        u.homePane_ = 1;
        u.homeChar_ = chr;
        u.sel_ = 0;
    }
    static void memo(Ui& u, int sel) {
        clearToast(u);
        u.screen_ = Ui::Screen::Memo;
        u.memoSel_ = sel;
        u.memoScroll_ = 0;
    }
    static void toast(Ui& u, const std::string& msg) {
        u.toast_ = msg;
        u.toastUntil_ = 100000;
    }
};

}  // namespace nexo

using namespace nexo;

static void savePPM(const char* path) {
    const FakeDisplay& d = M5Cardputer.Display;
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", FakeDisplay::W, FakeDisplay::H);
    for (int i = 0; i < FakeDisplay::W * FakeDisplay::H; ++i) {
        uint16_t v = d.fb[i];
        int r5 = (v >> 11) & 0x1F, g6 = (v >> 5) & 0x3F, b5 = v & 0x1F;
        unsigned char rgb[3] = {
            (unsigned char)((r5 << 3) | (r5 >> 2)),
            (unsigned char)((g6 << 2) | (g6 >> 4)),
            (unsigned char)((b5 << 3) | (b5 >> 2)),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    std::string outdir = argc > 1 ? argv[1] : "out";

    NexoApp app;
    BleService ble;
    WifiAccess wifi;
    Ui ui;
    ui.begin(&app, &ble, &wifi);
    UiPreview::battery(ui, 82, false);

    std::vector<Note> notes = {
        {"1", "Physics Lecture", "Read chapter 4 on thermodynamics.\nProblems 1-12 due Friday.", 0, 0, 1, false},
        {"2", "Grocery list", "milk, eggs, bread, coffee, olive oil", 0, 0, 1, false},
        {"3", "Project ideas", "Build a Cardputer synth\nWrite the sync server\nPort the UI", 0, 0, 1, false},
        {"4", "", "remember to call mom", 0, 0, 1, false},
    };
    std::vector<Homework> hw = {
        {"h1", "Math", "Chapter 5 problems", isoToDueDay("2026-10-20"), HomeworkStatus::InProgress, 0, 0, 1, false},
        {"h2", "History Essay", "WWII causes, 1500 words", isoToDueDay("2026-10-25"), HomeworkStatus::NotStarted, 0, 0, 1, false},
        {"h3", "Chemistry Lab", "Titration writeup", isoToDueDay("2026-10-12"), HomeworkStatus::Done, 0, 0, 1, false},
        {"h4", "Spanish Vocab", "Unit 3 flashcards", 0, HomeworkStatus::NotStarted, 0, 0, 1, false},
    };

    std::vector<std::pair<std::string, std::function<void()>>> jobs;

    jobs.push_back({"01_splash", [&] { UiPreview::splash(ui); UiPreview::draw(ui); }});
    jobs.push_back({"02_home", [&] { UiPreview::home(ui, 0); UiPreview::draw(ui); }});
    jobs.push_back({"03_notes", [&] { UiPreview::notes(ui, notes, 1); UiPreview::draw(ui); }});
    jobs.push_back({"04_noteview", [&] { UiPreview::noteView(ui, notes, 0); UiPreview::draw(ui); }});
    jobs.push_back({"05_noteedit", [&] { UiPreview::noteEdit(ui, {"Physics Lecture", "Read chapter 4 on thermodynamics."}, 1); UiPreview::draw(ui); }});
    jobs.push_back({"06_homework", [&] { UiPreview::homework(ui, hw, 0); UiPreview::draw(ui); }});
    jobs.push_back({"07_hwedit", [&] { UiPreview::hwEdit(ui, {"Math", "Chapter 5 problems", "2026-10-20", "In Progress"}, 2); UiPreview::draw(ui); }});
    jobs.push_back({"08_calculator", [&] { UiPreview::calculator(ui, "12+7*3-4", "33"); UiPreview::draw(ui); }});
    jobs.push_back({"09_timer", [&] { UiPreview::timer(ui, 300, false); UiPreview::draw(ui); }});
    jobs.push_back({"10_settings", [&] { UiPreview::settings(ui, 2); UiPreview::draw(ui); }});
    jobs.push_back({"11_about", [&] { UiPreview::about(ui); UiPreview::draw(ui); }});
    jobs.push_back({"12_toast", [&] { UiPreview::home(ui, 5); UiPreview::toast(ui, "Saved to device"); UiPreview::draw(ui); }});
    jobs.push_back({"13_music", [&] { musicPlay(1); UiPreview::music(ui, 1); UiPreview::draw(ui); }});
    jobs.push_back({"14_home_cat", [&] { UiPreview::homeChar(ui, 0); UiPreview::draw(ui); }});
    jobs.push_back({"15_home_kid", [&] { UiPreview::homeChar(ui, 1); UiPreview::draw(ui); }});
    jobs.push_back({"16_memo_empty", [&] { UiPreview::memo(ui, 0); UiPreview::draw(ui); }});
    jobs.push_back({"17_memo_rec", [&] { memoRecordStart(); UiPreview::memo(ui, 0); UiPreview::draw(ui); }});
    jobs.push_back({"18_memo_list", [&] {
                        memoRecordStop();
                        memoRecordStart();
                        memoRecordStop();
                        memoRecordStart();
                        memoRecordStop();
                        UiPreview::memo(ui, 1);
                        UiPreview::draw(ui);
                    }});
    jobs.push_back({"19_settings_char", [&] { UiPreview::settings(ui, 10); UiPreview::draw(ui); }});

    std::filesystem::create_directories(outdir);
    for (auto& [name, fn] : jobs) {
        fn();
        savePPM((outdir + "/" + name + ".ppm").c_str());
        printf("rendered %s\n", name.c_str());
    }
    return 0;
}
