#pragma once
// Adapters that host the Milestone 2 calibration and touch-test modules as screens.

#include "ui/screen.h"

class CalibrationScreen : public ui::Screen {
public:
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;
    bool idleReturnsHome() const override { return false; }
};

class TouchTestScreen : public ui::Screen {
public:
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
};
