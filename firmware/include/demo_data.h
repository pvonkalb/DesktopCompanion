#pragma once
// Placeholder data so the UI can be built before WiFi/NTP/weather/calendar exist.
// Everything it writes is marked Source::Demo and the UI labels it as demo data.

namespace demo_data {

void setClockFromBuildTime();   // sets the RTC to the firmware build time (local)
void loadWeather();             // sample current/hourly/daily weather (offline UI testing)
void loadCalendar();            // sample events relative to now - call once the clock is valid

}  // namespace demo_data
