#pragma once
// Wireless firmware updates (ArduinoOTA, password-protected, local network only).
// Upload with:  pio run -e cyd_ota -t upload   (password from secrets.ini)
// During an update the main loop is busy receiving; a progress screen is shown.

#include <TFT_eSPI.h>

namespace ota {

void loop(TFT_eSPI& tft);   // starts the listener once WiFi is up, then services it

}  // namespace ota
