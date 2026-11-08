#include "net/network.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <lwip/dns.h>
#include <time.h>

#include "device_config.h"
#include "model.h"
#include "net/open_meteo.h"
#include "net/timezones.h"
#include "settings.h"
#include "theme.h"

namespace net {
namespace {

// ---------------------------------------------------------------- main-loop state
WiFiManager wm;
WiFiManagerParameter* paramLocation = nullptr;
WiFiManagerParameter* paramCountry = nullptr;

char apSsid[24] = "";
char apPass[12] = "";
bool portal = false;
bool portalAuto = false;
bool portalSubmittedFlag = false;
bool everConnected = false;
bool wasConnected = false;
uint32_t connectStartMs = 0;

char appliedTz[64] = "";          // POSIX string currently in effect
bool sntpStarted = false;
uint32_t lastWeatherMs = 0;
bool haveWeather = false;
char lastErr[64] = "";
LocationState locState = LocationState::Missing;
bool closePortalWhenOnline = false;   // location re-saved unchanged: nothing left to do
bool wifiSubmitted = false;           // Wi-Fi form saved in this portal session
uint32_t wifiSubmittedMs = 0;
wifi_config_t previousWifi{};         // last working credentials, restored if new ones fail
bool havePreviousWifi = false;
bool restoredWifi = false;

// ---------------------------------------------------------------- shared with worker
SemaphoreHandle_t lock = nullptr;

struct Job {
    uint32_t geoSeq = 0;          // bump to request geocoding of query/country
    char     query[48] = "";
    char     country[4] = "";
    bool     haveCoords = false;
    float    lat = 0, lon = 0;
    bool     refresh = false;     // force a weather fetch
} job;

struct Results {
    bool     geoReady = false;
    bool     geoOk = false;
    uint32_t geoSeq = 0;
    open_meteo::GeoResult geo{};
    bool     wxReady = false;
    open_meteo::Forecast fc{};
    bool     errReady = false;
    char     err[64] = "";
} results;

struct Guard {
    Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
    ~Guard() { xSemaphoreGive(lock); }
};

// ---------------------------------------------------------------- worker task (core 0)
// While the setup access point is up, the ESP32 can end up resolving names through
// the AP side (where the captive portal answers everything with 192.168.4.1).
// Force the router's DNS (captured when the station got its address) + a public one.
volatile uint32_t routerDns = 0;

void useRouterDns() {
    if (!(WiFi.getMode() & WIFI_MODE_AP)) return;
    const IPAddress want = routerDns ? IPAddress(routerDns) : IPAddress(1, 1, 1, 1);
    if (WiFi.dnsIP(0) == want) return;
    Serial.printf("[net] dns server %s -> %s (setup AP active)\n", WiFi.dnsIP(0).toString().c_str(),
                  want.toString().c_str());
    ip_addr_t a = IPADDR4_INIT(static_cast<uint32_t>(want));
    ip_addr_t b = IPADDR4_INIT(static_cast<uint32_t>(IPAddress(1, 1, 1, 1)));
    dns_setserver(0, &a);
    dns_setserver(1, &b);
}

void worker(void*) {
    uint32_t doneGeoSeq = 0, seenGeoSeq = 0;
    uint32_t nextGeoMs = 0, geoBackoffMs = 15000;
    uint32_t nextWeatherMs = 0;
    uint32_t backoffMs = 60000;
    float lastLat = NAN, lastLon = NAN;
    static open_meteo::Forecast fc;   // static: keeps ~0.5 KB off the task stack

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (WiFi.status() != WL_CONNECTED) continue;

        Job j;
        {
            Guard g;
            j = job;
            job.refresh = false;
        }

        useRouterDns();

        if (j.geoSeq != seenGeoSeq) {   // new query: try immediately
            seenGeoSeq = j.geoSeq;
            nextGeoMs = millis();
            geoBackoffMs = 15000;
        }
        if (j.geoSeq != doneGeoSeq && j.query[0] && static_cast<int32_t>(millis() - nextGeoMs) >= 0) {
            open_meteo::GeoResult geo{};
            char err[64] = "";
            bool notFound = false;
            const bool ok = open_meteo::geocode(j.query, j.country, geo, notFound, err, sizeof(err));
            Serial.printf("[net] geocode '%s' %s: %s\n", j.query, ok ? "ok" : (notFound ? "NOT FOUND" : "FAILED"),
                          ok ? geo.name : err);
            Guard g;
            if (ok || notFound) {   // definitive answer
                results.geoReady = true;
                results.geoOk = ok;
                results.geoSeq = j.geoSeq;
                results.geo = geo;
                doneGeoSeq = j.geoSeq;
            } else {                // network trouble: retry with backoff
                nextGeoMs = millis() + geoBackoffMs;
                geoBackoffMs = min<uint32_t>(geoBackoffMs * 2, 5UL * 60 * 1000);
            }
            if (!ok) { snprintf(results.err, sizeof(results.err), "%s", err); results.errReady = true; }
            continue;
        }

        if (!j.haveCoords) continue;
        const uint32_t now = millis();
        const bool moved = j.lat != lastLat || j.lon != lastLon;
        if (!moved && !j.refresh && static_cast<int32_t>(now - nextWeatherMs) < 0) continue;
        {
            Guard g;
            if (results.wxReady) continue;   // previous result not consumed yet
        }

        char err[64] = "";
        const uint32_t t0 = millis();
        const bool ok = open_meteo::forecast(j.lat, j.lon, fc, err, sizeof(err));
        Serial.printf("[net] forecast %s in %lu ms (heap %lu, largest %lu)%s%s\n", ok ? "ok" : "FAILED",
                      static_cast<unsigned long>(millis() - t0), static_cast<unsigned long>(ESP.getFreeHeap()),
                      static_cast<unsigned long>(ESP.getMaxAllocHeap()), ok ? "" : ": ", ok ? "" : err);
        {
            Guard g;
            if (ok) {
                results.fc = fc;
                results.wxReady = true;
            } else {
                snprintf(results.err, sizeof(results.err), "Weather: %s", err);
                results.errReady = true;
            }
        }
        lastLat = j.lat;
        lastLon = j.lon;
        if (ok) {
            nextWeatherMs = millis() + settings::WEATHER_REFRESH_MS;
            backoffMs = 60000;
        } else {
            nextWeatherMs = millis() + backoffMs;
            backoffMs = min<uint32_t>(backoffMs * 2, settings::WEATHER_REFRESH_MS);
        }
    }
}

// ---------------------------------------------------------------- helpers (main loop)
void postLocationJob() {
    const device_config::Location& loc = device_config::location();
    Guard g;
    if (loc.resolved) {
        job.haveCoords = true;
        job.lat = loc.lat;
        job.lon = loc.lon;
    }
    if (!loc.resolved && loc.query[0]) {
        snprintf(job.query, sizeof(job.query), "%s", loc.query);
        snprintf(job.country, sizeof(job.country), "%s", loc.country);
        ++job.geoSeq;
    }
    locState = loc.resolved ? LocationState::Ok : (loc.query[0] ? LocationState::Resolving : LocationState::Missing);
}

void applyTimezone(const char* iana, int32_t fallbackOffset, bool haveOffset) {
    char posix[64];
    if (!timezones::posixFor(iana, posix, sizeof(posix))) {
        if (!haveOffset) return;   // wait for the first forecast to supply the offset
        timezones::posixFromOffset(fallbackOffset, posix, sizeof(posix));
    }
    if (strcmp(posix, appliedTz) == 0 && sntpStarted) return;
    snprintf(appliedTz, sizeof(appliedTz), "%s", posix);
    configTzTime(appliedTz, settings::NTP_SERVER_1, settings::NTP_SERVER_2);   // non-blocking; SNTP runs in background
    sntpStarted = true;
    Serial.printf("[net] timezone %s -> %s\n", iana && iana[0] ? iana : "(offset)", appliedTz);
}

void readPortalParams() {
    device_config::Location& loc = device_config::location();
    const char* q = paramLocation->getValue();
    const char* c = paramCountry->getValue();
    char country[4] = "";
    for (int i = 0; i < 3 && c[i]; ++i) country[i] = static_cast<char>(toupper(c[i]));
    portalSubmittedFlag = true;
    if (strcmp(q, loc.query) == 0 && strcmp(country, loc.country) == 0 && loc.resolved) {   // unchanged
        closePortalWhenOnline = true;
        return;
    }
    snprintf(loc.query, sizeof(loc.query), "%s", q);
    snprintf(loc.country, sizeof(loc.country), "%s", country);
    loc.resolved = false;
    loc.name[0] = '\0';
    device_config::saveLocation();
    Serial.printf("[net] location set to '%s' %s\n", loc.query, loc.country);
    postLocationJob();
}

void onWifiSaved() {   // WiFiManager has just stored (and is trying) new credentials
    wifiSubmitted = true;
    wifiSubmittedMs = millis();
    readPortalParams();
    closePortalWhenOnline = false;   // let the connection attempt decide
}

void restorePreviousWifi() {
    Serial.printf("[net] new Wi-Fi failed - restoring '%s'\n", reinterpret_cast<const char*>(previousWifi.sta.ssid));
    esp_wifi_set_config(WIFI_IF_STA, &previousWifi);   // persisted by the WiFi driver
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    restoredWifi = true;
    wifiSubmitted = false;
    connectStartMs = millis();
}

void makePortalCredentials() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apSsid, sizeof(apSsid), "DeskCompanion-%02X%02X", mac[4], mac[5]);
    static const char ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";   // no l/1/o/0 confusion
    for (int i = 0; i < 8; ++i) apPass[i] = ALPHABET[esp_random() % (sizeof(ALPHABET) - 1)];
    apPass[8] = '\0';
}

}  // namespace

