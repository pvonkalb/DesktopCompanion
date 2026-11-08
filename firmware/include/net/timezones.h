#pragma once
// IANA timezone name -> POSIX TZ string (what the ESP32 C library understands,
// including daylight-saving rules).

#include <stddef.h>
#include <stdint.h>

namespace timezones {

// True if the zone is in the built-in table (DST rules exact).
bool posixFor(const char* iana, char* out, size_t len);

// Fixed-offset fallback from Open-Meteo's utc_offset_seconds (east = positive).
// DST is then picked up within one weather refresh of the change.
void posixFromOffset(int32_t utcOffsetSeconds, char* out, size_t len);

}  // namespace timezones
