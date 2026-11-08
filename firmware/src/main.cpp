// Desktop Companion firmware.
// Current stage: Milestone 2b - touch calibration + calibrated touch test.
// (display_test is kept for display regression checks.)
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "board_cyd.h"
#include "calibration_screen.h"
#include "touch_calibration.h"
#include "touch_input.h"
#include "touch_test.h"

namespace {

TFT_eSPI tft;  // pins/driver come from build_flags in platformio.ini

enum class Mode { Calibrating, TouchTest };
Mode mode = Mode::TouchTest;

void enter(Mode m) {
    mode = m;
    if (m == Mode::Calibrating) calibration_screen::begin(tft);
    else touch_test::begin(tft);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    board::initPins();

    tft.init();
    tft.setRotation(board::DISPLAY_ROTATION);
    touch_input::begin();

    touch_cal::Matrix cal;
    const bool haveCal = touch_cal::load(cal);
    if (haveCal) touch_input::setCalibration(cal);
    const bool forceCal = touch_input::irqActive();   // finger on the screen at boot = recalibrate

    Serial.printf("[boot] Desktop Companion  size=%dx%d heap=%lu  touch cal: %s%s\n",
                  tft.width(), tft.height(), static_cast<unsigned long>(ESP.getFreeHeap()),
                  haveCal ? "stored" : "none (using defaults)", forceCal ? ", forced by touch" : "");

    enter(!haveCal || forceCal ? Mode::Calibrating : Mode::TouchTest);
    board::setBacklight(true);   // reveal only after the first full frame is drawn
}

void loop() {
    const uint32_t now = millis();

    touch_input::Event ev;
    while (touch_input::poll(now, ev)) {
        if (mode == Mode::Calibrating) calibration_screen::handle(tft, ev, now);
        else touch_test::handle(tft, ev);
    }

    if (mode == Mode::Calibrating) {
        calibration_screen::update(tft, now);
        if (calibration_screen::done()) enter(Mode::TouchTest);
    } else if (touch_test::wantsRecalibration()) {
        enter(Mode::Calibrating);
    }
}
