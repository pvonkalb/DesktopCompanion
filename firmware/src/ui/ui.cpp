#include "ui/ui.h"

#include <math.h>
#include <string.h>

#include "fonts/inter_body_16.h"
#include "fonts/inter_clock_60.h"
#include "fonts/inter_label_13.h"
#include "fonts/inter_temp_40.h"
#include "fonts/inter_title_22.h"
#include "settings.h"

namespace ui {
namespace {

const uint8_t* fontData(Font f) {
    switch (f) {
        case Font::Clock: return inter_clock_60;
        case Font::Temp:  return inter_temp_40;
        case Font::Title: return inter_title_22;
        case Font::Body:  return inter_body_16;
        case Font::Label: return inter_label_13;
    }
    return inter_body_16;
}

constexpr uint16_t CALENDAR_COLOURS[] = {
    theme::ACCENT,              // first calendar: cyan
    theme::rgb(0xB18CFF),       // second: soft violet
    theme::rgb(0x3DDC84),       // third: mint
    theme::rgb(0xFFB020),       // fourth: amber
};

}  // namespace

void setFont(TFT_eSPI& g, Font f) {
    const uint8_t* data = fontData(f);
    if (g.fontLoaded && g.gFont.gArray == data) return;
    if (g.fontLoaded) g.unloadFont();
    g.loadFont(data);
}

void clearFont(TFT_eSPI& g) {
    if (g.fontLoaded) g.unloadFont();
}

void fitText(TFT_eSPI& g, const char* text, int16_t maxWidth, char* out, size_t outLen) {
    strncpy(out, text, outLen - 1);
    out[outLen - 1] = '\0';
    if (g.textWidth(out) <= maxWidth) return;
    static const char ELLIPSIS[] = "\xE2\x80\xA6";   // "…" in UTF-8
    size_t n = strlen(out);
    while (n > 0) {
        --n;
        while (n > 0 && (static_cast<uint8_t>(out[n]) & 0xC0) == 0x80) --n;   // UTF-8 boundary
        while (n > 0 && out[n - 1] == ' ') --n;                                // no space before "…"
        if (n + sizeof(ELLIPSIS) > outLen) continue;
        memcpy(out + n, ELLIPSIS, sizeof(ELLIPSIS));
        if (g.textWidth(out) <= maxWidth) return;
    }
    out[0] = '\0';
}

void text(TFT_eSPI& g, const char* s, int16_t x, int16_t y, uint8_t datum, uint16_t fg, uint16_t bg) {
    g.setTextDatum(datum);
    g.setTextColor(fg, bg);
    g.drawString(s, x, y);
}

uint16_t calendarColour(uint8_t i) {
    return CALENDAR_COLOURS[i % (sizeof(CALENDAR_COLOURS) / sizeof(CALENDAR_COLOURS[0]))];
}

int roundTemp(float c) {
    const float v = settings::FAHRENHEIT ? c * 9.0f / 5.0f + 32.0f : c;
    return static_cast<int>(lroundf(v));
}

}  // namespace ui
