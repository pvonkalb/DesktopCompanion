#include "model.h"

namespace model {

State& state() {
    static State s;
    return s;
}

void sortCalendar(Calendar& c) {   // insertion sort: n <= 24
    for (int i = 1; i < c.count; ++i) {
        const Event e = c.events[i];
        int j = i - 1;
        while (j >= 0 && c.events[j].start > e.start) {
            c.events[j + 1] = c.events[j];
            --j;
        }
        c.events[j + 1] = e;
    }
}

}  // namespace model
