#pragma once
// [Timer | Stopwatch]
//   Timer     - set with -/+ (1 min) or presets; Start / Pause / Resume / Reset;
//               "Time's up" alert with +1 min / Done. Keeps running off-screen.
//   Stopwatch - Start / Lap / Stop / Reset, tenths, last two laps.

#include "ui/screen.h"
#include "ui/widgets.h"

class TimerScreen : public ui::Screen {
public:
    TimerScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;
    bool idleReturnsHome() const override;

    void showStopwatch(bool sw) { tab_ = sw ? 1 : 0; }

private:
    enum Target { BACK, TAB_TIMER, TAB_SW, MINUS, PLUS, PRESET0, PRESET4 = PRESET0 + 4, LEFT, RIGHT, FULL, COUNT };

    int layoutKey(uint32_t now) const;        // changes whenever the button set changes
    void layout();                            // place / hide targets for the current state
    void drawBody(TFT_eSPI& tft, uint32_t now);
    void drawTime(TFT_eSPI& tft, uint32_t now);
    void drawInfo(TFT_eSPI& tft, uint32_t now);
    void drawTarget(TFT_eSPI& tft, int i, bool pressed);
    void activate(TFT_eSPI& tft, int i, uint32_t now);
    void labels(const char*& left, const char*& right, const char*& full, bool& leftAccent,
                bool& rightAccent) const;

    ui::Rect rects_[COUNT];
    ui::ButtonGroup buttons_;
    int tab_ = 0;
    int lastKey_ = -1;
    uint32_t lastTimeDraw_ = 0;
    char lastTime_[16] = "";
    bool flash_ = false;
};
