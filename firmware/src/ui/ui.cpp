// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 Apiwish Anutravanichkul
#include "ui.h"

#include <M5Cardputer.h>

#include <cstdio>
#include <cstring>

#include "nexo_calc.h"
#include "hw/audio.h"
#include "ui/theme.h"
#include "assets/firmware_logo.h"

using namespace nexo::theme;

namespace nexo {

static const int kW = 240;
static const int kH = 135;
static const int kHeaderH = 18;
static const int kContentTop = kHeaderH + 3;  // 21
static const int kContentBottom = kH - 4;
static const int kRowH = 23;

// Auto-sleep tuning: dim after 15s idle, panel off after 45s. Any key wakes.
static const uint32_t kDimIdleMs = 15000;
static const uint32_t kSleepIdleMs = 45000;
static const uint8_t kActiveBrightness = 128;
static const uint8_t kDimBrightness = 45;

static int statusIndex(const std::string& s)
{
    if (s == "In Progress") return 1;
    if (s == "Done") return 2;
    return 0;
}

static const char* statusName(int i)
{
    static const char* names[3] = {"Not Started", "In Progress", "Done"};
    if (i < 0 || i > 2) i = 0;
    return names[i];
}

void Ui::begin(NexoApp* app, BleService* ble, WifiAccess* wifi)
{
    app_ = app;
    ble_ = ble;
    wifi_ = wifi;
    menuItems_ = {"Quick Notes", "Homework", "Calculator", "Timer",
                  "Settings",   "About",   "Music"};
    screen_ = Screen::Splash;
    splashUntil_ = millis() + 1600;
    sel_ = 0;
    lastActivity_ = millis();
    dirty_ = true;
}

void Ui::notify(const std::string& msg)
{
    toast_ = msg;
    toastUntil_ = millis() + 2000;
    dirty_ = true;
}

// ---- drawing helpers -------------------------------------------------------
void Ui::drawFirmwareLogo(int x, int y)
{
    auto& d = M5Cardputer.Display;
    bool prev = d.getSwapBytes();
    d.setSwapBytes(true);
    d.pushImage(x, y, kFirmwareLogoW, kFirmwareLogoH, kFirmwareLogo);
    d.setSwapBytes(prev);
}

void Ui::drawSplash()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(kBackground);
    int x = (kW - kFirmwareLogoW) / 2;
    drawFirmwareLogo(x, 26);
    d.setTextSize(1);
    d.setTextColor(kOnSurfaceVariant, kBackground);
    d.setCursor(x, 28 + kFirmwareLogoH + 4);
    d.print("Cardputer companion");
}

void Ui::drawHeader(const std::string& title, const std::string& trailing)
{
    auto& d = M5Cardputer.Display;
    d.fillRect(0, 0, kW, kHeaderH, kSurface);
    d.fillRect(0, kHeaderH - 1, kW, 1, kOutline);

    // Size-2 title gives the UI a real heading instead of a timid label.
    d.setTextSize(2);
    d.setTextColor(kPrimary, kSurface);
    d.setCursor(kPad, 1);
    d.print(title.c_str());
    d.setTextSize(1);

    // Right cluster: battery, then connection dots, then an optional count.
    int x = drawBattery(kW - kPad);
    if (wifi_ && wifi_->enabled()) {
        d.fillCircle(x - 4, 9, 3, kPrimary);
        x -= 11;
    }
    if (ble_ && ble_->connected()) {
        d.fillCircle(x - 4, 9, 3, kSuccess);
        x -= 11;
    }

    if (!trailing.empty()) {
        d.setTextColor(kOnSurfaceVariant, kSurface);
        int tw = (int)trailing.length() * 6;
        d.setCursor(x - 4 - tw, 5);
        d.print(trailing.c_str());
    }
}

// Draws a battery glyph ending at rightX; returns the x just left of whatever
// was drawn (icon, or the percentage label). Never overlaps the title.
int Ui::drawBattery(int rightX)
{
    auto& d = M5Cardputer.Display;
    const int w = 16, h = 9, by = 6;
    int bx = rightX - w;
    d.drawRect(bx, by, w, h, kOnSurfaceVariant);
    d.fillRect(bx + w, by + 2, 2, h - 4, kOnSurfaceVariant);
    if (batteryLevel_ < 0) return bx;

    int fill = (w - 4) * batteryLevel_ / 100;
    uint16_t col = batteryCharging_ ? kPrimary : (batteryLevel_ <= 15 ? kError : kSuccess);
    if (fill > 0) d.fillRect(bx + 2, by + 2, fill, h - 4, col);

    char b[8];
    snprintf(b, sizeof(b), "%d%%", batteryLevel_);
    int tw = (int)strlen(b) * 6;
    d.setTextColor(kOnSurfaceVariant, kSurface);
    d.setCursor(bx - 3 - tw, 7);
    d.print(b);
    return bx - 3 - tw;
}

