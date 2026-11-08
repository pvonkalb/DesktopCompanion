#include "calibration_screen.h"

#include <Arduino.h>
#include <math.h>

#include "board_cyd.h"
#include "theme.h"
#include "touch_calibration.h"

namespace calibration_screen {
namespace {

using namespace theme;

constexpr int16_t W = board::SCREEN_W;
constexpr int16_t H = board::SCREEN_H;
constexpr int16_t INSET = 24;

constexpr int STEPS = 5;           // 4 corners to fit + centre to verify
constexpr int FIT_POINTS = 4;
const touch_cal::Point TARGETS[STEPS] = {
    {INSET, INSET}, {W - 1 - INSET, INSET},
    {W - 1 - INSET, H - 1 - INSET}, {INSET, H - 1 - INSET},
    {W / 2, H / 2},
};

constexpr uint32_t SETTLE_MS   = 100;  // ignore the first moments of contact
constexpr uint32_t SAMPLE_MS   = 10;
constexpr int      MIN_SAMPLES = 8;
constexpr int      DROP_TAIL   = 4;    // discard samples taken while the finger lifts
constexpr int      RING        = 64;
constexpr float    MAX_ERR_PX  = 12.0f;
constexpr uint32_t MESSAGE_MS  = 1800;

constexpr int16_t TITLE_Y = 44, INSTRUCTION_Y = 192, STEP_Y = 214;

enum class Phase { WaitLift, Target, Retry, Finished };

Phase phase = Phase::Target;
int step = 0;
bool holding = false, isDone = false;
uint32_t holdStart = 0, lastSampleMs = 0, phaseUntil = 0;
touch_cal::Point ring[RING];
int count = 0;
touch_cal::Point measured[STEPS];
touch_cal::Matrix fitted{};

void drawTarget(TFT_eSPI& tft, bool active) {
    const int16_t cx = TARGETS[step].x, cy = TARGETS[step].y;
    tft.fillRect(cx - 19, cy - 19, 39, 39, BG);
    tft.drawFastHLine(cx - 18, cy, 37, ACCENT);
    tft.drawFastVLine(cx, cy - 18, 37, ACCENT);
    if (active) {
        tft.fillCircle(cx, cy, 12, ACCENT);
        tft.fillCircle(cx, cy, 2, BG);
    } else {
        tft.fillCircle(cx, cy, 12, BG);
        tft.drawCircle(cx, cy, 12, ACCENT);
        tft.drawCircle(cx, cy, 11, ACCENT);
        tft.fillCircle(cx, cy, 2, TEXT);
    }
}

void drawLine(TFT_eSPI& tft, const char* text, int16_t y, uint8_t font, uint16_t colour) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(colour, BG);
    tft.setTextPadding(W - 2 * (INSET + 24));   // never reaches the corner targets
    tft.drawString(text, W / 2, y, font);
    tft.setTextPadding(0);
}

void drawStepScreen(TFT_eSPI& tft) {
    tft.fillScreen(BG);
    const bool verify = step == FIT_POINTS;
    drawLine(tft, "Touch calibration", TITLE_Y, 4, TEXT);
    drawTarget(tft, false);
    drawLine(tft, verify ? "Check: tap and hold the centre" : "Tap and hold the target",
             INSTRUCTION_Y, 2, TEXT_MUTED);
    char buf[16];
    snprintf(buf, sizeof(buf), "Step %d of %d", step + 1, STEPS);
    drawLine(tft, buf, STEP_Y, 2, TEXT_MUTED);
}

void showResult(TFT_eSPI& tft, Phase next, const char* title, const char* detail, uint16_t accent,
                uint32_t nowMs) {
    phase = next;
    phaseUntil = nowMs + MESSAGE_MS;
    tft.fillScreen(BG);
    tft.fillRoundRect(W / 2 - 40, 70, 80, 4, 2, accent);
    drawLine(tft, title, 110, 4, TEXT);
    drawLine(tft, detail, 140, 2, TEXT_MUTED);
}

void startRun(TFT_eSPI& tft) {
    phase = Phase::Target;
    step = 0;
    holding = false;
    drawStepScreen(tft);
}

void finishHold(TFT_eSPI& tft, uint32_t nowMs) {
    const int usable = count - DROP_TAIL;
    if (usable < MIN_SAMPLES) {
        drawTarget(tft, false);
        drawLine(tft, "Hold the target a little longer", INSTRUCTION_Y, 2, STATUS_WARN);
        return;
    }
    const int k = min(usable, RING - DROP_TAIL);
    double sx = 0, sy = 0;
    for (int j = 0; j < k; ++j) {
        const touch_cal::Point& p = ring[(count - DROP_TAIL - 1 - j) % RING];
        sx += p.x; sy += p.y;
    }
    measured[step] = {static_cast<float>(sx / k), static_cast<float>(sy / k)};
    Serial.printf("[cal] step %d target=(%.0f,%.0f) raw=(%.0f,%.0f) n=%d\n", step + 1,
                  TARGETS[step].x, TARGETS[step].y, measured[step].x, measured[step].y, k);

    if (step < FIT_POINTS - 1) { ++step; drawStepScreen(tft); return; }

    if (step == FIT_POINTS - 1) {
        if (!touch_cal::solve(measured, TARGETS, FIT_POINTS, fitted)) {
            Serial.println(F("[cal] fit failed (degenerate points)"));
            showResult(tft, Phase::Retry, "Let's try again", "Those taps didn't line up", STATUS_WARN, nowMs);
            return;
        }
        ++step;
        drawStepScreen(tft);
        return;
    }

    // Verify step: map the centre tap through the new fit.
    const touch_cal::Point p = touch_cal::map(fitted, measured[step].x, measured[step].y);
    const float err = hypotf(p.x - TARGETS[step].x, p.y - TARGETS[step].y);
    Serial.printf("[cal] verify error %.1f px\n", err);
    char buf[40];
    if (err > MAX_ERR_PX) {
        snprintf(buf, sizeof(buf), "Centre was off by %d px", static_cast<int>(lroundf(err)));
        showResult(tft, Phase::Retry, "Let's try again", buf, STATUS_WARN, nowMs);
        return;
    }
    const bool saved = touch_cal::save(fitted);
    touch_input::setCalibration(fitted);
    Serial.printf("[cal] %s ax=%.5f bx=%.5f cx=%.2f ay=%.5f by=%.5f cy=%.2f\n",
                  saved ? "saved" : "SAVE FAILED (applied for this session only)",
                  fitted.ax, fitted.bx, fitted.cx, fitted.ay, fitted.by, fitted.cy);
    snprintf(buf, sizeof(buf), saved ? "Accuracy: %d px" : "Not saved - %d px",
             static_cast<int>(lroundf(err)));
    showResult(tft, Phase::Finished, saved ? "Calibration saved" : "Calibration applied", buf,
               saved ? STATUS_OK : STATUS_WARN, nowMs);
}

}  // namespace

