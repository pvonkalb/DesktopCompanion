#include "touch_test.h"

#include <Arduino.h>

#include "board_cyd.h"
#include "theme.h"

namespace touch_test {
namespace {

using namespace theme;
using touch_input::EventType;

constexpr int16_t W = board::SCREEN_W;
constexpr int16_t H = board::SCREEN_H;

struct Rect {
    int16_t x, y, w, h;
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

constexpr int16_t HEADER_H = 36;
constexpr int16_t BTN_H = 44;
constexpr Rect CANVAS = {0, HEADER_H, W, H - HEADER_H - BTN_H - 2 * PAD};
constexpr Rect BTN_CLEAR = {PAD, H - BTN_H - PAD, (W - 3 * PAD) / 2, BTN_H};
constexpr Rect BTN_CAL = {static_cast<int16_t>(2 * PAD + (W - 3 * PAD) / 2), H - BTN_H - PAD,
                          (W - 3 * PAD) / 2, BTN_H};

bool recalRequested = false;
const Rect* pressedBtn = nullptr;   // button under the current press
bool drawing = false;
int16_t lastX = 0, lastY = 0;

void drawButton(TFT_eSPI& tft, const Rect& r, const char* label, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : CARD;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, RADIUS, fill);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, RADIUS, pressed ? ACCENT : CARD_EDGE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TEXT, fill);
    tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2 + 1, 4);
}

void drawButtons(TFT_eSPI& tft) {
    drawButton(tft, BTN_CLEAR, "Clear", pressedBtn == &BTN_CLEAR);
    drawButton(tft, BTN_CAL, "Calibrate", pressedBtn == &BTN_CAL);
}

void drawStatus(TFT_eSPI& tft, const char* text) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TEXT_MUTED, BG);
    tft.setTextPadding(130);
    tft.drawString(text, W - PAD - 4, HEADER_H / 2, 2);
    tft.setTextPadding(0);
}

void clearCanvas(TFT_eSPI& tft) {
    tft.fillRect(CANVAS.x, CANVAS.y, CANVAS.w, CANVAS.h, BG);
    // faint guides at 1/4 points help judge accuracy
    for (int i = 1; i < 4; ++i) {
        tft.drawFastVLine(W * i / 4, CANVAS.y, CANVAS.h, CARD);
        tft.drawFastHLine(0, CANVAS.y + CANVAS.h * i / 4, W, CARD);
    }
}

void drawAll(TFT_eSPI& tft) {
    tft.fillScreen(BG);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TEXT, BG);
    tft.drawString("Touch test", PAD + 4, HEADER_H / 2, 4);
    tft.drawFastHLine(0, HEADER_H - 1, W, CARD_EDGE);
    clearCanvas(tft);
    drawButtons(tft);
    drawStatus(tft, "Draw or tap a button");
}

}  // namespace

void begin(TFT_eSPI& tft) {
    recalRequested = false;
    pressedBtn = nullptr;
    drawing = false;
    drawAll(tft);
}

void handle(TFT_eSPI& tft, const touch_input::Event& ev) {
    char buf[24];

    switch (ev.type) {
        case EventType::Press:
            if (BTN_CLEAR.contains(ev.x, ev.y)) pressedBtn = &BTN_CLEAR;
            else if (BTN_CAL.contains(ev.x, ev.y)) pressedBtn = &BTN_CAL;
            if (pressedBtn) { drawButtons(tft); break; }
            drawing = CANVAS.contains(ev.x, ev.y);
            if (drawing) tft.fillCircle(ev.x, ev.y, 2, ACCENT);
            lastX = ev.x; lastY = ev.y;
            break;

        case EventType::Move:
            if (pressedBtn && !pressedBtn->contains(ev.x, ev.y)) {   // slid off: cancel
                pressedBtn = nullptr;
                drawButtons(tft);
            }
            if (drawing && CANVAS.contains(ev.x, ev.y)) {
                tft.drawLine(lastX, lastY, ev.x, ev.y, ACCENT);
                tft.drawLine(lastX + 1, lastY, ev.x + 1, ev.y, ACCENT);   // 2 px stroke
                tft.drawLine(lastX, lastY + 1, ev.x, ev.y + 1, ACCENT);
                lastX = ev.x; lastY = ev.y;
            }
            break;

        case EventType::Release:
            if (pressedBtn) {
                const Rect* btn = pressedBtn;
                pressedBtn = nullptr;
                drawButtons(tft);
                if (btn->contains(ev.x, ev.y)) {   // activate on release inside, like a phone
                    if (btn == &BTN_CLEAR) { clearCanvas(tft); drawStatus(tft, "Cleared"); }
                    else recalRequested = true;
                }
            }
            drawing = false;
            break;

        case EventType::Tap:
            snprintf(buf, sizeof(buf), "Tap  %d, %d", ev.x, ev.y);
            drawStatus(tft, buf);
            break;

        case EventType::LongPress:
            if (!pressedBtn && CANVAS.contains(ev.x, ev.y)) tft.drawCircle(ev.x, ev.y, 14, STATUS_WARN);
            snprintf(buf, sizeof(buf), "Hold  %d, %d", ev.x, ev.y);
            drawStatus(tft, buf);
            break;
    }

    if (ev.type == EventType::Tap || ev.type == EventType::LongPress)
        Serial.printf("[touch] %s %d,%d\n", touch_input::eventName(ev.type), ev.x, ev.y);
}

bool wantsRecalibration() { return recalRequested; }

}  // namespace touch_test