void Ui::readBattery()
{
    if (batteryReadAt_ != 0 && millis() - batteryReadAt_ < 5000u) return;
    batteryReadAt_ = millis();
    int lvl = (int)M5Cardputer.Power.getBatteryLevel();
    bool chg = M5Cardputer.Power.isCharging() != 0;
    if (lvl != batteryLevel_ || chg != batteryCharging_) {
        batteryLevel_ = lvl;
        batteryCharging_ = chg;
        dirty_ = true;
    }
}

void Ui::wrappedText(const std::string& text, int x, int y, int w, int maxLines)
{
    auto& d = M5Cardputer.Display;
    int charsPerLine = w / 6;  // ~6px per char at size 1
    if (charsPerLine < 1) charsPerLine = 1;
    int line = 0;
    size_t i = 0;
    while (i < text.size() && line < maxLines) {
        int n = 0;
        while (i < text.size() && n < charsPerLine && text[i] != '\n') {
            ++i;
            ++n;
        }
        d.setCursor(x, y + line * 10);
        d.print(text.substr(i - n, n).c_str());
        ++line;
        if (i < text.size() && text[i] == '\n') ++i;
    }
}

// A two-line list row: title on top, dim subtitle below. Selection adds a
// primary accent bar and a lighter fill so the cursor is always obvious.
void Ui::drawListRow(int y, int h, const std::string& title, const std::string& sub,
                     bool selected)
{
    auto& d = M5Cardputer.Display;
    uint16_t bg = selected ? kSurfaceVariant : kSurface;
    d.fillRoundRect(kPad, y, kW - 2 * kPad, h, kRadius, bg);
    if (selected) d.fillRoundRect(kPad, y, 4, h, 2, kPrimary);
    d.setTextColor(selected ? kPrimary : kOnSurface, bg);
    d.setCursor(kPad + 12, y + 4);
    d.print(title.c_str());
    if (!sub.empty()) {
        d.setTextColor(kOnSurfaceVariant, bg);
        d.setCursor(kPad + 12, y + 13);
        d.print(sub.c_str());
    }
}

void Ui::drawEmptyState(const std::string& msg)
{
    auto& d = M5Cardputer.Display;
    d.setTextColor(kOnSurfaceVariant, kBackground);
    int tw = (int)msg.length() * 6;
    d.setCursor((kW - tw) / 2, 62);
    d.print(msg.c_str());
}

// A labelled input box; the active field is outlined and shows a cursor.
void Ui::drawField(int y, const std::string& label, const std::string& value, bool active,
                   int boxH)
{
    auto& d = M5Cardputer.Display;
    d.setTextColor(active ? kPrimary : kOnSurfaceVariant, kBackground);
    d.setCursor(kPad, y);
    d.print(label.c_str());

    uint16_t bg = active ? kSurfaceVariant : kSurface;
    d.fillRoundRect(kPad, y + 9, kW - 2 * kPad, boxH, kRadius, bg);
    if (active) d.drawRoundRect(kPad, y + 9, kW - 2 * kPad, boxH, kRadius, kPrimary);
    d.setTextColor(kOnSurface, bg);
    d.setCursor(kPad + 5, y + 12);

    int maxChars = (kW - 2 * kPad - 12) / 6;
    std::string v = value;
    if ((int)v.size() > maxChars) v = v.substr(v.size() - maxChars);
    d.print((v + (active ? "_" : "")).c_str());
}

void Ui::drawMenu()
{
    auto& d = M5Cardputer.Display;
    drawHeader("Nexo");
    const int gap = 5;
    const int top = kContentTop;
    const int bottom = kContentBottom;
    int count = (int)menuItems_.size();
    const int colW = (kW - 2 * kPad - gap) / 2;
    int rows = (count + 1) / 2;
    if (rows < 1) rows = 1;
    const int rowH = (bottom - top - (rows - 1) * gap) / rows;
    for (int i = 0; i < (int)menuItems_.size(); ++i) {
        int col = i % 2, row = i / 2;
        int x = kPad + col * (colW + gap);
        int y = top + row * (rowH + gap);
        bool selected = i == sel_;
        uint16_t bg = selected ? kPrimary : kSurface;
        uint16_t fg = selected ? kOnPrimary : kOnSurface;
        d.fillRoundRect(x, y, colW, rowH, kRadius, bg);
        if (!selected) d.drawRoundRect(x, y, colW, rowH, kRadius, kOutline);

        // Number in a small chip, label beside it on one line and vertically
        // centred - avoids the number/label overlap tiny tiles used to have.
        int cy = y + (rowH - 8) / 2;
        d.fillRoundRect(x + 5, cy - 1, 13, 10, 3,
                        selected ? kOnPrimary : kSurfaceVariant);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        d.setTextColor(selected ? kPrimary : kOnSurfaceVariant,
                       selected ? kOnPrimary : kSurfaceVariant);
        d.setCursor(x + 8, cy);
        d.print(num);
        d.setTextColor(fg, bg);
        d.setCursor(x + 22, cy);
        d.print(menuItems_[i].c_str());
    }
}

