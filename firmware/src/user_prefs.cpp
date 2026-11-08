#include "user_prefs.h"

#include <Arduino.h>
#include <Preferences.h>

namespace user_prefs {
namespace {

constexpr const char* NVS_NAMESPACE = "prefs";
constexpr const char* KEY_IDLE = "idle";

struct IdleOption {
    uint32_t ms;
    const char* label;
};
constexpr IdleOption IDLE_OPTIONS[] = {
    {0, "Off"}, {30000, "30 s"}, {60000, "1 min"}, {120000, "2 min"}, {300000, "5 min"},
};
constexpr uint8_t IDLE_COUNT = sizeof(IDLE_OPTIONS) / sizeof(IDLE_OPTIONS[0]);
constexpr uint8_t IDLE_DEFAULT = 2;   // 1 min

uint8_t idleIndex = IDLE_DEFAULT;

constexpr const char* KEY_DIM = "dim";
constexpr bool DIM_DEFAULT = true;    // calibrated on hardware 2026-11-07
bool dim = DIM_DEFAULT;

}  // namespace

bool autoDim() { return dim; }

void toggleAutoDim() {
    dim = !dim;
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putBool(KEY_DIM, dim);
        prefs.end();
    }
    Serial.printf("[prefs] night dimming: %s\n", dim ? "auto" : "off");
}

void load() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    const uint8_t v = prefs.getUChar(KEY_IDLE, IDLE_DEFAULT);
    idleIndex = v < IDLE_COUNT ? v : IDLE_DEFAULT;
    dim = prefs.getBool(KEY_DIM, DIM_DEFAULT);
    prefs.end();
}

uint32_t idleReturnMs() { return IDLE_OPTIONS[idleIndex].ms; }
const char* idleReturnLabel() { return IDLE_OPTIONS[idleIndex].label; }

void cycleIdleReturn() {
    idleIndex = (idleIndex + 1) % IDLE_COUNT;
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.putUChar(KEY_IDLE, idleIndex);
        prefs.end();
    }
    Serial.printf("[prefs] return to Home: %s\n", idleReturnLabel());
}

}  // namespace user_prefs
