#pragma once
// Milestone 1 display diagnostic screen.
// Verifies: orientation, full-panel extent (no offset/clipping), colour order,
// colour inversion, greyscale, font rendering, SPI fill speed, partial redraws.

#include <TFT_eSPI.h>

namespace display_test {

void begin(TFT_eSPI& tft);              // draws the static diagnostic once
void update(TFT_eSPI& tft, uint32_t nowMs);  // non-blocking; redraws only dynamic regions
uint32_t fillTimeMs();                  // measured full-screen fill time

}  // namespace display_test
