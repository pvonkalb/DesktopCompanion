#pragma once
// App launcher: header + grid of large tiles (4 on top, 3 centred below).

#include "ui/screen.h"
#include "ui/widgets.h"

class LauncherScreen : public ui::Screen {
public:
    LauncherScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;

private:
    static constexpr int APPS = 7;
    void drawTile(TFT_eSPI& tft, int app, bool pressed);

    ui::Rect rects_[APPS + 1];   // [0] = back, [1..] = tiles
    ui::ButtonGroup buttons_;
};
