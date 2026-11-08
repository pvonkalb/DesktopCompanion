#include "demo_data.h"

#include <math.h>
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

void sortByStart(model::Calendar& c) {   // insertion sort: tiny n, no allocation
    for (int i = 1; i < c.count; ++i) {
        const model::Event e = c.events[i];
        int j = i - 1;
        while (j >= 0 && c.events[j].start > e.start) { c.events[j + 1] = c.events[j]; --j; }
        c.events[j + 1] = e;
    }
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

void loadWeather() {
    model::State& st = model::state();

    model::Weather& w = st.weather;
    w.source = model::Source::Demo;
    w.tempC = 12.2f;    // 54 °F
    w.highC = 16.1f;    // 61 °F
    w.lowC = 6.7f;      // 44 °F
    w.code = 2;         // partly cloudy
    w.isDay = true;
    w.rainChance = 40;

    // Hourly: smooth daily temperature curve (low ~5 AM, peak ~3 PM), a rain band.
    const time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    const time_t thisHour = now - lt.tm_min * 60 - lt.tm_sec;
    static const uint8_t CODES[model::HOURLY] = {2, 3, 61, 63, 61, 3, 2, 1, 0, 0, 0, 1};
    static const uint8_t RAIN[model::HOURLY]  = {10, 30, 60, 70, 55, 30, 15, 5, 0, 0, 0, 5};
    for (int i = 0; i < model::HOURLY; ++i) {
        model::HourForecast& h = w.hourly[i];
        h.time = thisHour + i * 3600;
        struct tm ht;
        localtime_r(&h.time, &ht);
        h.tempC = 11.4f + 4.7f * cosf((ht.tm_hour - 15) * 2.0f * 3.14159265f / 24.0f);
        h.code = CODES[i];
        h.rainChance = RAIN[i];
        h.isDay = ht.tm_hour >= 7 && ht.tm_hour < 18;
    }
    w.hourlyCount = model::HOURLY;

    // Daily: a believable week.
    struct DemoDay { uint8_t code, rain; float lo, hi; };
    static const DemoDay DAYS[model::DAILY] = {
        {2, 40, 6.7f, 16.1f}, {61, 80, 5.0f, 12.8f}, {0, 0, 3.9f, 17.2f}, {1, 10, 6.1f, 18.3f},
        {3, 20, 8.3f, 15.6f}, {63, 90, 7.2f, 11.7f}, {2, 20, 5.0f, 14.4f},
    };
    for (int i = 0; i < model::DAILY; ++i) {
        model::DayForecast& d = w.daily[i];
        struct tm dt = lt;
        dt.tm_mday += i;
        dt.tm_hour = dt.tm_min = dt.tm_sec = 0;
        dt.tm_isdst = -1;
        d.date = mktime(&dt);
        d.code = DAYS[i].code;
        d.rainChance = DAYS[i].rain;
        d.lowC = DAYS[i].lo;
        d.highC = DAYS[i].hi;
    }
    w.dailyCount = model::DAILY;
    ++w.version;

}

void loadCalendar() {
    model::Calendar& c = model::state().calendar;
    const time_t now = time(nullptr);
    c.source = model::Source::Demo;
    c.count = 0;
    // calendar 0 = yours (cyan), calendar 1 = shared from your wife (violet)
    add(c, "Coffee with Jordan", now - 3 * 3600, 60, false, 0);           // already over today
    add(c, "Dinner with Sam's parents", now + (2 * 60 + 15) * 60, 120, false, 0);
    add(c, "Farmers market", at(1, 9, 0), 120, false, 1);
    add(c, "Haircut", at(1, 14, 30), 45, false, 1);
    add(c, "Pay rent", at(2, 0, 0), 24 * 60, true, 0);
    add(c, "Yoga", at(3, 18, 0), 60, false, 1);
    add(c, "Dentist", at(4, 10, 30), 60, false, 0);
    add(c, "Brunch with Alex", at(6, 11, 0), 90, false, 1);
    add(c, "Call Mom", at(6, 16, 0), 30, false, 0);
    add(c, "Meal prep", at(6, 18, 0), 60, false, 1);
    add(c, "Movie night", at(6, 20, 0), 120, false, 0);
    add(c, "Weekend trip to the coast", at(9, 0, 0), 3 * 24 * 60, true, 1);   // multi-day
    sortByStart(c);
    ++c.version;
}

}  // namespace demo_data
