#include "backlight.h"

#include <Arduino.h>

#include "board_cyd.h"
#include "settings.h"
#include "user_prefs.h"

namespace backlight {
namespace {

constexpr uint8_t  DAY_LEVEL   = 255;
constexpr uint8_t  NIGHT_LEVEL = 40;     // ~15 %: readable, not glaring in a dark room
constexpr uint8_t  DIP_LEVEL   = 30;     // momentary level while a new screen is drawn
constexpr float    FADE_PER_MS = 255.0f / 140.0f;   // full-range fade in ~140 ms
constexpr uint32_t SAMPLE_MS   = 500;
constexpr uint32_t LOG_MS      = 5000;
constexpr uint32_t HOLD_MS     = 8000;   // light level must persist this long before switching

float level = 0;
uint8_t target = DAY_LEVEL;
bool shown = false, held = false, night = false;
bool candidate = false;
uint32_t candidateSince = 0, lastSample = 0, lastLog = 0, lastStep = 0;
float smoothed = -1;

void apply() { board::setBrightness(static_cast<uint8_t>(level + 0.5f)); }

}  // namespace

void begin() {
    level = 0;
    apply();
}

void show() { shown = true; }

void dipForRedraw() {
    if (level > DIP_LEVEL) {
        level = DIP_LEVEL;
        apply();
    }
}

void suspend(bool full) {
    held = full;
    if (full) {
        level = 255;
        apply();
    }
}

bool isNight() { return night; }
int ambient() { return static_cast<int>(smoothed); }

void loop(uint32_t now) {
    if (now - lastSample >= SAMPLE_MS) {
        lastSample = now;
        const int raw = board::readAmbientRaw();
        smoothed = smoothed < 0 ? raw : smoothed * 0.8f + raw * 0.2f;
        if (settings::LIGHT_LOG && now - lastLog >= LOG_MS) {
            lastLog = now;
            Serial.printf("[light] raw=%d smoothed=%d night=%d\n", raw, static_cast<int>(smoothed), night);
        }
        if (user_prefs::autoDim()) {
            // Hysteresis + hold time so passing shadows or a lamp flicker don't toggle it.
            const bool want = night ? smoothed > settings::LIGHT_NIGHT_OFF : smoothed > settings::LIGHT_NIGHT_ON;
            if (want == night) {
                candidate = night;
            } else if (want != candidate) {
                candidate = want;
                candidateSince = now;
            } else if (now - candidateSince >= HOLD_MS) {
                night = want;
                Serial.printf("[light] %s (ambient %d)\n", night ? "night - dimming" : "day - full brightness",
                              static_cast<int>(smoothed));
            }
        } else {
            night = false;
        }
        target = held ? 255 : (night ? NIGHT_LEVEL : DAY_LEVEL);
    }

    if (!shown) return;
    const uint32_t dt = now - lastStep;
    lastStep = now;
    if (static_cast<int>(level + 0.5f) == target) return;
    const float step = FADE_PER_MS * min<uint32_t>(dt, 50);
    level = level < target ? min<float>(target, level + step) : max<float>(target, level - step);
    apply();
}

}  // namespace backlight
