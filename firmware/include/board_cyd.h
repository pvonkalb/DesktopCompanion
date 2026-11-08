#pragma once
// Hardware map for the ESP32-2432S028R (Cheap Yellow Display).
// Application code must reference these names, never raw GPIO numbers.
// Display SPI pins live in platformio.ini (TFT_eSPI needs them at library compile time).

#include <stdint.h>

namespace board {

// Display
constexpr int8_t   PIN_BACKLIGHT   = 21;   // active HIGH
constexpr uint8_t  DISPLAY_ROTATION = 1;   // landscape, USB connector on the right
constexpr uint16_t SCREEN_W        = 320;
constexpr uint16_t SCREEN_H        = 240;

// XPT2046 resistive touch - separate bus (VSPI), NOT shared with the display.
// Not used yet; verified in Milestone 2.
constexpr int8_t PIN_TOUCH_CLK  = 25;
constexpr int8_t PIN_TOUCH_MOSI = 32;
constexpr int8_t PIN_TOUCH_MISO = 39;      // input-only pin
constexpr int8_t PIN_TOUCH_CS   = 33;
constexpr int8_t PIN_TOUCH_IRQ  = 36;      // input-only pin, active LOW

// On-board RGB LED - active LOW (HIGH = off)
constexpr int8_t PIN_LED_R = 4;
constexpr int8_t PIN_LED_G = 16;
constexpr int8_t PIN_LED_B = 17;

// Light sensor (LDR), analog input
constexpr int8_t PIN_LDR = 34;

void initPins();                 // safe default state for all board peripherals
void setBacklight(bool on);

}  // namespace board
