#pragma once
// XPT2046 resistive touch on its own SPI bus (VSPI). Pins: board_cyd.h.
//
// Call poll() every loop iteration. It samples at a fixed rate, debounces
// press/release, smooths position, applies calibration and emits events.

#include <stdint.h>

#include "touch_calibration.h"

namespace touch_input {

struct RawSample {
    int16_t x = 0;      // raw ADC, ~0..4095 (library rotation 1 applied)
    int16_t y = 0;
    int16_t z = 0;      // pressure
    bool touched = false;
};

enum class EventType : uint8_t { Press, Move, Release, Tap, LongPress };

struct Event {
    EventType type;
    int16_t x, y;       // calibrated screen position (clamped to the screen)
};

void begin();
void setCalibration(const touch_cal::Matrix& m);

bool poll(uint32_t nowMs, Event& out);    // true when an event was dequeued

bool isDown();                             // debounced touch state
touch_cal::Point smoothedRaw();            // filtered raw position (valid while isDown)
bool irqActive();                          // IRQ line LOW = panel touched (no SPI)
RawSample readRaw();                       // single unfiltered read (diagnostics)

const char* eventName(EventType t);

}  // namespace touch_input
