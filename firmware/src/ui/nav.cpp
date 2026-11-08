#include <Arduino.h>

#include "settings.h"
#include "ui/screen.h"

namespace ui {
namespace nav {
namespace {

Screen* screens[static_cast<size_t>(ScreenId::Count)] = {};
ScreenId active = ScreenId::Home;
ScreenId pending = ScreenId::Home;
bool hasPending = false;
uint32_t lastTouchMs = 0;

Screen* get(ScreenId id) { return screens[static_cast<size_t>(id)]; }

void enter(TFT_eSPI& tft, ScreenId id, uint32_t now) {
    if (!get(id)) {
        Serial.printf("[nav] screen %u not registered\n", static_cast<unsigned>(id));
        return;
    }
    active = id;
    lastTouchMs = now;
    get(id)->enter(tft, now);
}

}  // namespace

void add(ScreenId id, Screen* screen) { screens[static_cast<size_t>(id)] = screen; }

void begin(TFT_eSPI& tft, ScreenId first) { enter(tft, first, millis()); }

void go(ScreenId id) {
    pending = id;
    hasPending = true;
}

ScreenId current() { return active; }

void loop(TFT_eSPI& tft, uint32_t now) {
    Screen* s = get(active);
    if (!s) return;

    touch_input::Event ev;
    while (touch_input::poll(now, ev)) {
        lastTouchMs = now;
        s->handle(tft, ev, now);
    }
    if (!hasPending) s->update(tft, now);

    if (!hasPending && active != ScreenId::Home && s->idleReturnsHome() &&
        now - lastTouchMs >= settings::IDLE_RETURN_MS && !touch_input::isDown()) {
        go(ScreenId::Home);
    }
    if (hasPending) {
        hasPending = false;
        if (pending != active || pending == ScreenId::Calibration) enter(tft, pending, now);
    }
}

}  // namespace nav
}  // namespace ui
