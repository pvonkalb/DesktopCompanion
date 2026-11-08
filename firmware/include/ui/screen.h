#pragma once
// Screen interface + navigator.
//
// Each screen owns its layout and redraw logic. The navigator owns the loop:
// it dispatches touch events to the active screen, calls update() every
// iteration, applies screen changes between iterations (never mid-draw), and
// returns to Home after a period without touches.

#include <TFT_eSPI.h>

#include "touch_input.h"

namespace ui {

enum class ScreenId : uint8_t {
    Home, Weather, Calendar, Launcher, Calibration, TouchTest,
    Focus, Timer, Music, PcStats, Settings,
    Count
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual void enter(TFT_eSPI& tft, uint32_t nowMs) = 0;    // draw everything
    virtual void handle(TFT_eSPI&, const touch_input::Event&, uint32_t) {}
    virtual void update(TFT_eSPI&, uint32_t) {}                // redraw only what changed
    virtual bool idleReturnsHome() const { return true; }
};

namespace nav {

void add(ScreenId id, Screen* screen);
void begin(TFT_eSPI& tft, ScreenId first);
void go(ScreenId id);                // takes effect at the end of the current loop iteration
void back();                         // previous screen (Home if there is no history)
void loop(TFT_eSPI& tft, uint32_t nowMs);
ScreenId current();

}  // namespace nav
}  // namespace ui
