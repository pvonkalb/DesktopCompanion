#include "version.h"

#include <stdio.h>

namespace version {

const char* buildStamp() { return __DATE__ " " __TIME__; }

const char* full() {
    static char buf[40];
    snprintf(buf, sizeof(buf), "%s (%.17s)", FW_VERSION, buildStamp());   // drop the seconds
    return buf;
}

}  // namespace version
