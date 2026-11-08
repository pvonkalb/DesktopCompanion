#pragma once
// Preferences the user changes on the device (Settings screen), persisted in NVS "prefs".
// Compile-time defaults live in settings.h.

#include <stdint.h>

namespace user_prefs {

void load();

// How long a non-Home screen may sit untouched before returning Home. 0 = never.
uint32_t idleReturnMs();
const char* idleReturnLabel();   // "Off", "30 s", "1 min", ...
void cycleIdleReturn();          // next option, saved immediately

// Night dimming from the light sensor.
bool autoDim();
void toggleAutoDim();

}  // namespace user_prefs
