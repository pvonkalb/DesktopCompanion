#pragma once
// XPT2046 resistive touch on its own SPI bus (VSPI). Pins: board_cyd.h.

#include <stdint.h>

namespace touch_input {

struct RawSample {
    int16_t x = 0;      // raw ADC, ~0..4095 (library rotation 1 applied)
    int16_t y = 0;
    int16_t z = 0;      // pressure
    bool touched = false;
};

void begin();
bool irqActive();          // true while the IRQ line reports a touch (cheap, no SPI)
RawSample readRaw();       // one SPI read; touched = pressure above threshold

}  // namespace touch_input