// ---------------------------------------------------------------- public API
void begin() {
    lock = xSemaphoreCreateMutex();
    device_config::load();

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(settings::HOSTNAME);
    WiFi.setAutoReconnect(true);

    const device_config::Location& loc = device_config::location();
    // Heading so the location fields aren't missed below the Wi-Fi password.
    static WiFiManagerParameter heading(
        "<hr><h3 style='margin:12px 0 4px'>Your location</h3>"
        "<p style='margin:0 0 8px;opacity:.75'>Needed for weather and the clock's time zone. "
        "After you tap Save, the device screen confirms.</p>");
    paramLocation = new WiFiManagerParameter("location", "City or ZIP code", loc.query, sizeof(loc.query) - 1);
    paramCountry = new WiFiManagerParameter("country", "Country code, optional (e.g. US)", loc.country, 2);
    wm.addParameter(&heading);
    wm.addParameter(paramLocation);
    wm.addParameter(paramCountry);
    wm.setCustomMenuHTML("<form action='/param' method='get'><button>Set location</button></form><br/>\n");
    wm.setConfigPortalBlocking(false);
    wm.setBreakAfterConfig(true);          // leave the portal after Save even if the connect fails
    wm.setConnectTimeout(20);
    wm.setTitle("Desk Companion");
    wm.setDarkMode(true);
    wm.setHostname(settings::HOSTNAME);
    wm.setShowInfoUpdate(false);
    wm.setShowInfoErase(false);
    wm.setRemoveDuplicateAPs(true);
    wm.setDebugOutput(settings::WIFIMANAGER_LOG);
    // Scanning takes the radio off the AP channel for seconds, and phones then drop
    // the setup network (seen on hardware). Scan once when the portal opens - before
    // a phone has joined - and serve that cached list instead of rescanning per page.
    wm._preloadwifiscan = true;
    wm._asyncScan = true;
    wm._scancachetime = 10UL * 60 * 1000;
    // The ESP32's default AP address (192.168.4.1) collides with home routers on
    // 192.168.4.0/24 (seen on the dev network): traffic for the router went to the
    // AP itself while setup was open. Use a range home routers rarely hand out.
    wm.setAPStaticIPConfig(IPAddress(10, 42, 0, 1), IPAddress(10, 42, 0, 1), IPAddress(255, 255, 255, 0));
    wm.setSaveConfigCallback(onWifiSaved);
    wm.setSaveParamsCallback(readPortalParams);

    postLocationJob();
    if (loc.resolved) applyTimezone(loc.tz, 0, false);   // known zone: clock is right as soon as NTP answers

    // Priority 0 (= idle): the TLS handshake is several seconds of crypto; at a higher
    // priority it starved IDLE0 and the task watchdog rebooted the device.
    xTaskCreatePinnedToCore(worker, "net", 10240, nullptr, tskIDLE_PRIORITY, nullptr, 0);

    connectStartMs = millis();
    if (wm.getWiFiIsSaved()) {
        Serial.printf("[net] connecting to saved network '%s'\n", wm.getWiFiSSID(true).c_str());
        WiFi.begin();
    } else {
        Serial.println(F("[net] no WiFi saved - opening setup portal"));
        startPortal(true);
    }
}

