#pragma once
// User preferences. Compile-time for now; will move to NVS + a Settings screen.

#include <stdint.h>

namespace settings {

constexpr bool     USE_24H         = false;   // 12-hour clock with AM/PM
constexpr bool     FAHRENHEIT      = true;    // weather is stored in °C, converted for display
constexpr bool     WEEK_STARTS_MONDAY = false; // calendar week strip starts on Sunday
constexpr uint32_t NAV_LOCKOUT_MS  = 100;     // input ignored right after a screen change

// Network
constexpr uint32_t WEATHER_REFRESH_MS   = 15UL * 60 * 1000;   // Open-Meteo updates hourly; 15 min is plenty
constexpr uint32_t WEATHER_STALE_MS     = 2UL * 60 * 60 * 1000; // flag weather older than this on Home
constexpr uint32_t FIRST_CONNECT_MS     = 45000;  // no connection since boot -> open setup portal
constexpr uint16_t PORTAL_TIMEOUT_S     = 180;    // auto-opened portal closes after idle, then WiFi retries
constexpr const char* NTP_SERVER_1      = "pool.ntp.org";
constexpr const char* NTP_SERVER_2      = "time.google.com";
constexpr const char* HOSTNAME          = "desk-companion";

// Calendar
constexpr bool     DEMO_CALENDAR        = false;  // sample events when no PC calendar exists (UI testing)
constexpr uint32_t CALENDAR_STALE_S     = 3UL * 60 * 60;  // flag a PC calendar older than this on Home

// Night dimming: on-board light sensor (GPIO34), raw ADC. Darker = higher.
// Measured on the dev unit 2026-11-07: room light 0, sensor covered ~210-240.
constexpr int      LIGHT_NIGHT_ON       = 150;   // switch to night above this (held 8 s)
constexpr int      LIGHT_NIGHT_OFF      = 60;    // back to day below this

// Diagnostics
constexpr bool     TOUCH_LOG       = false;   // log touch events + screen changes to serial
constexpr bool     WIFIMANAGER_LOG = false;   // verbose setup-portal logging (diagnosing phone drop-outs)
constexpr bool     LIGHT_LOG       = false;   // light-sensor readings every 5 s (calibrating night dimming)

}  // namespace settings