void Ui::drawNotes()
{
    drawHeader("Notes", std::to_string(noteList_.size()));
    if (noteList_.empty()) {
        drawEmptyState("No notes - press N");
        return;
    }
    const int visible = 4;
    if (noteSel_ < noteScroll_) noteScroll_ = noteSel_;
    if (noteSel_ >= noteScroll_ + visible) noteScroll_ = noteSel_ - visible + 1;
    int y = kContentTop;
    for (int i = noteScroll_; i < (int)noteList_.size() && i < noteScroll_ + visible; ++i) {
        std::string title = noteList_[i].title.empty() ? "(untitled)" : noteList_[i].title;
        if (title.size() > 32) title = title.substr(0, 31) + "~";
        std::string sub = noteList_[i].body;
        size_t nl = sub.find('\n');
        if (nl != std::string::npos) sub = sub.substr(0, nl);
        if (sub.size() > 32) sub = sub.substr(0, 31) + "~";
        drawListRow(y, kRowH, title, sub, i == noteSel_);
        y += kRowH + 1;
    }
}

void Ui::drawNoteView()
{
    auto& d = M5Cardputer.Display;
    const Note& n = noteList_[noteSel_];
    drawHeader("Note");
    d.setTextColor(kPrimary, kBackground);
    d.setTextSize(2);
    d.setCursor(kPad, kContentTop);
    std::string title = n.title.empty() ? "(untitled)" : n.title;
    if ((int)title.size() > kPad + 16) title = title.substr(0, 16) + "~";
    d.print(title.c_str());
    d.setTextSize(1);
    d.setTextColor(kOnSurface, kBackground);
    wrappedText(n.body, kPad, kContentTop + 18, kW - 2 * kPad, 7);
}

void Ui::drawNoteEdit()
{
    drawHeader("Edit Note");
    drawField(kContentTop, "Title", fields_[0], field_ == 0, 20);
    drawField(kContentTop + 35, "Body", fields_[1], field_ == 1, 40);
}

void Ui::drawHomework()
{
    drawHeader("Homework", std::to_string(hwList_.size()));
    if (hwList_.empty()) {
        drawEmptyState("No homework - press N");
        return;
    }
    const int visible = 4;
    if (hwSel_ < hwScroll_) hwScroll_ = hwSel_;
    if (hwSel_ >= hwScroll_ + visible) hwScroll_ = hwSel_ - visible + 1;
    const char* st[3] = {"TODO", "DOING", "DONE"};
    int y = kContentTop;
    for (int i = hwScroll_; i < (int)hwList_.size() && i < hwScroll_ + visible; ++i) {
        std::string subject = hwList_[i].subject.empty() ? "(no subject)" : hwList_[i].subject;
        if (subject.size() > 32) subject = subject.substr(0, 31) + "~";
        std::string due = hwList_[i].dueEpochDay ? dueDayToIso(hwList_[i].dueEpochDay) : "-";
        std::string sub = std::string(st[(int)hwList_[i].status]) + "   due " + due;
        drawListRow(y, kRowH, subject, sub, i == hwSel_);
        y += kRowH + 1;
    }
}

void Ui::drawHwEdit()
{
    drawHeader("Homework");
    drawField(20, "Subject", fields_[0], field_ == 0, 15);
    drawField(45, "Description", fields_[1], field_ == 1, 15);
    drawField(70, "Due (YYYY-MM-DD)", fields_[2], field_ == 2, 15);
    drawField(95, "Status", fields_[3], field_ == 3, 15);
}

