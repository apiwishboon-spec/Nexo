// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#pragma once
// Compact Material-inspired Cardputer UI. Keyboard driven, non-blocking.
#include <string>
#include <vector>

#include "app/nexo_app.h"
#include "ble/ble_service.h"
#include "wifi/wifi_access.h"

namespace nexo {

class Ui {
public:
    void begin(NexoApp* app, BleService* ble, WifiAccess* wifi);
    void loop();
    void notify(const std::string& msg);  // toast on next frame

private:
#ifdef NEXO_UI_PREVIEW
    friend struct UiPreview;  // host preview harness (tools/preview)
#endif
    enum class Screen {
        Splash,
        Home,
        Notes,
        NoteView,
        NoteEdit,
        Homework,
        HwEdit,
        Calculator,
        Timer,
        Settings,
        About,
        Music,
        Memo,       // voice memos (record/play/delete)
        Clock,      // idle flip-clock screensaver
        Lab,        // hidden tools menu
        WifiScan,   // lab: AP scanner
        SpamWifi,   // lab: beacon spam
        SpamBle,    // lab: BLE name spam
    };

    // Rendering
    void draw();
    void drawHeader(const std::string& title, const std::string& trailing = "");
    int drawBattery(int rightX);
    void readBattery();
    void drawSplash();
    void drawFirmwareLogo(int x, int y);
    void drawListRow(int y, int h, const std::string& title, const std::string& sub,
                     bool selected);
    void drawEmptyState(const std::string& msg);
    void drawField(int y, const std::string& label, const std::string& value, bool active,
                   int boxH);
    void drawMenu();
    void drawNotes();
    void drawNoteView();
    void drawNoteEdit();
    void drawHomework();
    void drawHwEdit();
    void drawCalculator();
    void drawTimer();
    void drawSettings();
    void drawAbout();
    void drawMusic();
    void drawMemo();
    void drawClock();
    void drawLab();
    void drawWifiScan();
    void drawSpam(bool ble);
    void drawCharacter(int x, int y, int w, int h);
    void drawCheckbox(int x, int y, bool done);
    void toast();
    void wrappedText(const std::string& text, int x, int y, int w, int maxLines);

    // Input
    void handleKeys();
    void noteActivity();
    void updatePower();
    void gotoHome();
    void openMenu();
    void openNotes();
    void openHomework();
    void openMemo();
    void openLab();
    void computeDueToday();
    void toggleHwDone();
    void loadPrefs();
    void savePrefs();
    bool secretReveal();  // types "hack" within 5s on Home to unlock the Lab

    // Editing helpers
    void startNewNote();
    void startEditNote(const Note& n);
    void commitNoteEdit();
    void startNewHomework();
    void startEditHomework(const Homework& h);
    void commitHwEdit();

    NexoApp* app_ = nullptr;
    BleService* ble_ = nullptr;
    WifiAccess* wifi_ = nullptr;

    Screen screen_ = Screen::Home;
    bool dirty_ = true;
    uint32_t splashUntil_ = 0;
    std::string toast_;
    uint32_t toastUntil_ = 0;

    // Menu state
    std::vector<std::string> menuItems_;
    int sel_ = 0;

    // Notes
    std::vector<Note> noteList_;
    int noteSel_ = 0;
    int noteScroll_ = 0;
    Note editNote_;
    std::vector<std::string> fields_;
    int field_ = 0;

    // Homework
    std::vector<Homework> hwList_;
    int hwSel_ = 0;
    int hwScroll_ = 0;

    // Calculator
    std::string calcExpr_;
    std::string calcResult_;

    // Timer
    bool timerRunning_ = false;
    bool stopwatch_ = false;
    bool timerDone_ = false;
    uint32_t timerStart_ = 0;
    uint32_t stopwatchAccum_ = 0;
    int timerSeconds_ = 60;
    int timerEditField_ = 0;
    uint32_t alarmUntil_ = 0;
    uint32_t alarmNextBeep_ = 0;

    // Settings
    int settingsSel_ = 0;
    int settingsScroll_ = 0;
    bool editingText_ = false;
    std::string editBuffer_;

    // Music
    int musicSel_ = 0;
    int musicScroll_ = 0;

    // Voice memos
    int memoSel_ = 0;
    int memoScroll_ = 0;
    uint32_t memoRedrawAt_ = 0;

    // Battery (Cardputer reports this over the ADC on GPIO10).
    int batteryLevel_ = -1;
    bool batteryCharging_ = false;
    uint32_t batteryReadAt_ = 0;

    // Auto-sleep: dim then turn the panel off after inactivity to save power.
    uint32_t lastActivity_ = 0;
    bool dimmed_ = false;
    bool asleep_ = false;

    // Home layout: left pane shows either today's tasks or the character.
    int homePane_ = 0;   // 0 = due today, 1 = character
    int homeChar_ = 0;   // 0 = cat, 1 = glasses kid
    std::vector<Homework> dueToday_;
    Screen prevScreen_ = Screen::Home;  // where to return after the screensaver
    uint32_t petDrawAt_ = 0;
    uint32_t petFrame_ = 0xFFFFFFFFu;
    uint32_t clockMinute_ = 0xFFFFFFFFu;

    // Screensaver + hidden Lab menu.
    bool screensaver_ = false;
    bool labUnlocked_ = false;
    int labSel_ = 0;
    int wifiScanSel_ = 0;
    int wifiScanScroll_ = 0;
    std::string hackBuf_;
    uint32_t hackBufAt_ = 0;
};

}  // namespace nexo
