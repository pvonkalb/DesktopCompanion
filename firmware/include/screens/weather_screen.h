#pragma once
// Weather: [Hourly | 7 Day] tabs.
//   Hourly - current conditions summary + next 12 hours, 6 per page (tap/swipe to page).
//   7 Day  - one row per day with a low/high range bar scaled to the week.

#include "ui/screen.h"
#include "ui/widgets.h"

class WeatherScreen : public ui::Screen {
public:
    WeatherScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;

private:
    static constexpr int TARGETS = 4;   // back, Hourly tab, 7 Day tab, content

    void drawTabs(TFT_eSPI& tft, int pressed = -1);
    void drawContent(TFT_eSPI& tft);
    void drawSummary(TFT_eSPI& tft);
    void drawHourly(TFT_eSPI& tft);
    void drawDaily(TFT_eSPI& tft);
    void drawTarget(TFT_eSPI& tft, int index, bool pressed);

    ui::Rect rects_[TARGETS];
    ui::ButtonGroup buttons_;
    int tab_ = 0;          // 0 = hourly, 1 = 7 day
    int page_ = 0;         // hourly page
    int16_t pressX_ = 0, pressY_ = 0;
    uint32_t weatherVersion_ = 0;
};
