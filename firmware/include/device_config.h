#pragma once
// User configuration entered on the setup portal, persisted in NVS ("cfg").
// WiFi credentials are NOT stored here - the ESP32 WiFi driver keeps those.
// Owned by the main loop; the network worker only ever sees copies.

#include <stdint.h>

namespace device_config {

struct Location {
    char  query[48] = "";     // what the user typed: city or postal code
    char  country[4] = "";    // optional ISO 3166 code to disambiguate ("US")
    char  name[48] = "";      // resolved display name ("Austin, Texas")
    char  tz[48] = "";        // IANA timezone from geocoding ("America/Chicago")
    float lat = 0, lon = 0;
    bool  resolved = false;   // lat/lon/tz valid for the current query
};

void load();
Location& location();
bool saveLocation();

}  // namespace device_config
