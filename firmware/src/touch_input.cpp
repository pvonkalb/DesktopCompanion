#include "touch_input.h"

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "board_cyd.h"
#include "settings.h"

namespace touch_input {
namespace {

// Tuning (measured on the dev unit: touched pressure min ~560, median ~2100)
// Pressure hysteresis: a touch must be firm to START (light stylus contact drifts
// and used to split one gesture into two), but may lighten to HOLD_Z while held.
constexpr int16_t  PRESSURE_MIN    = 300;   // below this a reading is noise / release
constexpr int16_t  PRESS_START_Z   = 500;   // finger min seen 562; light stylus contact 430-490
constexpr uint32_t SAMPLE_MS       = 10;    // 100 Hz
constexpr uint8_t  PRESS_CONFIRM   = 2;     // consecutive touched samples to start a press
constexpr uint8_t  RELEASE_CONFIRM = 8;     // consecutive untouched samples to end it (stylus lightens >50 ms mid-hold)
constexpr float    SMOOTHING       = 0.5f;  // EMA weight of each new raw sample
constexpr uint32_t TAP_MAX_MS      = 500;
constexpr uint32_t LONG_PRESS_MS   = 550;
constexpr int16_t  SLOP_PX         = 20;    // movement allowed for tap / long-press

constexpr uint8_t QUEUE_LEN = 4;

SPIClass touchSpi(VSPI);   // display uses HSPI, so VSPI is free
XPT2046_Touchscreen ts(board::PIN_TOUCH_CS, board::PIN_TOUCH_IRQ);
touch_cal::Matrix cal = touch_cal::defaults();

constexpr int16_t PRESS_AGREE_RAW = 150;   // confirm samples must agree (~13 px) - rejects lift-off noise

bool down = false;
uint8_t touchedRun = 0, releasedRun = 0;
int16_t firstX = 0, firstY = 0;            // first sample of a candidate press
float rawX = 0, rawY = 0;
int16_t posX = 0, posY = 0, pressX = 0, pressY = 0, lastZ = 0;
uint32_t pressMs = 0, lastSample = 0;
bool longFired = false, leftSlop = false;

Event queue[QUEUE_LEN];
uint8_t qHead = 0, qCount = 0;

void push(EventType t) {
    if (qCount == QUEUE_LEN) return;   // drop if the app falls behind (never blocks)
    queue[(qHead + qCount) % QUEUE_LEN] = {t, posX, posY};
    ++qCount;
    if (settings::TOUCH_LOG && t != EventType::Move)
        Serial.printf("[touch] %7lu %-7s %3d,%3d  raw=%4.0f,%4.0f z=%d\n", static_cast<unsigned long>(millis()),
                      eventName(t), posX, posY, rawX, rawY, lastZ);
}

void updatePosition() {
    const touch_cal::Point p = touch_cal::map(cal, rawX, rawY);
    posX = static_cast<int16_t>(constrain(lroundf(p.x), 0L, static_cast<long>(board::SCREEN_W - 1)));
    posY = static_cast<int16_t>(constrain(lroundf(p.y), 0L, static_cast<long>(board::SCREEN_H - 1)));
}

void sample(uint32_t now) {
    const RawSample s = readRaw();

    if (!s.touched) {
        touchedRun = 0;
        if (down && ++releasedRun >= RELEASE_CONFIRM) {
            down = false;
            releasedRun = 0;
            push(EventType::Release);
            if (!longFired && !leftSlop && now - pressMs <= TAP_MAX_MS) push(EventType::Tap);
        }
        return;
    }

    releasedRun = 0;
    lastZ = s.z;
    if (!down) {
        if (s.z < PRESS_START_Z) { touchedRun = 0; return; }   // too light to start a gesture
        if (touchedRun == 0 ||
            abs(s.x - firstX) > PRESS_AGREE_RAW || abs(s.y - firstY) > PRESS_AGREE_RAW) {
            if (settings::TOUCH_LOG && touchedRun > 0)
                Serial.printf("[touch] rejected unstable press (%d,%d)->(%d,%d) z=%d\n",
                              firstX, firstY, s.x, s.y, s.z);
            firstX = s.x; firstY = s.y;   // (re)start candidate press
            touchedRun = 1;
            if (touchedRun < PRESS_CONFIRM) return;
        } else if (++touchedRun < PRESS_CONFIRM) {
            return;
        }
        down = true;
        rawX = s.x; rawY = s.y;
        updatePosition();
        pressX = posX; pressY = posY; pressMs = now;
        longFired = leftSlop = false;
        push(EventType::Press);
        return;
    }

    rawX += SMOOTHING * (s.x - rawX);
    rawY += SMOOTHING * (s.y - rawY);
    const int16_t oldX = posX, oldY = posY;
    updatePosition();
    if (abs(posX - pressX) > SLOP_PX || abs(posY - pressY) > SLOP_PX) leftSlop = true;
    if (posX != oldX || posY != oldY) push(EventType::Move);
    if (!longFired && !leftSlop && now - pressMs >= LONG_PRESS_MS) {
        longFired = true;
        push(EventType::LongPress);
    }
}

}  // namespace

void begin() {
    touchSpi.begin(board::PIN_TOUCH_CLK, board::PIN_TOUCH_MISO,
                   board::PIN_TOUCH_MOSI, board::PIN_TOUCH_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);   // match the display's landscape rotation
}

void setCalibration(const touch_cal::Matrix& m) { cal = m; }

bool poll(uint32_t nowMs, Event& out) {
    if (qCount == 0 && nowMs - lastSample >= SAMPLE_MS) {
        lastSample = nowMs;
        sample(nowMs);
    }
    if (qCount == 0) return false;
    out = queue[qHead];
    qHead = (qHead + 1) % QUEUE_LEN;
    --qCount;
    return true;
}

bool isDown() { return down; }

touch_cal::Point smoothedRaw() { return {rawX, rawY}; }

bool irqActive() { return digitalRead(board::PIN_TOUCH_IRQ) == LOW; }

RawSample readRaw() {
    RawSample s;
    const TS_Point p = ts.getPoint();
    s.x = p.x;
    s.y = p.y;
    s.z = p.z;
    s.touched = p.z >= PRESSURE_MIN;
    return s;
}

const char* eventName(EventType t) {
    switch (t) {
        case EventType::Press:     return "PRESS";
        case EventType::Move:      return "MOVE";
        case EventType::Release:   return "RELEASE";
        case EventType::Tap:       return "TAP";
        case EventType::LongPress: return "HOLD";
    }
    return "?";
}

}  // namespace touch_input
