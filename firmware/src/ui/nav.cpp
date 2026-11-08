#include <Arduino.h>

#include "settings.h"
#include "ui/screen.h"
#include "user_prefs.h"

namespace ui {
namespace nav {
namespace {

Screen* screens[static_cast<size_t>(ScreenId::Count)] = {};
ScreenId active = ScreenId::Home;
ScreenId pending = ScreenId::Home;
bool hasPending = false;
bool pendingIsBack = false;
uint32_t lastTouchMs = 0;

// Back history. Going Home clears it, so it never grows past a few levels.
constexpr uint8_t HISTORY_MAX = 6;
ScreenId history[HISTORY_MAX];
uint8_t historyLen = 0;

// After a screen change the new screen only receives gestures that *start*
// after the lockout, so the tail of the gesture that caused the change (or
// lift-off noise from a resistive panel) can never trigger it.
uint32_t lockoutUntil = 0;
bool armed = true;

const char* const NAMES[] = {"Home", "Weather", "Calendar", "Launcher", "Calibration", "TouchTest",
                             "Focus", "Timer", "Music", "PcStats", "Settings", "Setup"};
static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == static_cast<size_t>(ScreenId::Count), "NAMES out of sync");

Screen* get(ScreenId id) { return screens[static_cast<size_t>(id)]; }

void enter(TFT_eSPI& tft, ScreenId id, uint32_t now) {
    if (!get(id)) {
        Serial.printf("[nav] screen %u not registered\n", static_cast<unsigned>(id));
        return;
    }
    if (settings::TOUCH_LOG)
        Serial.printf("[nav]   %7lu -> %s\n", static_cast<unsigned long>(now), NAMES[static_cast<size_t>(id)]);
    active = id;
    lastTouchMs = now;
    get(id)->enter(tft, now);
    lockoutUntil = millis() + settings::NAV_LOCKOUT_MS;   // after drawing, which takes time
    armed = false;
}

bool accept(const touch_input::Event& ev, uint32_t now) {
    if (armed) return true;
    if (ev.type == touch_input::EventType::Press && static_cast<int32_t>(now - lockoutUntil) >= 0) {
        armed = true;
        return true;
    }
    if (settings::TOUCH_LOG)
        Serial.printf("[nav] dropped %s %d,%d (screen change lockout)\n",
                      touch_input::eventName(ev.type), ev.x, ev.y);
    return false;
}

}  // namespace

void add(ScreenId id, Screen* screen) { screens[static_cast<size_t>(id)] = screen; }

void begin(TFT_eSPI& tft, ScreenId first) { enter(tft, first, millis()); }

void go(ScreenId id) {
    pending = id;
    pendingIsBack = false;
    hasPending = true;
}

void back() {
    pending = historyLen ? history[historyLen - 1] : ScreenId::Home;
    pendingIsBack = true;
    hasPending = true;
}

ScreenId current() { return active; }

void loop(TFT_eSPI& tft, uint32_t now) {
    Screen* s = get(active);
    if (!s) return;

    touch_input::Event ev;
    while (touch_input::poll(now, ev)) {
        lastTouchMs = now;
        if (accept(ev, now)) s->handle(tft, ev, now);
    }
    if (!hasPending) s->update(tft, now);

    const uint32_t idleMs = user_prefs::idleReturnMs();   // 0 = never (Settings > Return to Home)
    if (!hasPending && idleMs && active != ScreenId::Home && s->idleReturnsHome() &&
        now - lastTouchMs >= idleMs && !touch_input::isDown()) {
        go(ScreenId::Home);
    }
    if (hasPending) {
        hasPending = false;
        if (pending == active && pending != ScreenId::Calibration) return;
        if (pending == ScreenId::Home) {
            historyLen = 0;
        } else if (pendingIsBack) {
            if (historyLen) --historyLen;
        } else if (active != ScreenId::Calibration) {   // never "back" into a finished calibration
            if (historyLen == HISTORY_MAX) {             // drop the oldest entry
                for (uint8_t i = 1; i < HISTORY_MAX; ++i) history[i - 1] = history[i];
                --historyLen;
            }
            history[historyLen++] = active;
        }
        enter(tft, pending, now);
    }
}

}  // namespace nav
}  // namespace ui