void startPortal(bool autoOpened) {
    if (portal) return;
    makePortalCredentials();
    const device_config::Location& loc = device_config::location();
    paramLocation->setValue(loc.query, sizeof(loc.query) - 1);
    paramCountry->setValue(loc.country, 2);
    // Already online: location is the likely job, so offer it first (Wi-Fi change still available).
    // Offline: one page with Wi-Fi + location fields.
    static const char* MENU_ONLINE[] = {"custom", "wifi"};
    static const char* MENU_OFFLINE[] = {"wifi"};
    if (WiFi.status() == WL_CONNECTED) wm.setMenu(MENU_ONLINE, 2);
    else wm.setMenu(MENU_OFFLINE, 1);
    // Auto-opened portals time out so a slow router after a power cut can still be joined.
    wm.setConfigPortalTimeout(autoOpened && wm.getWiFiIsSaved() ? settings::PORTAL_TIMEOUT_S : 0);
    wm.startConfigPortal(apSsid, apPass);
    portal = true;
    portalAuto = autoOpened;
    portalSubmittedFlag = false;
    closePortalWhenOnline = false;
    wifiSubmitted = false;
    restoredWifi = false;
    havePreviousWifi = everConnected && esp_wifi_get_config(WIFI_IF_STA, &previousWifi) == ESP_OK &&
                       previousWifi.sta.ssid[0] != 0;
    Serial.printf("[net] setup portal '%s' (password %s) at %s\n", apSsid, apPass,
                  WiFi.softAPIP().toString().c_str());
}

