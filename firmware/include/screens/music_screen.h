#pragma once
// Music: whatever is playing on the PC (via the companion) with previous / play-pause /
// next and master volume (-, +, mute). Updates optimistically on tap; the PC's
// next report (within ~1 s) is the truth.

#include "ui/screen.h"
#include "ui/widgets.h"

class MusicScreen : public ui::Screen {
public:
    MusicScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;
    bool idleReturnsHome() const override;   // stays up while the PC is connected

private:
    enum Target { BACK, PREV, PLAY, NEXT, MUTE, VOL_DOWN, VOL_UP, COUNT };

    void drawAll(TFT_eSPI& tft);
    void drawInfo(TFT_eSPI& tft);
    void drawVolume(TFT_eSPI& tft);
    void drawTarget(TFT_eSPI& tft, int i, bool pressed);
    bool available() const;

    ui::Rect rects_[COUNT];
    ui::ButtonGroup buttons_;
    uint32_t mediaVersion_ = 0;
    int linkKey_ = -1;   // connected / has-data combination last drawn
    uint32_t lastCheck_ = 0;
};
