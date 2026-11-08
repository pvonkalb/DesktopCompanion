// Desktop Companion firmware - Milestone 1: display bring-up.
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "board_cyd.h"
#include "display_test.h"

namespace {
TFT_eSPI tft;  // pins/driver come from build_flags in platformio.ini
}

void setup() {
    Serial.begin(115200);
    board::initPins();

    tft.init();
    tft.setRotation(board::DISPLAY_ROTATION);
    display_test::begin(tft);
    board::setBacklight(true);   // reveal only after the first full frame is drawn

    Serial.println();
    Serial.println(F("[boot] Desktop Companion - display diagnostic"));
    Serial.printf("[boot] rotation=%u size=%dx%d fill=%lums cpu=%luMHz heap=%lu\n",
                  board::DISPLAY_ROTATION, tft.width(), tft.height(),
                  static_cast<unsigned long>(display_test::fillTimeMs()),
                  static_cast<unsigned long>(getCpuFrequencyMhz()),
                  static_cast<unsigned long>(ESP.getFreeHeap()));
}

void loop() {
    display_test::update(tft, millis());
}
