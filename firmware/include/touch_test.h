#pragma once
// Milestone 2b: calibrated touch test.
// Drawing canvas (accuracy/edge check) + two large buttons (Done, Calibrate).
// Long-press on the canvas clears it.

#include <TFT_eSPI.h>

#include "touch_input.h"

namespace touch_test {

void begin(TFT_eSPI& tft);
void handle(TFT_eSPI& tft, const touch_input::Event& ev);
bool wantsRecalibration();   // set when the Calibrate button is tapped
bool wantsExit();            // set when the Done button is tapped

}  // namespace touch_test