void Ui::drawCalculator()
{
    auto& d = M5Cardputer.Display;
    drawHeader("Calculator");
    int cardY = kContentTop + 2;
    d.fillRoundRect(kPad, cardY, kW - 2 * kPad, 44, kRadius, kSurface);
    d.setTextColor(kOnSurface, kSurface);
    d.setCursor(kPad + 5, cardY + 6);
    std::string e = calcExpr_.empty() ? "0" : calcExpr_;
    if (e.size() > 34) e = e.substr(e.size() - 34);
    d.print(e.c_str());
    d.setTextColor(kPrimary, kSurface);
    d.setTextSize(2);
    d.setCursor(kPad + 5, cardY + 22);
    std::string r = calcResult_.empty() ? "= 0" : ("= " + calcResult_);
    d.print(r.c_str());
    d.setTextSize(1);
}

void Ui::drawTimer()
{
    auto& d = M5Cardputer.Display;
    drawHeader(stopwatch_ ? "Stopwatch" : "Timer");
    uint32_t elapsedMs = 0;
    if (stopwatch_) {
        elapsedMs = stopwatchAccum_ + (timerRunning_ ? millis() - timerStart_ : 0);
    } else {
        elapsedMs = (uint32_t)timerSeconds_ * 1000;
    }
    char buf[32];
    if (stopwatch_) {
        uint32_t cs = (elapsedMs / 10) % 100;
        uint32_t s = (elapsedMs / 1000) % 60;
        uint32_t m = elapsedMs / 60000;
        snprintf(buf, sizeof(buf), "%02u:%02u.%02u", m, s, cs);
    } else {
        uint32_t remaining = timerSeconds_;
        if (timerDone_) {
            remaining = 0;
        } else if (timerRunning_) {
            uint32_t passed = (millis() - timerStart_) / 1000;
            remaining = passed >= (uint32_t)timerSeconds_ ? 0 : timerSeconds_ - passed;
        }
        snprintf(buf, sizeof(buf), "%02u:%02u", remaining / 60, remaining % 60);
    }
    d.setTextColor(kPrimary, kBackground);
    d.setTextSize(3);
    int tw = strlen(buf) * 18;
    d.setCursor((kW - tw) / 2, 50);
    d.print(buf);
    d.setTextSize(1);
}

void Ui::drawSettings()
{
    auto& d = M5Cardputer.Display;
    Settings& s = app_->settings();
    drawHeader("Settings");
    std::vector<std::string> rows = {
        "Device name: " + s.deviceName,
        "PIN: " + std::string(s.pin.size(), '*'),
        std::string("Sound: ") + (audioEnabled() ? "ON" : "OFF"),
        "Volume: " + std::to_string(audioVolume()),
        std::string("Wi-Fi AP: ") + (s.apEnabled ? "ON" : "OFF"),
        "AP SSID: " + s.apSsid,
        "Wi-Fi IP: " + std::string(wifi_ ? wifi_->ipString().c_str() : "-"),
        "AP pass: " + std::string(wifi_ ? wifi_->apPassword().c_str() : "-"),
        "Save & persist",
    };
    const int visible = 7;
    int count = (int)rows.size();
    if (settingsSel_ < settingsScroll_) settingsScroll_ = settingsSel_;
    if (settingsSel_ >= settingsScroll_ + visible)
        settingsScroll_ = settingsSel_ - visible + 1;
    for (int i = settingsScroll_; i < count && i < settingsScroll_ + visible; ++i) {
        int y = kContentTop + (i - settingsScroll_) * 14;
        bool selected = i == settingsSel_;
        uint16_t bg = selected ? kSurfaceVariant : kSurface;
        d.fillRoundRect(kPad, y, kW - 2 * kPad, 13, kRadius, bg);
        if (selected) d.fillRect(kPad, y, 3, 13, kPrimary);
        d.setTextColor(selected ? kPrimary : kOnSurface, bg);
        d.setCursor(kPad + 7, y + 3);
        std::string r = rows[i];
        if (r.size() > 35) r = r.substr(0, 34) + "~";
        d.print(r.c_str());
    }
    if (settingsScroll_ > 0) {
        d.setTextColor(kOnSurfaceVariant, kBackground);
        d.setCursor(kW - 10, kContentTop);
        d.print("^");
    }
    if (settingsScroll_ + visible < count) {
        d.setTextColor(kOnSurfaceVariant, kBackground);
        d.setCursor(kW - 10, kH - 17);
        d.print("v");
    }
    if (editingText_) {
        int ey = kH - 26;
        d.fillRoundRect(kPad, ey, kW - 2 * kPad, 16, kRadius, kSurfaceVariant);
        d.drawRoundRect(kPad, ey, kW - 2 * kPad, 16, kRadius, kPrimary);
        d.setTextColor(kOnSurface, kSurfaceVariant);
        d.setCursor(kPad + 4, ey + 4);
        d.print((editBuffer_ + "_").c_str());
    }
}

