#include "net/open_meteo.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "net/root_certs.h"

namespace open_meteo {
namespace {

constexpr uint32_t HTTP_TIMEOUT_MS = 12000;

String urlEncode(const char* s) {
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    String out;
    for (const char* p = s; *p; ++p) {
        const uint8_t c = static_cast<uint8_t>(*p);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += HEX_DIGITS[c >> 4];
            out += HEX_DIGITS[c & 0x0F];
        }
    }
    return out;
}

// HTTPS GET with certificate verification against the bundled ISRG roots.
bool httpsGet(const String& url, String& body, char* err, size_t errLen) {
    WiFiClientSecure tls;
    tls.setCACert(ROOT_CA_BUNDLE);
    tls.setTimeout(HTTP_TIMEOUT_MS / 1000);
    {   // diagnostics: what does the host resolve to, via which DNS server?
        const int a = url.indexOf("://") + 3, b = url.indexOf('/', a);
        const String host = url.substring(a, b);
        IPAddress ip;
        const bool ok = WiFi.hostByName(host.c_str(), ip);
        Serial.printf("[net] dns %s -> %s (server %s)\n", host.c_str(), ok ? ip.toString().c_str() : "FAILED",
                      WiFi.dnsIP(0).toString().c_str());
    }
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.useHTTP10(true);   // no chunked transfer encoding
    if (!http.begin(tls, url)) {
        snprintf(err, errLen, "bad URL");
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        if (code < 0) snprintf(err, errLen, "%s", HTTPClient::errorToString(code).c_str());
        else snprintf(err, errLen, "HTTP %d", code);
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    return true;
}

}  // namespace

bool geocode(const char* query, const char* countryCode, GeoResult& out, bool& notFound, char* err,
             size_t errLen) {
    notFound = false;
    String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=";
    url += urlEncode(query);
    if (countryCode && countryCode[0]) {
        url += "&countryCode=";
        url += urlEncode(countryCode);
    }
    String body;
    if (!httpsGet(url, body, err, errLen)) return false;

    JsonDocument doc;
    if (const DeserializationError e = deserializeJson(doc, body)) {
        snprintf(err, errLen, "geocode JSON: %s", e.c_str());
        return false;
    }
    JsonObject r = doc["results"][0];
    if (r.isNull()) {
        notFound = true;
        snprintf(err, errLen, "Location not found");
        return false;
    }
    const char* name = r["name"] | "";
    const char* admin = r["admin1"] | "";
    if (admin[0] && strcmp(admin, name) != 0) snprintf(out.name, sizeof(out.name), "%s, %s", name, admin);
    else snprintf(out.name, sizeof(out.name), "%s", name);
    snprintf(out.tz, sizeof(out.tz), "%s", r["timezone"] | "");
    out.lat = r["latitude"] | 0.0f;
    out.lon = r["longitude"] | 0.0f;
    return true;
}

bool forecast(float lat, float lon, Forecast& out, char* err, size_t errLen) {
    char url[420];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code,is_day"
             "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
             "&timezone=auto&timeformat=unixtime&forecast_days=%d&forecast_hours=%d",
             lat, lon, model::DAILY, model::HOURLY);
    String body;
    if (!httpsGet(url, body, err, errLen)) return false;

    JsonDocument doc;
    if (const DeserializationError e = deserializeJson(doc, body)) {
        snprintf(err, errLen, "forecast JSON: %s", e.c_str());
        return false;
    }
    if (doc["error"] | false) {
        snprintf(err, errLen, "%s", doc["reason"] | "API error");
        return false;
    }

    model::Weather& w = out.weather;
    w = model::Weather();
    w.source = model::Source::Live;

    JsonObject cur = doc["current"];
    w.tempC = cur["temperature_2m"] | 0.0f;
    w.code = cur["weather_code"] | 0;
    w.isDay = (cur["is_day"] | 1) == 1;

    JsonObject h = doc["hourly"];
    JsonArray ht = h["time"];
    const int nh = min<int>(ht.size(), model::HOURLY);
    for (int i = 0; i < nh; ++i) {
        model::HourForecast& f = w.hourly[i];
        f.time = ht[i] | 0L;
        f.tempC = h["temperature_2m"][i] | 0.0f;
        f.code = h["weather_code"][i] | 0;
        f.rainChance = h["precipitation_probability"][i] | 0;   // may be null for some models
        f.isDay = (h["is_day"][i] | 1) == 1;
        if (f.rainChance > w.rainChance) w.rainChance = f.rainChance;
    }
    w.hourlyCount = nh;

    JsonObject d = doc["daily"];
    JsonArray dt = d["time"];
    const int nd = min<int>(dt.size(), model::DAILY);
    for (int i = 0; i < nd; ++i) {
        model::DayForecast& f = w.daily[i];
        f.date = dt[i] | 0L;
        f.code = d["weather_code"][i] | 0;
        f.highC = d["temperature_2m_max"][i] | 0.0f;
        f.lowC = d["temperature_2m_min"][i] | 0.0f;
        f.rainChance = d["precipitation_probability_max"][i] | 0;
    }
    w.dailyCount = nd;
    if (nd > 0) {
        w.highC = w.daily[0].highC;
        w.lowC = w.daily[0].lowC;
    }

    out.utcOffsetSeconds = doc["utc_offset_seconds"] | 0;
    snprintf(out.tz, sizeof(out.tz), "%s", doc["timezone"] | "");
    if (nh == 0 || nd == 0) {
        snprintf(err, errLen, "empty forecast");
        return false;
    }
    return true;
}

}  // namespace open_meteo
