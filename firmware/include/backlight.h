#pragma once
// Backlight brightness: night auto-dim from the on-board light sensor, and a soft
// fade-in after each screen change (hides the redraw, reads as a smooth transition).
// Non-blocking: loop() advances fades and samples the sensor.

#include <stdint.h>

namespace backlight {

void begin();                 // starts dark; call show() once the first frame is drawn
void show();                  // fade up to the current target
void loop(uint32_t nowMs);
void dipForRedraw();          // drop low now; loop() fades back up
bool isNight();
int ambient();                // smoothed sensor reading (for diagnostics)
void suspend(bool full);      // hold at full brightness (e.g. firmware update screen)

}  // namespace backlight