void Ui::drawAbout()
{
    auto& d = M5Cardputer.Display;
    drawHeader("About");
    int x = (kW - kFirmwareLogoW) / 2;
    drawFirmwareLogo(x, kContentTop + 2);

    d.setTextSize(1);
    const char* ver = "Nexo 1.0.0  -  Protocol v1";
    d.setTextColor(kPrimary, kBackground);
    d.setCursor((kW - (int)strlen(ver) * 6) / 2, kContentTop + kFirmwareLogoH + 6);
    d.print(ver);

    d.setTextColor(kOnSurfaceVariant, kBackground);
    wrappedText(
        "Offline-first notes, homework, calculator, timer and music for the "
        "M5Stack Cardputer. Sync over BLE.",
        kPad, kContentTop + kFirmwareLogoH + 18, kW - 2 * kPad, 2);
}

void Ui::drawMusic()
{
    auto& d = M5Cardputer.Display;
    int count = musicCount();
    drawHeader("Music", count > 0 ? std::to_string(count) : std::string());
    if (count == 0) {
        drawEmptyState("No audio on SD");
        return;
    }
    const int visible = 4;
    if (musicSel_ < musicScroll_) musicScroll_ = musicSel_;
    if (musicSel_ >= musicScroll_ + visible) musicScroll_ = musicSel_ - visible + 1;
    int y = kContentTop;
    for (int i = musicScroll_; i < count && i < musicScroll_ + visible; ++i) {
        std::string sub = (i == musicIndex()) ? (musicPlaying() ? "Playing" : "Paused")
                                              : ("Track " + std::to_string(i + 1));
        drawListRow(y, kRowH, musicTitle(i), sub, i == musicSel_);
        y += kRowH;
    }
}

void Ui::toast()
{
    if (toast_.empty() || millis() > toastUntil_) {
        toast_.clear();
        return;
    }
    auto& d = M5Cardputer.Display;
    int ty = kH - 26;
    d.fillRoundRect(kPad, ty, kW - 2 * kPad, 16, kRadius, kPrimary);
    d.setTextColor(kOnPrimary, kPrimary);
    d.setCursor(kPad + 5, ty + 4);
    d.print(toast_.c_str());
}

void Ui::draw()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(kBackground);
    d.setTextSize(1);
    switch (screen_) {
        case Screen::Splash: drawSplash(); break;
        case Screen::Home: drawMenu(); break;
        case Screen::Notes: drawNotes(); break;
        case Screen::NoteView: drawNoteView(); break;
        case Screen::NoteEdit: drawNoteEdit(); break;
        case Screen::Homework: drawHomework(); break;
        case Screen::HwEdit: drawHwEdit(); break;
        case Screen::Calculator: drawCalculator(); break;
        case Screen::Timer: drawTimer(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::About: drawAbout(); break;
        case Screen::Music: drawMusic(); break;
    }
    toast();
    dirty_ = false;
}

// ---- navigation / actions --------------------------------------------------
void Ui::gotoHome()
{
    screen_ = Screen::Home;
    sel_ = 0;
}

void Ui::openMenu()
{
    switch (sel_) {
        case 0: openNotes(); break;
        case 1: openHomework(); break;
        case 2: calcExpr_.clear(); calcResult_.clear(); screen_ = Screen::Calculator; break;
        case 3: screen_ = Screen::Timer; break;
        case 4: settingsSel_ = 0; screen_ = Screen::Settings; break;
        case 5: screen_ = Screen::About; break;
        case 6:
            musicSel_ = musicIndex() >= 0 ? musicIndex() : 0;
            musicScroll_ = 0;
            screen_ = Screen::Music;
            break;
    }
}

void Ui::openNotes()
{
    noteList_.clear();
    for (const auto& n : app_->repo().notes())
        if (!n.deleted) noteList_.push_back(n);
    noteSel_ = 0;
    noteScroll_ = 0;
    screen_ = Screen::Notes;
}

void Ui::openHomework()
{
    hwList_.clear();
    for (const auto& h : app_->repo().homework())
        if (!h.deleted) hwList_.push_back(h);
    hwSel_ = 0;
    hwScroll_ = 0;
    screen_ = Screen::Homework;
}

void Ui::startNewNote()
{
    editNote_ = Note{};
    fields_ = {"", ""};
    field_ = 0;
    screen_ = Screen::NoteEdit;
}

void Ui::startEditNote(const Note& n)
{
    editNote_ = n;
    fields_ = {n.title, n.body};
    field_ = 0;
    screen_ = Screen::NoteEdit;
}

