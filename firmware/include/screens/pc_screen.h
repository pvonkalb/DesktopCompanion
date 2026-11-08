#pragma once
// PC: four tiles - CPU %, GPU % + temperature, memory, network down/up.
// Data from the companion every 2 s (Windows counters + NVIDIA NVML; no drivers).

#include "ui/screen.h"
#include "ui/widgets.h"

class PcScreen : public ui::Screen {
public:
    PcScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;
    bool idleReturnsHome() const override;   // stays up while live stats arrive (in-game glance)

private:
    void drawTiles(TFT_eSPI& tft);
    void drawTile(TFT_eSPI& tft, int index);

    ui::Rect back_[1];
    ui::ButtonGroup buttons_;
    uint32_t version_ = 0;
    int linkKey_ = -1;
    uint32_t lastCheck_ = 0;
};
