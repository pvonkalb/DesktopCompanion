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

namespace {
constexpr uint8_t BL_CHANNEL = 7;      // LEDC channel (0-7 are free; TFT/touch don't use LEDC)
constexpr uint32_t BL_FREQ_HZ = 5000;  // above audible / flicker range
bool pwmReady = false;
}  // namespace

void setBrightness(uint8_t level) {
    if (!pwmReady) {   // switch GPIO21 from plain output to PWM on first use
        ledcSetup(BL_CHANNEL, BL_FREQ_HZ, 8);
        ledcAttachPin(PIN_BACKLIGHT, BL_CHANNEL);
        pwmReady = true;
    }
    ledcWrite(BL_CHANNEL, level);
}

void setBacklight(bool on) { setBrightness(on ? 255 : 0); }

int readAmbientRaw() { return analogRead(PIN_LDR); }

void setLed(bool r, bool g, bool b) {
    digitalWrite(PIN_LED_R, r ? LOW : HIGH);
    digitalWrite(PIN_LED_G, g ? LOW : HIGH);
    digitalWrite(PIN_LED_B, b ? LOW : HIGH);
}

}  // namespace board
