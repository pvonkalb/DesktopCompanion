#pragma once
// Open-Meteo HTTPS client (no API key). Blocking calls - run them only from the
// network worker task, never from the UI loop.

#include <stddef.h>
#include <stdint.h>

#include "model.h"

namespace open_meteo {

struct GeoResult {
    char  name[48];   // "Denver, Colorado"
    char  tz[48];     // "America/Denver"
    float lat, lon;
};

struct Forecast {
    model::Weather weather;      // source = Live, version untouched
    int32_t utcOffsetSeconds;
    char    tz[48];
};

// notFound = the service answered but had no match (final); otherwise a failure is
// a network/transient error worth retrying.
bool geocode(const char* query, const char* countryCode, GeoResult& out, bool& notFound, char* err,
             size_t errLen);
bool forecast(float lat, float lon, Forecast& out, char* err, size_t errLen);

}  // namespace open_meteo
