#pragma once
// Central application data. Data sources (demo, WiFi fetchers, PC link) write
// here and bump the matching version counter; screens compare versions to
// decide what to redraw. Single-threaded: only touched from loop().

#include <stdint.h>
#include <time.h>

namespace model {

enum class Source : uint8_t { None, Demo, Live };

struct HourForecast {
    time_t  time;                 // start of the hour
    float   tempC;
    uint8_t code;                 // WMO weather code
    uint8_t rainChance;           // %
    bool    isDay;
};

struct DayForecast {
    time_t  date;                 // local midnight
    float   lowC, highC;
    uint8_t code;
    uint8_t rainChance;           // max for the day, %
};

constexpr int HOURLY = 12;
constexpr int DAILY  = 7;

struct Weather {
    Source   source = Source::None;
    float    tempC = 0, highC = 0, lowC = 0;
    uint8_t  code = 0;            // WMO weather code (Open-Meteo "weather_code")
    bool     isDay = true;
    uint8_t  rainChance = 0;      // max precipitation probability, next 12 h (%)
    HourForecast hourly[HOURLY];  // hourly[0] = current hour
    uint8_t  hourlyCount = 0;
    DayForecast daily[DAILY];     // daily[0] = today
    uint8_t  dailyCount = 0;
    uint32_t version = 0;
};

struct Event {
    char    title[48];
    time_t  start;
    time_t  end;
    bool    allDay;
    uint8_t calendar;             // index into the calendar colour palette
};

constexpr int MAX_EVENTS = 24;

struct Calendar {
    Source   source = Source::None;
    uint8_t  count = 0;
    Event    events[MAX_EVENTS];  // sorted by start time
    time_t   receivedAt = 0;      // wall-clock time the snapshot arrived (0 = unknown)
    uint32_t version = 0;
};

void sortCalendar(Calendar& c);   // by start time (stable, no allocation)

// Now playing on the PC (from the companion). state: 0 none, 1 playing, 2 paused.
struct Media {
    bool     valid = false;
    char     app[24] = "";
    char     title[64] = "";
    char     artist[48] = "";
    uint8_t  state = 0;
    int8_t   volume = -1;         // 0..100, -1 unknown
    bool     muted = false;
    uint32_t version = 0;
};

// PC load (from the companion). -1 = not available.
struct PcStats {
    bool     valid = false;
    int8_t   cpu = -1, ram = -1, gpu = -1, gpuTemp = -1;
    float    ramUsedGb = 0;
    uint16_t ramTotalGb = 0;
    uint32_t downBps = 0, upBps = 0;
    uint32_t version = 0;
};

struct State {
    Weather  weather;
    Calendar calendar;
    Media    media;
    PcStats  pc;
};

State& state();

}  // namespace model
