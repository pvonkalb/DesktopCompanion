// Desktop Companion firmware.
// Current stage: Milestone 2a - raw touch diagnostic.
// (display_test is kept for regression checks: swap it back in below.)
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "board_cyd.h"
#include "touch_test.h"

namespace {
TFT_eSPI tft;  // pins/driver come from build_flags in platformio.ini
}

void setup() {
    Serial.begin(115200);
    board::initPins();

    tft.init();
    tft.setRotation(board::DISPLAY_ROTATION);
    touch_test::begin(tft);
    board::setBacklight(true);   // reveal only after the first full frame is drawn

    Serial.printf("[boot] Desktop Companion  size=%dx%d heap=%lu\n",
                  tft.width(), tft.height(), static_cast<unsigned long>(ESP.getFreeHeap()));
}

void loop() {
    touch_test::update(tft, millis());
}
