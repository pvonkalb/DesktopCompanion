#include "touch_test.h"

#include <Arduino.h>

#include "board_cyd.h"
#include "theme.h"
#include "touch_input.h"

namespace touch_test {
namespace {

using namespace theme;

constexpr int16_t W = board::SCREEN_W;
constexpr int16_t H = board::SCREEN_H;

constexpr int16_t CARD_X = 8, CARD_Y = 172, CARD_W = W - 16, CARD_H = 60;
constexpr int16_t COL_W = CARD_W / 4;
constexpr int16_t LABEL_Y = CARD_Y + 7, VALUE_Y = CARD_Y + 26;

constexpr uint32_t POLL_MS       = 10;
constexpr uint32_t VALUES_MS     = 80;    // throttle text redraws
constexpr uint32_t LOG_MS        = 100;
constexpr uint32_t CLEAR_AFTER_MS = 3000; // wipe the dot trail after this idle time

// Common CYD raw ranges; ONLY used for the uncalibrated estimate dot.
constexpr int16_t EST_X_MIN = 200, EST_X_MAX = 3700;
constexpr int16_t EST_Y_MIN = 240, EST_Y_MAX = 3800;

uint32_t lastPoll = 0, lastValues = 0, lastLog = 0, lastTouchMs = 0;
bool wasTouched = false, trailDirty = false, lastIrq = false;
touch_input::RawSample shown;   // what is currently on screen
int16_t minX = INT16_MAX, maxX = INT16_MIN, minY = INT16_MAX, maxY = INT16_MIN;

int16_t colCentre(int i) { return CARD_X + COL_W * i + COL_W / 2; }

void drawIrq(TFT_eSPI& tft, bool active) {
    tft.fillCircle(W - 16, 20, 6, active ? STATUS_OK : CARD_EDGE);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString("IRQ", W - 28, 21, 2);
}

void drawValue(TFT_eSPI& tft, int col, const char* text, uint16_t colour) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(colour, CARD);
    tft.setTextPadding(COL_W - 6);
    tft.drawString(text, colCentre(col), VALUE_Y, 4);
    tft.setTextPadding(0);
}

void drawValues(TFT_eSPI& tft, const touch_input::RawSample& s) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", s.x); drawValue(tft, 0, buf, TEXT);
    snprintf(buf, sizeof(buf), "%d", s.y); drawValue(tft, 1, buf, TEXT);
    snprintf(buf, sizeof(buf), "%d", s.z); drawValue(tft, 2, buf, TEXT);
    drawValue(tft, 3, s.touched ? "DOWN" : "UP", s.touched ? ACCENT : TEXT_MUTED);
    shown = s;
}

void drawStatic(TFT_eSPI& tft) {
    tft.fillScreen(BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TEXT, BG);
    tft.drawString("Touch diagnostic", 12, 10, 4);
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString("Tap around the screen, including the edges", 12, 38, 2);
    tft.drawString("Dot = uncalibrated estimate", 12, 56, 2);

    tft.fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, RADIUS, CARD);
    tft.drawRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, RADIUS, CARD_EDGE);
    const char* labels[] = {"RAW X", "RAW Y", "PRESSURE", "STATE"};
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TEXT_MUTED, CARD);
    for (int i = 0; i < 4; ++i) tft.drawString(labels[i], colCentre(i), LABEL_Y, 2);

    drawIrq(tft, lastIrq);
    drawValues(tft, shown);
    trailDirty = false;
}

void plotEstimate(TFT_eSPI& tft, const touch_input::RawSample& s) {
    const int16_t sx = constrain(map(s.x, EST_X_MIN, EST_X_MAX, 0, W - 1), 0, W - 1);
    const int16_t sy = constrain(map(s.y, EST_Y_MIN, EST_Y_MAX, 0, H - 1), 0, H - 1);
    tft.fillCircle(sx, sy, 3, ACCENT);
    trailDirty = true;
}

}  // namespace

void begin(TFT_eSPI& tft) {
    touch_input::begin();
    drawStatic(tft);
    Serial.println(F("[touch] raw diagnostic running - tap the screen"));
}

void update(TFT_eSPI& tft, uint32_t nowMs) {
    if (nowMs - lastPoll < POLL_MS) return;
    lastPoll = nowMs;

    const bool irq = touch_input::irqActive();
    if (irq != lastIrq) { lastIrq = irq; drawIrq(tft, irq); }

    const touch_input::RawSample s = touch_input::readRaw();

    if (s.touched) {
        lastTouchMs = nowMs;
        minX = min(minX, s.x); maxX = max(maxX, s.x);
        minY = min(minY, s.y); maxY = max(maxY, s.y);
        plotEstimate(tft, s);
        if (nowMs - lastLog >= LOG_MS) {
            lastLog = nowMs;
            Serial.printf("[touch] x=%4d y=%4d z=%4d irq=%d\n", s.x, s.y, s.z, irq);
        }
    } else if (wasTouched) {
        Serial.printf("[touch] release  seen range x=[%d..%d] y=[%d..%d]\n", minX, maxX, minY, maxY);
    }

    const bool stateChanged = s.touched != wasTouched;
    wasTouched = s.touched;
    if (stateChanged || (s.touched && nowMs - lastValues >= VALUES_MS)) {
        lastValues = nowMs;
        touch_input::RawSample display = s;
        if (!s.touched) { display = shown; display.z = 0; display.touched = false; }  // keep last X/Y
        drawValues(tft, display);
    }

    if (!s.touched && trailDirty && nowMs - lastTouchMs > CLEAR_AFTER_MS) drawStatic(tft);
}

}  // namespace touch_test
