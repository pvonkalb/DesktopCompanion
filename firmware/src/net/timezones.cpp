#include "net/timezones.h"

#include <stdio.h>
#include <string.h>

namespace timezones {
namespace {

struct Zone {
    const char* iana;
    const char* posix;
};

constexpr const char* US_EAST    = "EST5EDT,M3.2.0,M11.1.0";
constexpr const char* US_CENTRAL = "CST6CDT,M3.2.0,M11.1.0";
constexpr const char* US_MOUNTAIN = "MST7MDT,M3.2.0,M11.1.0";
constexpr const char* US_PACIFIC = "PST8PDT,M3.2.0,M11.1.0";
constexpr const char* EU_CENTRAL = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* EU_EAST    = "EET-2EEST,M3.5.0/3,M10.5.0/4";
constexpr const char* AU_EAST    = "AEST-10AEDT,M10.1.0,M4.1.0/3";

const Zone ZONES[] = {
    // North America
    {"America/New_York", US_EAST}, {"America/Detroit", US_EAST}, {"America/Toronto", US_EAST},
    {"America/Indiana/Indianapolis", US_EAST}, {"America/Kentucky/Louisville", US_EAST},
    {"America/Chicago", US_CENTRAL}, {"America/Winnipeg", US_CENTRAL},
    {"America/Indiana/Knox", US_CENTRAL}, {"America/Menominee", US_CENTRAL},
    {"America/Denver", US_MOUNTAIN}, {"America/Boise", US_MOUNTAIN}, {"America/Edmonton", US_MOUNTAIN},
    {"America/Phoenix", "MST7"},
    {"America/Los_Angeles", US_PACIFIC}, {"America/Vancouver", US_PACIFIC},
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"Pacific/Honolulu", "HST10"},
    {"America/Halifax", "AST4ADT,M3.2.0,M11.1.0"},
    {"America/St_Johns", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"America/Regina", "CST6"},
    {"America/Puerto_Rico", "AST4"},
    {"America/Mexico_City", "CST6"},
    // Europe
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Dublin", "IST-1GMT0,M10.5.0,M3.5.0/1"},
    {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
    {"Europe/Paris", EU_CENTRAL}, {"Europe/Berlin", EU_CENTRAL}, {"Europe/Madrid", EU_CENTRAL},
    {"Europe/Rome", EU_CENTRAL}, {"Europe/Amsterdam", EU_CENTRAL}, {"Europe/Brussels", EU_CENTRAL},
    {"Europe/Vienna", EU_CENTRAL}, {"Europe/Zurich", EU_CENTRAL}, {"Europe/Stockholm", EU_CENTRAL},
    {"Europe/Oslo", EU_CENTRAL}, {"Europe/Copenhagen", EU_CENTRAL}, {"Europe/Warsaw", EU_CENTRAL},
    {"Europe/Prague", EU_CENTRAL}, {"Europe/Budapest", EU_CENTRAL},
    {"Europe/Helsinki", EU_EAST}, {"Europe/Athens", EU_EAST}, {"Europe/Kyiv", EU_EAST},
    {"Europe/Kiev", EU_EAST}, {"Europe/Bucharest", EU_EAST}, {"Europe/Sofia", EU_EAST},
    {"Europe/Riga", EU_EAST}, {"Europe/Tallinn", EU_EAST}, {"Europe/Vilnius", EU_EAST},
    // Asia / Pacific
    {"Asia/Tokyo", "JST-9"}, {"Asia/Seoul", "KST-9"}, {"Asia/Shanghai", "CST-8"},
    {"Asia/Hong_Kong", "HKT-8"}, {"Asia/Singapore", "<+08>-8"}, {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Dubai", "<+04>-4"},
    {"Australia/Sydney", AU_EAST}, {"Australia/Melbourne", AU_EAST}, {"Australia/Hobart", AU_EAST},
    {"Australia/Brisbane", "AEST-10"}, {"Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Australia/Perth", "AWST-8"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"UTC", "UTC0"}, {"Etc/UTC", "UTC0"},
};

}  // namespace

bool posixFor(const char* iana, char* out, size_t len) {
    if (!iana || !iana[0]) return false;
    for (const Zone& z : ZONES) {
        if (strcmp(z.iana, iana) == 0) {
            snprintf(out, len, "%s", z.posix);
            return true;
        }
    }
    return false;
}

void posixFromOffset(int32_t s, char* out, size_t len) {
    // POSIX sign is inverted: UTC-6 is written "UTC6".
    const char sign = s > 0 ? '-' : '+';
    const int32_t a = s < 0 ? -s : s;
    const int h = a / 3600, m = (a % 3600) / 60;
    if (m) snprintf(out, len, "<UTC>%c%d:%02d", sign, h, m);
    else snprintf(out, len, "<UTC>%c%d", sign, h);
}

}  // namespace timezones
