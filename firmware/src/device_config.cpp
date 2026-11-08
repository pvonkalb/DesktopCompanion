#include "device_config.h"

#include <Preferences.h>

namespace device_config {
namespace {

constexpr const char* NVS_NAMESPACE = "cfg";
constexpr const char* KEY_LOCATION  = "loc";
constexpr uint32_t    MAGIC         = 0x4C4F4331;   // "LOC1" - bump if Location layout changes

struct Stored {
    uint32_t magic;
    Location loc;
};

Location current;

}  // namespace

void load() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    Stored st;
    if (prefs.isKey(KEY_LOCATION) && prefs.getBytesLength(KEY_LOCATION) == sizeof(st) &&
        prefs.getBytes(KEY_LOCATION, &st, sizeof(st)) == sizeof(st) && st.magic == MAGIC) {
        current = st.loc;
        // defensive termination in case of corruption
        current.query[sizeof(current.query) - 1] = current.country[sizeof(current.country) - 1] = '\0';
        current.name[sizeof(current.name) - 1] = current.tz[sizeof(current.tz) - 1] = '\0';
    }
    prefs.end();
}

Location& location() { return current; }

bool saveLocation() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    Stored st{MAGIC, current};
    const bool ok = prefs.putBytes(KEY_LOCATION, &st, sizeof(st)) == sizeof(st);
    prefs.end();
    return ok;
}

}  // namespace device_config
