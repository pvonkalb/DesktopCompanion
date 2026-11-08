#include "ui/widgets.h"

namespace ui {

using namespace theme;

void drawChevron(TFT_eSPI& g, float cx, float cy, float size, bool pointLeft, uint16_t colour, uint16_t bg) {
    const float dx = pointLeft ? size * 0.5f : -size * 0.5f;
    const float w = 2.5f;
    g.drawWideLine(cx + dx, cy - size, cx - dx, cy, w, colour, bg);
    g.drawWideLine(cx - dx, cy, cx + dx, cy + size, w, colour, bg);
}

void drawChevronButton(TFT_eSPI& tft, const Rect& r, bool pointLeft, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
        drawChevron(g, w / 2.0f, h / 2.0f, 7, pointLeft, TEXT, fill);
    });
}

void drawHeaderTitle(TFT_eSPI& tft, const char* title, int16_t rightLimit) {
    const int16_t x = HEADER_BACK.x + HEADER_BACK.w + 10;
    region(tft, x, 0, rightLimit - x, HEADER_H, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        setFont(g, Font::Title);
        char buf[40];
        fitText(g, title, w, buf, sizeof(buf));
        text(g, buf, 0, h / 2 + 1, ML_DATUM, TEXT, BG);
    });
}

void drawHeader(TFT_eSPI& tft, const char* title, bool backPressed) {
    drawChevronButton(tft, HEADER_BACK, true, backPressed);
    drawHeaderTitle(tft, title);
}

}  // namespace ui