void Ui::commitNoteEdit()
{
    Note in;
    in.id = editNote_.id;
    in.title = fields_[0];
    in.body = fields_[1];
    Note stored;
    uint32_t base = editNote_.id.empty() ? 0 : editNote_.rev;
    RepoStatus st = app_->repo().putNote(in, base, stored);
    if (st == RepoStatus::Ok) {
        notify("Saved");
    } else if (st == RepoStatus::Conflict) {
        notify("Conflict: reloaded");
    } else {
        notify("Save failed");
    }
    if (ble_) ble_->notifyDataChanged();
    openNotes();
}

void Ui::startNewHomework()
{
    editNote_ = Note{};  // reuse id field not needed
    fields_ = {"", "", "", "Not Started"};
    field_ = 0;
    screen_ = Screen::HwEdit;
}

void Ui::startEditHomework(const Homework& h)
{
    fields_ = {h.subject, h.description, dueDayToIso(h.dueEpochDay),
               statusName(static_cast<int>(h.status))};
    field_ = 0;
    editNote_.id = h.id;
    editNote_.rev = h.rev;
    screen_ = Screen::HwEdit;
}

void Ui::commitHwEdit()
{
    Homework h;
    h.id = editNote_.id;
    h.subject = fields_[0];
    h.description = fields_[1];
    h.dueEpochDay = isoToDueDay(fields_[2]);
    int st = 0;
    if (fields_[3] == "In Progress") st = 1;
    else if (fields_[3] == "Done") st = 2;
    h.status = static_cast<HomeworkStatus>(st);
    Homework stored;
    uint32_t base = h.id.empty() ? 0 : editNote_.rev;
    RepoStatus res = app_->repo().putHomework(h, base, stored);
    notify(res == RepoStatus::Ok ? "Saved" : "Conflict/failed");
    if (ble_) ble_->notifyDataChanged();
    openHomework();
}

// ---- key handling ----------------------------------------------------------
void Ui::noteActivity()
{
    lastActivity_ = millis();
    if (asleep_) {
        asleep_ = false;
        dimmed_ = false;
        M5Cardputer.Display.wakeup();
        M5Cardputer.Display.setBrightness(kActiveBrightness);
        dirty_ = true;
    } else if (dimmed_) {
        dimmed_ = false;
        M5Cardputer.Display.setBrightness(kActiveBrightness);
    }
}

void Ui::updatePower()
{
    if (asleep_) return;
    uint32_t idle = millis() - lastActivity_;
    if (idle >= kSleepIdleMs) {
        M5Cardputer.Display.setBrightness(0);
        M5Cardputer.Display.sleep();
        asleep_ = true;
        dimmed_ = false;
    } else if (idle >= kDimIdleMs && !dimmed_) {
        M5Cardputer.Display.setBrightness(kDimBrightness);
        dimmed_ = true;
    }
}

