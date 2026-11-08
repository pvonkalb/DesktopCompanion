#include "board_cyd.h"
#include <Arduino.h>

namespace board {

void initPins() {
    // Backlight off until the first frame is drawn (avoids showing boot noise).
    pinMode(PIN_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_BACKLIGHT, LOW);

    // RGB LED is active LOW; drive HIGH so it stays dark.
    for (int8_t pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }
}

void setBacklight(bool on) {
    digitalWrite(PIN_BACKLIGHT, on ? HIGH : LOW);
}

void setLed(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r ? LOW : HIGH);
    digitalWrite(PIN_LED_G, g ? LOW : HIGH);
    digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

}  // namespace board
