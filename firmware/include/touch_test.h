#pragma once
// Milestone 2a: raw touch diagnostic.
// Shows live raw X/Y/pressure + IRQ state, and plots an UNCALIBRATED position
// estimate so axis orientation/swap problems are visible before calibration.

#include <TFT_eSPI.h>

namespace touch_test {

void begin(TFT_eSPI& tft);
void update(TFT_eSPI& tft, uint32_t nowMs);   // non-blocking

}  // namespace touch_test