void Ui::handleKeys()
{
    auto& kb = M5Cardputer.Keyboard;
    if (!kb.isChange() || !kb.isPressed()) return;
    // While the panel is off, the first key only wakes it (does not act).
    if (asleep_) {
        noteActivity();
        return;
    }
    noteActivity();
    auto ks = kb.keysState();

    auto typed = [&](char c) {
        for (char w : ks.word)
            if (w == c) return true;
        return false;
    };
    // Fn-free navigation: plain keys mirror the Fn+arrow positions, plus wasd.
    const bool navUp = ks.up || typed('w') || typed(';');
    const bool navDown = ks.down || typed('s') || typed('.');
    const bool navLeft = ks.left || typed('a') || typed(',');
    const bool navRight = ks.right || typed('d') || typed('/');

    // Settings text-entry mode intercepts typing.
    if (screen_ == Screen::Settings && editingText_) {
        for (char c : ks.word) editBuffer_.push_back(c);
        if (ks.backspace || ks.del) {
            if (!editBuffer_.empty()) editBuffer_.pop_back();
        }
        if (ks.enter) {
            Settings& s = app_->settings();
            if (settingsSel_ == 0) s.deviceName = editBuffer_;
            else if (settingsSel_ == 1 && editBuffer_.size() >= kMinPinLen) s.pin = editBuffer_;
            editingText_ = false;
            app_->repo().persistSettings();
            if (ble_) ble_->notifyDataChanged();
            notify("Saved");
        }
        if (ks.esc) editingText_ = false;
        dirty_ = true;
        return;
    }

    switch (screen_) {
        case Screen::Home: {
            // Home is a 2-column grid: Left/Right move within a row,
            // Up/Down move between rows (wrapping).
            const int n = (int)menuItems_.size();
            if (navUp) sel_ = (sel_ + n - 2) % n;
            if (navDown) sel_ = (sel_ + 2) % n;
            if (navLeft && (sel_ % 2) == 1) sel_--;
            if (navRight && (sel_ % 2) == 0 && sel_ + 1 < n) sel_++;
            bool hotkey = false;
            for (char c : ks.word) {
                if (c >= '1' && c <= '9' && (c - '1') < (int)menuItems_.size()) {
                    sel_ = c - '1';
                    hotkey = true;
                }
            }
            if (ks.enter || hotkey) openMenu();
            break;
        }
        case Screen::Notes: {
            if (navUp && noteSel_ > 0) noteSel_--;
            if (navDown && noteSel_ + 1 < (int)noteList_.size()) noteSel_++;
            if (ks.enter) {
                if (noteList_.empty()) startNewNote();
                else screen_ = Screen::NoteView;
            }
            for (char c : ks.word) if (c == 'n' || c == 'N') startNewNote();
            if (ks.esc) gotoHome();
            break;
        }
        case Screen::NoteView: {
            if (ks.enter) startEditNote(noteList_[noteSel_]);
            if (ks.del || ks.backspace) {
                Note stored;
                app_->repo().deleteNote(noteList_[noteSel_].id, noteList_[noteSel_].rev, stored);
                notify("Deleted");
                openNotes();
            }
            if (ks.esc) openNotes();
            break;
        }
        case Screen::NoteEdit: {
            for (char c : ks.word) fields_[field_].push_back(c);
            if (ks.space) fields_[field_].push_back(' ');
            if (ks.backspace || ks.del) {
                if (!fields_[field_].empty()) fields_[field_].pop_back();
            }
            if (ks.tab) field_ = (field_ + 1) % 2;
            if (ks.enter) commitNoteEdit();
            if (ks.esc) openNotes();
            break;
        }
        case Screen::Homework: {
            if (navUp && hwSel_ > 0) hwSel_--;
            if (navDown && hwSel_ + 1 < (int)hwList_.size()) hwSel_++;
            if (ks.enter) {
                if (hwList_.empty()) startNewHomework();
                else startEditHomework(hwList_[hwSel_]);
            }
            for (char c : ks.word) if (c == 'n' || c == 'N') startNewHomework();
            if (ks.esc) gotoHome();
            break;
        }
        case Screen::HwEdit: {
            if (field_ == 3) {
                if (ks.up || ks.left) {
                    int v = (statusIndex(fields_[3]) + 2) % 3;
                    fields_[3] = statusName(v);
                }
                if (ks.down || ks.right) {
                    int v = (statusIndex(fields_[3]) + 1) % 3;
                    fields_[3] = statusName(v);
                }
            } else {
                for (char c : ks.word) fields_[field_].push_back(c);
                if (ks.space) fields_[field_].push_back(' ');
                if (ks.backspace || ks.del) {
                    if (!fields_[field_].empty()) fields_[field_].pop_back();
                }
            }
            if (ks.tab) field_ = (field_ + 1) % 4;
            if (ks.enter) commitHwEdit();
            if (ks.esc) openHomework();
            break;
        }
        case Screen::Calculator: {
            for (char c : ks.word) {
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '*' ||
                    c == '/' || c == '(' || c == ')' || c == '.')
                    calcExpr_.push_back(c);
                if (c == '=') {
                    double r;
                    if (evalExpression(calcExpr_, r)) {
                        char b[32];
                        snprintf(b, sizeof(b), "%g", r);
                        calcResult_ = b;
                    } else {
                        calcResult_ = "err";
                    }
                }
            }
            if (ks.enter) {
                double r;
                if (evalExpression(calcExpr_, r)) {
                    char b[32];
                    snprintf(b, sizeof(b), "%g", r);
                    calcResult_ = b;
                } else {
                    calcResult_ = "err";
                }
            }
            if (ks.backspace || ks.del) {
                if (!calcExpr_.empty()) calcExpr_.pop_back();
            }
            if (ks.esc) gotoHome();
            break;
        }
        case Screen::Timer: {
            if (ks.enter) {
                if (timerRunning_) {
                    if (stopwatch_) stopwatchAccum_ += millis() - timerStart_;
                    timerRunning_ = false;
                } else {
                    timerStart_ = millis();
                    timerRunning_ = true;
                    timerDone_ = false;
                }
            }
            for (char c : ks.word) {
                if (c == 'r' || c == 'R') {
                    timerRunning_ = false;
                    timerDone_ = false;
                    stopwatchAccum_ = 0;
                    timerSeconds_ = 60;
                }
                if (c == 's' || c == 'S') {
                    stopwatch_ = !stopwatch_;
                    timerRunning_ = false;
                    timerDone_ = false;
                    stopwatchAccum_ = 0;
                }
            }
            if (!stopwatch_ && !timerRunning_) {
                if (navUp || ks.down || navRight || navLeft) timerDone_ = false;
                if (navUp) timerSeconds_ += 10;
                if (ks.down && timerSeconds_ > 10) timerSeconds_ -= 10;
                if (navRight) timerSeconds_ += 1;
                if (navLeft && timerSeconds_ > 1) timerSeconds_ -= 1;
            }
            if (ks.esc) gotoHome();
            break;
        }
        case Screen::Settings: {
            const int rows = 9;
            if (navUp && settingsSel_ > 0) settingsSel_--;
            if (navDown && settingsSel_ < rows - 1) settingsSel_++;
            if (settingsSel_ == 3) {
                if (navLeft && audioVolume() >= 5) {
                    audioSetVolume((uint8_t)(audioVolume() - 5));
                    audioBeep(880, 60);
                }
                if (navRight && audioVolume() <= 95) {
                    audioSetVolume((uint8_t)(audioVolume() + 5));
                    audioBeep(880, 60);
                }
            }
            if (ks.enter) {
                if (settingsSel_ == 2) {
                    audioSetEnabled(!audioEnabled());
                    if (audioEnabled()) audioBeep(1100, 120);
                    notify(audioEnabled() ? "Sound on" : "Sound off");
                } else if (settingsSel_ == 4) {
                    Settings& s = app_->settings();
                    s.apEnabled = !s.apEnabled;
                    wifi_->setEnabled(s.apEnabled);
                    app_->repo().persistSettings();
                    if (ble_) ble_->notifyDataChanged();
                    notify(s.apEnabled ? "AP on" : "AP off");
                } else if (settingsSel_ == 0) {
                    editBuffer_ = app_->settings().deviceName;
                    editingText_ = true;
                } else if (settingsSel_ == 1) {
                    editBuffer_.clear();
                    editingText_ = true;
                } else if (settingsSel_ == 8) {
                    app_->repo().persistSettings();
                    notify("Persisted");
                }
            }
            if (ks.esc) gotoHome();
            break;
        }
        case Screen::About: {
            if (ks.esc || ks.enter) gotoHome();
            break;
        }
        case Screen::Music: {
            int count = musicCount();
            if (count > 0) {
                if (navUp && musicSel_ > 0) musicSel_--;
                if (navDown && musicSel_ + 1 < count) musicSel_++;
                if (navLeft) {
                    musicPrev();
                    if (musicIndex() >= 0) musicSel_ = musicIndex();
                }
                if (navRight) {
                    musicNext();
                    if (musicIndex() >= 0) musicSel_ = musicIndex();
                }
                if (ks.enter) {
                    if (musicIndex() == musicSel_)
                        musicTogglePlay();
                    else
                        musicPlay(musicSel_);
                }
            }
            if (ks.esc) gotoHome();
            break;
        }
    }
    dirty_ = true;
}

