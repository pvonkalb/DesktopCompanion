#pragma once
// Settings: device actions (touch calibration, touch test) + read-only info.

#include "ui/screen.h"
#include "ui/widgets.h"

class SettingsScreen : public ui::Screen {
public:
    SettingsScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;

private:
    static constexpr int ACTIONS = 2;
    void drawAction(TFT_eSPI& tft, int index, bool pressed);
    void drawInfo(TFT_eSPI& tft);

    ui::Rect rects_[ACTIONS + 1];   // [0] = back
    ui::ButtonGroup buttons_;
};
