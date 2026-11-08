#pragma once
// Focus (pomodoro): ring showing time left in the current phase, session dots,
// focus length adjustable before starting (15/25/30/45/50 min), 5 min breaks and
// a 15 min break after 4 sessions. Alerts when a phase ends.

#include "ui/screen.h"
#include "ui/widgets.h"

class FocusScreen : public ui::Screen {
public:
    FocusScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;
    bool idleReturnsHome() const override;

private:
    enum Target { BACK, PRIMARY, SECONDARY, COUNT };

    int stateKey() const;
    void labels(const char*& primary, const char*& secondary, bool& primaryAccent) const;
    void drawAll(TFT_eSPI& tft, uint32_t now);
    void drawRing(TFT_eSPI& tft, uint32_t now);
    void drawText(TFT_eSPI& tft, uint32_t now);
    void drawTarget(TFT_eSPI& tft, int i, bool pressed);

    ui::Rect rects_[COUNT];
    ui::ButtonGroup buttons_;
    int lastKey_ = -1;
    uint32_t lastTick_ = 0;
    char lastTime_[12] = "";
    char secondaryLabel_[16] = "";
    bool flash_ = false;
};
