#include "timefmt.h"

#include <stdio.h>
#include <string.h>

#include "settings.h"

namespace timefmt {
namespace {

const char* const WEEKDAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                "Thursday", "Friday", "Saturday"};
const char* const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

constexpr time_t MIN_VALID = 1735689600;   // 2025-01-01: anything earlier means "not set"

time_t localMidnight(time_t t) {
    struct tm lt;
    localtime_r(&t, &lt);
    lt.tm_hour = 12;   // noon: immune to DST shifts, rounding below absorbs the offset
    lt.tm_min = lt.tm_sec = 0;
    lt.tm_isdst = -1;
    return mktime(&lt);
}

}  // namespace

bool valid() { return time(nullptr) >= MIN_VALID; }

bool local(struct tm& out) { return local(time(nullptr), out); }

bool local(time_t t, struct tm& out) {
    if (t < MIN_VALID) return false;
    localtime_r(&t, &out);
    return true;
}

void clock(const struct tm& t, char* hm, size_t hmLen, char* ampm, size_t ampmLen) {
    if (settings::USE_24H) {
        snprintf(hm, hmLen, "%02d:%02d", t.tm_hour, t.tm_min);
        if (ampmLen) ampm[0] = '\0';
        return;
    }
    const int h = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
    snprintf(hm, hmLen, "%d:%02d", h, t.tm_min);
    snprintf(ampm, ampmLen, "%s", t.tm_hour < 12 ? "AM" : "PM");
}

void date(const struct tm& t, char* out, size_t len) {
    snprintf(out, len, "%s, %s %d", WEEKDAYS[t.tm_wday], MONTHS[t.tm_mon], t.tm_mday);
}

void timeOfDay(time_t t, char* out, size_t len) {
    struct tm lt;
    localtime_r(&t, &lt);
    char hm[8], ampm[4];
    clock(lt, hm, sizeof(hm), ampm, sizeof(ampm));
    if (ampm[0]) snprintf(out, len, "%s %s", hm, ampm);
    else snprintf(out, len, "%s", hm);
}

int dayDiff(time_t from, time_t to) {
    const double secs = difftime(localMidnight(to), localMidnight(from));
    return static_cast<int>(secs >= 0 ? (secs + 43200) / 86400 : (secs - 43200) / 86400);
}

time_t dayStart(time_t t, int addDays) {
    struct tm lt;
    localtime_r(&t, &lt);
    lt.tm_mday += addDays;
    lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
    lt.tm_isdst = -1;   // let mktime work out DST for the target day
    return mktime(&lt);
}

time_t weekStart(time_t t) {
    struct tm lt;
    localtime_r(&t, &lt);
    const int first = settings::WEEK_STARTS_MONDAY ? 1 : 0;
    return dayStart(t, -((lt.tm_wday - first + 7) % 7));
}

const char* weekdayShort(int wday) {
    static const char* const SHORT[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    return SHORT[((wday % 7) + 7) % 7];
}

void monthYear(time_t t, char* out, size_t len) {
    static const char* const LONG[] = {"January", "February", "March", "April", "May", "June", "July",
                                       "August", "September", "October", "November", "December"};
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(out, len, "%s %d", LONG[lt.tm_mon], lt.tm_year + 1900);
}

void dayHeading(time_t t, char* out, size_t len) {
    static const char* const UPPER[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
                                        "THURSDAY", "FRIDAY", "SATURDAY"};
    static const char* const MON[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    struct tm lt;
    localtime_r(&t, &lt);
    snprintf(out, len, "%s, %s %d", UPPER[lt.tm_wday], MON[lt.tm_mon], lt.tm_mday);
}

void when(time_t start, time_t end, bool allDay, time_t now, char* out, size_t len) {
    if (start <= now && now < end) {
        if (allDay) { snprintf(out, len, "Today"); return; }
        char t[12];
        timeOfDay(end, t, sizeof(t));
        snprintf(out, len, "Now \xC2\xB7 until %s", t);   // "·" in UTF-8
        return;
    }
    const int days = dayDiff(now, start);
    if (days == 0) {
        if (allDay) { snprintf(out, len, "Today"); return; }
        const long mins = static_cast<long>((start - now + 59) / 60);
        if (mins < 60) snprintf(out, len, "Today \xC2\xB7 in %ld min", mins);
        else snprintf(out, len, "Today \xC2\xB7 in %ldh %02ldm", mins / 60, mins % 60);
        return;
    }
    if (days == 1) { snprintf(out, len, "Tomorrow"); return; }
    struct tm lt;
    localtime_r(&start, &lt);
    if (days < 7) snprintf(out, len, "%s", WEEKDAYS[lt.tm_wday]);
    else snprintf(out, len, "%s %d", MONTHS[lt.tm_mon], lt.tm_mday);
}

}  // namespace timefmt
