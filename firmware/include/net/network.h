#pragma once
// WiFi + setup portal + background data fetching.
//
// Threading: begin()/loop()/startPortal()/stopPortal() and all getters run on the
// main loop (core 1). Blocking HTTPS work runs in a worker task on core 0; it
// receives copies of the job and hands results back through a mutex, which
// loop() applies to device_config and model::state(). The UI never blocks.

#include <stddef.h>
#include <stdint.h>

namespace net {

enum class Link : uint8_t {
    NotConfigured,   // no WiFi credentials saved yet
    Connecting,      // credentials saved, not connected since boot
    Online,
    Offline,         // was online, connection lost (auto-reconnecting)
};

enum class LocationState : uint8_t { Missing, Resolving, Ok, NotFound };

void begin();
void loop(uint32_t nowMs);

// Setup portal (phone-based WiFi + location entry)
void startPortal(bool autoOpened = false);
void stopPortal();
bool portalActive();
bool portalAutoOpened();
const char* portalSsid();
const char* portalPassword();
bool portalSubmitted();          // user saved the form during this portal session
bool restoredPreviousWifi();     // new Wi-Fi failed and the previous network was restored

Link link();
const char* ssid();              // connected / saved network name
int rssi();
LocationState locationState();
const char* locationName();      // resolved name or what the user typed
bool timeSynced();
uint32_t weatherAgeMs();         // UINT32_MAX if never fetched
const char* lastError();
void requestWeatherRefresh();

// One-line status for Home; returns false when everything is fine.
bool statusLine(char* out, size_t len, uint16_t& colour);

}  // namespace net
