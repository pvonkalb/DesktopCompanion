#pragma once
// Local-time helpers and display formatting.

#include <stddef.h>
#include <time.h>

namespace timefmt {

bool valid();                                   // false until a time source has set the clock
bool local(struct tm& out);                     // current local time; false if not valid
bool local(time_t t, struct tm& out);

// "2:32" + "PM" (12 h) or "14:32" + "" (24 h)
void clock(const struct tm& t, char* hm, size_t hmLen, char* ampm, size_t ampmLen);
void date(const struct tm& t, char* out, size_t len);                 // "Friday, Nov 7"
void timeOfDay(time_t t, char* out, size_t len);                      // "6:30 PM"
// "Now · until 7:30 PM", "Today · in 2h 15m", "Tomorrow", "Saturday", "Nov 14"
void when(time_t start, time_t end, bool allDay, time_t now, char* out, size_t len);
int  dayDiff(time_t from, time_t to);           // calendar-day difference in local time

}  // namespace timefmt
