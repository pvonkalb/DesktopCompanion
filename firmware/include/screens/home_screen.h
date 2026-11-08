#pragma once
// Ambient home: clock + date (left), current weather (right), "Up next" card.
// Tap weather -> Weather, tap card -> Calendar, long-press anywhere -> Launcher.

#include "ui/screen.h"

class HomeScreen : public ui::Screen {
public:
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;

private:
    void drawClock(TFT_eSPI& tft);
    void drawWeather(TFT_eSPI& tft);
    void drawEvents(TFT_eSPI& tft);
    void drawCardOutline(TFT_eSPI& tft, bool pressed);

    int lastMinute_ = -1;
    uint32_t weatherVersion_ = 0;
    uint32_t calendarVersion_ = 0;
    uint32_t lastCheckMs_ = 0;
    bool cardPressed_ = false;
    bool weatherPressed_ = false;
};
