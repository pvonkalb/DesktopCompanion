#include "demo_data.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "model.h"

namespace demo_data {
namespace {

time_t at(int dayOffset, int hour, int minute) {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_mday += dayOffset;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    return mktime(&t);
}

void add(model::Calendar& c, const char* title, time_t start, int durationMin, bool allDay, uint8_t cal) {
    if (c.count >= model::MAX_EVENTS) return;
    model::Event& e = c.events[c.count++];
    strncpy(e.title, title, sizeof(e.title) - 1);
    e.title[sizeof(e.title) - 1] = '\0';
    e.start = start;
    e.end = start + durationMin * 60;
    e.allDay = allDay;
    e.calendar = cal;
}

}  // namespace

void setClockFromBuildTime() {
    // __DATE__ = "Nov  7 2026", __TIME__ = "19:20:33" (compiler's local time)
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int day = 1, year = 2026, h = 0, m = 0, s = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &h, &m, &s);
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = static_cast<int>((strstr(months, mon) - months) / 3);
    t.tm_mday = day;
    t.tm_hour = h;
    t.tm_min = m;
    t.tm_sec = s;
    t.tm_isdst = -1;
    const timeval tv = {mktime(&t), 0};
    settimeofday(&tv, nullptr);
}

void load() {
    model::State& st = model::state();

    model::Weather& w = st.weather;
    w.source = model::Source::Demo;
    w.tempC = 12.2f;    // 54 °F
    w.highC = 16.1f;    // 61 °F
    w.lowC = 6.7f;      // 44 °F
    w.code = 2;         // partly cloudy
    w.isDay = true;
    w.rainChance = 40;
    ++w.version;

    model::Calendar& c = st.calendar;
    c.source = model::Source::Demo;
    c.count = 0;
    const time_t now = time(nullptr);
    add(c, "Dinner with Sam's parents", now + (2 * 60 + 15) * 60, 120, false, 0);
    add(c, "Farmers market", at(1, 9, 0), 120, false, 1);
    add(c, "Pay rent", at(2, 0, 0), 24 * 60, true, 0);
    add(c, "Yoga", at(3, 18, 0), 60, false, 1);
    ++c.version;
}

}  // namespace demo_data
