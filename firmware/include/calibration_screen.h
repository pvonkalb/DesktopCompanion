#pragma once
// Touch calibration: 4 corner targets (fit) + 1 centre target (verify).
// On success the new calibration is saved to NVS and applied to touch_input.

#include <TFT_eSPI.h>

#include "touch_input.h"

namespace calibration_screen {

void begin(TFT_eSPI& tft);
void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs);
void update(TFT_eSPI& tft, uint32_t nowMs);   // non-blocking: sampling + timed messages
bool done();                                   // true once a calibration was saved

}  // namespace calibration_screen
