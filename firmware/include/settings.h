#pragma once
// User preferences. Compile-time for now; will move to NVS + a Settings screen.

#include <stdint.h>

namespace settings {

constexpr bool     USE_24H         = false;   // 12-hour clock with AM/PM
constexpr bool     FAHRENHEIT      = true;    // weather is stored in °C, converted for display
constexpr uint32_t IDLE_RETURN_MS  = 60000;   // other screens fall back to Home after this
constexpr uint32_t NAV_LOCKOUT_MS  = 100;     // input ignored right after a screen change

// Diagnostics
constexpr bool     TOUCH_LOG       = true;    // log touch events + screen changes to serial

}  // namespace settings