void begin(TFT_eSPI& tft) {
    isDone = false;
    startRun(tft);
    if (touch_input::irqActive() || touch_input::isDown()) {   // e.g. held at boot to force recalibration
        phase = Phase::WaitLift;
        drawLine(tft, "Release the screen to begin", INSTRUCTION_Y, 2, STATUS_WARN);
    }
}

void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) {
    if (phase == Phase::WaitLift) {
        if (ev.type == touch_input::EventType::Release) startRun(tft);
        return;
    }
    if (phase != Phase::Target) return;

    if (ev.type == touch_input::EventType::Press) {
        holding = true;
        holdStart = lastSampleMs = nowMs;
        count = 0;
        drawTarget(tft, true);
        drawLine(tft, "Hold steady...", INSTRUCTION_Y, 2, TEXT_MUTED);
    } else if (ev.type == touch_input::EventType::Release && holding) {
        holding = false;
        finishHold(tft, nowMs);
    }
}

void update(TFT_eSPI& tft, uint32_t nowMs) {
    switch (phase) {
        case Phase::WaitLift:
            if (!touch_input::isDown() && !touch_input::irqActive()) startRun(tft);
            break;
        case Phase::Target:
            if (holding && touch_input::isDown() && nowMs - holdStart >= SETTLE_MS &&
                nowMs - lastSampleMs >= SAMPLE_MS) {
                lastSampleMs = nowMs;
                ring[count % RING] = touch_input::smoothedRaw();
                ++count;
            }
            break;
        case Phase::Retry:
            if (static_cast<int32_t>(nowMs - phaseUntil) >= 0) startRun(tft);
            break;
        case Phase::Finished:
            if (static_cast<int32_t>(nowMs - phaseUntil) >= 0) isDone = true;
            break;
    }
}

bool done() { return isDone; }

}  // namespace calibration_screen
