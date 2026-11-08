#pragma once
// User preferences. Compile-time for now; will move to NVS + a Settings screen.

#include <stdint.h>

namespace settings {

constexpr bool     USE_24H         = false;   // 12-hour clock with AM/PM
constexpr bool     FAHRENHEIT      = true;    // weather is stored in °C, converted for display
constexpr bool     WEEK_STARTS_MONDAY = false; // calendar week strip starts on Sunday
constexpr uint32_t IDLE_RETURN_MS  = 60000;   // other screens fall back to Home after this
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

// Diagnostics
constexpr bool     TOUCH_LOG       = true;    // log touch events + screen changes to serial
constexpr bool     WIFIMANAGER_LOG = false;   // verbose setup-portal logging (diagnosing phone drop-outs)

}  // namespace settings
