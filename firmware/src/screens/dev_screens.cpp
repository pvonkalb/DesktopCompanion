#include "screens/dev_screens.h"

#include "calibration_screen.h"
#include "touch_test.h"
#include "ui/ui.h"

// These modules draw with TFT_eSPI's built-in fonts, so any smooth font loaded
// on the panel must be released first (a loaded smooth font overrides them).

void CalibrationScreen::enter(TFT_eSPI& tft, uint32_t) {
    ui::clearFont(tft);
    calibration_screen::begin(tft);
}

void CalibrationScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) {
    calibration_screen::handle(tft, ev, nowMs);
}

void CalibrationScreen::update(TFT_eSPI& tft, uint32_t nowMs) {
    calibration_screen::update(tft, nowMs);
    if (calibration_screen::done()) ui::nav::go(ui::ScreenId::Home);
}

void TouchTestScreen::enter(TFT_eSPI& tft, uint32_t) {
    ui::clearFont(tft);
    touch_test::begin(tft);
}

void TouchTestScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    touch_test::handle(tft, ev);
    if (touch_test::wantsRecalibration()) ui::nav::go(ui::ScreenId::Calibration);
}