void stopPortal() {
    if (!portal) return;
    wm.stopConfigPortal();
    portal = false;
    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) WiFi.begin();
    connectStartMs = millis();
    Serial.println(F("[net] setup portal closed"));
}

void loop(uint32_t now) {
    if (portal) {
        wm.process();
        if (!wm.getConfigPortalActive()) {   // saved+connected, timed out, or broke after config
            portal = false;
            Serial.printf("[net] portal ended (%s)\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
            if (WiFi.status() != WL_CONNECTED) {
                WiFi.mode(WIFI_STA);
                WiFi.begin();
                connectStartMs = now;
            }
        }
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    if (portal && closePortalWhenOnline && connected) {
        closePortalWhenOnline = false;
        stopPortal();
    }
    // New credentials that don't connect within 30 s: put the old, working ones back.
    if (wifiSubmitted && !connected && havePreviousWifi && now - wifiSubmittedMs > 30000) {
        if (portal) stopPortal();
        restorePreviousWifi();
    }
    if (wifiSubmitted && connected) wifiSubmitted = false;
    if (connected != wasConnected) {
        wasConnected = connected;
        if (connected) {
            everConnected = true;
            Serial.printf("[net] online: '%s' %s rssi %d dns %s\n", WiFi.SSID().c_str(),
                          WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.dnsIP(0).toString().c_str());
            if (WiFi.dnsIP(0) != WiFi.softAPIP() && WiFi.dnsIP(0) != IPAddress(0, 0, 0, 0))
                routerDns = static_cast<uint32_t>(WiFi.dnsIP(0));
            if (portal && (portalSubmittedFlag || portalAuto)) stopPortal();   // auto portal: router is back
            const device_config::Location& loc = device_config::location();
            if (loc.resolved) applyTimezone(loc.tz, 0, false);
        } else {
            Serial.println(F("[net] offline"));
            connectStartMs = now;
        }
    }

    // Never connected since boot (moved house, new router, wrong password): offer setup.
    if (!connected && !portal && !everConnected && now - connectStartMs > settings::FIRST_CONNECT_MS) {
        Serial.println(F("[net] could not connect - opening setup portal"));
        startPortal(true);
    }

    // ---- apply worker results
    Guard g;
    if (results.geoReady) {
        results.geoReady = false;
        device_config::Location& loc = device_config::location();
        if (results.geoSeq == job.geoSeq) {   // ignore stale answers to an older query
            if (results.geoOk) {
                snprintf(loc.name, sizeof(loc.name), "%s", results.geo.name);
                snprintf(loc.tz, sizeof(loc.tz), "%s", results.geo.tz);
                loc.lat = results.geo.lat;
                loc.lon = results.geo.lon;
                loc.resolved = true;
                device_config::saveLocation();
                job.haveCoords = true;
                job.lat = loc.lat;
                job.lon = loc.lon;
                job.refresh = true;
                locState = LocationState::Ok;
                lastErr[0] = '\0';
                applyTimezone(loc.tz, 0, false);
                // Location entered while online: the job is done, close the setup network.
                if (portal && portalSubmittedFlag && WiFi.status() == WL_CONNECTED) stopPortal();
            } else {
                locState = LocationState::NotFound;
            }
        }
    }
    if (results.wxReady) {
        results.wxReady = false;
        model::Weather& w = model::state().weather;
        const uint32_t version = w.version;
        w = results.fc.weather;
        w.version = version + 1;
        lastWeatherMs = now;
        haveWeather = true;
        if (lastErr[0] && strncmp(lastErr, "Weather", 7) == 0) lastErr[0] = '\0';
        applyTimezone(results.fc.tz, results.fc.utcOffsetSeconds, true);
    }
    if (results.errReady) {
        results.errReady = false;
        snprintf(lastErr, sizeof(lastErr), "%s", results.err);
    }
}

bool portalActive() { return portal; }
bool portalAutoOpened() { return portalAuto; }
const char* portalSsid() { return apSsid; }
const char* portalPassword() { return apPass; }
bool portalSubmitted() { return portalSubmittedFlag; }
bool restoredPreviousWifi() { return restoredWifi; }

Link link() {
    if (WiFi.status() == WL_CONNECTED) return Link::Online;
    if (everConnected) return Link::Offline;
    return wm.getWiFiIsSaved() ? Link::Connecting : Link::NotConfigured;
}

const char* ssid() {
    static String s;
    s = WiFi.SSID();
    if (s.isEmpty()) s = wm.getWiFiSSID(true);
    return s.c_str();
}

int rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }
LocationState locationState() { return locState; }

const char* locationName() {
    const device_config::Location& loc = device_config::location();
    return loc.name[0] ? loc.name : loc.query;
}

bool timeSynced() { return sntpStarted && time(nullptr) > 1735689600; }   // clock only moves past 2025 via NTP

uint32_t weatherAgeMs() { return haveWeather ? millis() - lastWeatherMs : UINT32_MAX; }
const char* lastError() { return lastErr; }

void requestWeatherRefresh() {
    Guard g;
    job.refresh = true;
}

bool statusLine(char* out, size_t len, uint16_t& colour) {
    colour = theme::STATUS_WARN;
    if (portal) { snprintf(out, len, "Wi-Fi setup open"); colour = theme::ACCENT; return true; }
    switch (link()) {
        case Link::NotConfigured: snprintf(out, len, "Wi-Fi not set up"); return true;
        case Link::Connecting: snprintf(out, len, "Connecting to Wi-Fi"); colour = theme::TEXT_MUTED; return true;
        case Link::Offline: snprintf(out, len, "Offline"); return true;
        case Link::Online: break;
    }
    if (locState == LocationState::Missing) { snprintf(out, len, "Set location in Settings"); return true; }
    if (locState == LocationState::NotFound) { snprintf(out, len, "Location not found"); return true; }
    const uint32_t age = weatherAgeMs();
    if (haveWeather && age > settings::WEATHER_STALE_MS) {
        snprintf(out, len, "Weather %luh old", static_cast<unsigned long>(age / 3600000UL));
        colour = theme::TEXT_MUTED;
        return true;
    }
    return false;
}

}  // namespace net
