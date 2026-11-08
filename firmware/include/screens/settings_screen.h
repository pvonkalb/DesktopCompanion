#pragma once
// Settings: device actions (Wi-Fi & location, touch calibration, touch test) + status info.

#include "ui/screen.h"
#include "ui/widgets.h"

class SettingsScreen : public ui::Screen {
public:
    SettingsScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;   // live light reading on the dimming row

private:
    int lastAmbient_ = -1;
    uint32_t lastAmbientMs_ = 0;
    static constexpr int ACTIONS = 4;
    void drawAction(TFT_eSPI& tft, int index, bool pressed);
    void drawInfo(TFT_eSPI& tft);

    ui::Rect rects_[ACTIONS + 1];   // [0] = back
    ui::ButtonGroup buttons_;
};
