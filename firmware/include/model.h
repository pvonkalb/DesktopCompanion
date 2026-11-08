#pragma once
// Central application data. Data sources (demo, WiFi fetchers, PC link) write
// here and bump the matching version counter; screens compare versions to
// decide what to redraw. Single-threaded: only touched from loop().

#include <stdint.h>
#include <time.h>

namespace model {

enum class Source : uint8_t { None, Demo, Live };

struct Weather {
    Source   source = Source::None;
    float    tempC = 0, highC = 0, lowC = 0;
    uint8_t  code = 0;            // WMO weather code (Open-Meteo "weather_code")
    bool     isDay = true;
    uint8_t  rainChance = 0;      // max precipitation probability, next 12 h (%)
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
    uint32_t version = 0;
};

struct State {
    Weather  weather;
    Calendar calendar;
};

State& state();

}  // namespace model