void Ui::loop()
{
    M5Cardputer.update();
    readBattery();
    if (screen_ == Screen::Splash && millis() > splashUntil_) {
        gotoHome();
    }
    handleKeys();
    // Little popups when the phone pairs or finishes a sync.
    if (ble_) {
        if (ble_->consumePairedEvent()) notify("Paired");
        if (ble_->consumeSyncEvent()) notify("Synced");
    }
    // Countdown timer: fire the alarm once it reaches zero.
    if (!stopwatch_ && timerRunning_ && timerSeconds_ > 0 &&
        (millis() - timerStart_) / 1000u >= (uint32_t)timerSeconds_) {
        timerRunning_ = false;
        timerDone_ = true;
        alarmUntil_ = millis() + 4000;
        alarmNextBeep_ = millis();
        noteActivity();  // wake the panel so the alarm is seen
        notify("Time's up!");
    }
    if (alarmUntil_ != 0) {
        if ((int32_t)(millis() - alarmUntil_) < 0) {
            if ((int32_t)(millis() - alarmNextBeep_) >= 0) {
                audioBeep(1400, 160);
                alarmNextBeep_ += 350;
            }
        } else {
            alarmUntil_ = 0;
        }
    }
    if (dirty_ && !asleep_) draw();
    // Live-updating screens.
    if (screen_ == Screen::Timer && timerRunning_ && !asleep_) drawTimer();
    updatePower();
}

}  // namespace nexo
