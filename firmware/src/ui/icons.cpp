#include "ui/icons.h"

#include <math.h>

#include "theme.h"

namespace ui {
namespace {

constexpr uint32_t READ_BG = 0x00FFFFFF;   // TFT_eSPI: blend against existing pixels

constexpr uint16_t SUN        = theme::rgb(0xFFC94A);
constexpr uint16_t MOON       = theme::rgb(0xD6DCE4);
constexpr uint16_t CLOUD      = theme::rgb(0xC9D1DB);
constexpr uint16_t CLOUD_DARK = theme::rgb(0x8A929C);
constexpr uint16_t RAIN       = theme::ACCENT;
constexpr uint16_t SNOW       = theme::rgb(0xFFFFFF);
constexpr uint16_t BOLT       = theme::rgb(0xFFC94A);

void cloud(TFT_eSPI& g, float cx, float cy, float s, uint16_t col) {
    g.fillSmoothRoundRect(cx - 0.42f * s, cy - 0.02f * s, 0.84f * s, 0.30f * s, 0.15f * s, col, READ_BG);
    g.fillSmoothCircle(cx - 0.18f * s, cy + 0.02f * s, 0.18f * s, col, READ_BG);
    g.fillSmoothCircle(cx + 0.08f * s, cy - 0.06f * s, 0.25f * s, col, READ_BG);
}

void sun(TFT_eSPI& g, float cx, float cy, float r) {
    const float w = fmaxf(1.5f, r * 0.22f);
    for (int i = 0; i < 8; ++i) {
        const float a = i * PI / 4;
        g.drawWideLine(cx + cosf(a) * r * 1.45f, cy + sinf(a) * r * 1.45f,
                       cx + cosf(a) * r * 1.95f, cy + sinf(a) * r * 1.95f, w, SUN, READ_BG);
    }
    g.fillSmoothCircle(cx, cy, r, SUN, READ_BG);
}

void moon(TFT_eSPI& g, float cx, float cy, float r, uint16_t bg) {
    g.fillSmoothCircle(cx, cy, r, MOON, READ_BG);
    g.fillSmoothCircle(cx + 0.45f * r, cy - 0.35f * r, 0.85f * r, bg, READ_BG);   // crescent cut-out
}

void streaks(TFT_eSPI& g, float cx, float top, float s, int n) {
    const float w = fmaxf(1.5f, 0.06f * s);
    for (int i = 0; i < n; ++i) {
        const float x = cx + (i - (n - 1) / 2.0f) * 0.2f * s;
        g.drawWideLine(x, top, x - 0.07f * s, top + 0.17f * s, w, RAIN, READ_BG);
    }
}

void flakes(TFT_eSPI& g, float cx, float top, float s) {
    const float r = fmaxf(1.2f, 0.045f * s);
    for (int i = 0; i < 3; ++i) g.drawSpot(cx + (i - 1) * 0.2f * s, top + 0.04f * s, r, SNOW, READ_BG);
    for (int i = 0; i < 2; ++i) g.drawSpot(cx + (i - 0.5f) * 0.2f * s, top + 0.17f * s, r, SNOW, READ_BG);
}

}  // namespace

void drawWeatherIcon(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint8_t code, bool isDay, uint16_t bg) {
    const float s = size, cx = x, cy = y;

    switch (code) {
        case 0:                                   // clear
            if (isDay) sun(g, cx, cy, 0.24f * s);
            else moon(g, cx, cy, 0.32f * s, bg);
            return;
        case 1: case 2:                           // mainly clear / partly cloudy
            if (isDay) sun(g, cx - 0.16f * s, cy - 0.14f * s, 0.17f * s);
            else moon(g, cx - 0.14f * s, cy - 0.16f * s, 0.22f * s, bg);
            cloud(g, cx + 0.06f * s, cy + 0.08f * s, 0.8f * s, CLOUD);
            return;
        case 3:                                   // overcast
            cloud(g, cx + 0.12f * s, cy - 0.08f * s, 0.7f * s, CLOUD_DARK);
            cloud(g, cx - 0.02f * s, cy + 0.06f * s, 0.9f * s, CLOUD);
            return;
        case 45: case 48: {                       // fog
            const float w = fmaxf(2.0f, 0.07f * s);
            for (int i = 0; i < 3; ++i) {
                const float yy = cy + (i - 1) * 0.16f * s, off = (i % 2) * 0.08f * s;
                g.drawWideLine(cx - 0.38f * s + off, yy, cx + 0.30f * s + off, yy, w, CLOUD, READ_BG);
            }
            return;
        }
        case 71: case 73: case 75: case 77: case 85: case 86:   // snow
            cloud(g, cx, cy - 0.12f * s, s, CLOUD);
            flakes(g, cx, cy + 0.24f * s, s);
            return;
        case 95: case 96: case 99:                // thunderstorm
            cloud(g, cx, cy - 0.14f * s, s, CLOUD_DARK);
            g.fillTriangle(cx + 0.06f * s, cy + 0.12f * s, cx - 0.10f * s, cy + 0.34f * s,
                           cx + 0.02f * s, cy + 0.32f * s, BOLT);
            g.fillTriangle(cx - 0.02f * s, cy + 0.27f * s, cx + 0.10f * s, cy + 0.27f * s,
                           cx - 0.08f * s, cy + 0.50f * s, BOLT);
            return;
        default:
            if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {   // drizzle / rain / showers
                cloud(g, cx, cy - 0.12f * s, s, CLOUD);
                streaks(g, cx, cy + 0.24f * s, s, code <= 57 ? 2 : 3);
                return;
            }
            cloud(g, cx, cy, s, CLOUD);           // unknown code: neutral cloud
            return;
    }
}

void drawAppIcon(TFT_eSPI& g, int16_t x, int16_t y, AppIcon icon, uint16_t bg) {
    const float cx = x, cy = y;
    constexpr uint16_t FG = theme::ACCENT;
    constexpr uint16_t SOFT = theme::rgb(0xD6DCE4);

    switch (icon) {
        case AppIcon::Calendar:
            g.fillSmoothRoundRect(cx - 14, cy - 11, 28, 25, 5, FG, READ_BG);
            g.fillSmoothRoundRect(cx - 11, cy - 4, 22, 15, 2, bg, READ_BG);
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 3; ++c)
                    g.drawSpot(cx - 6 + c * 6, cy + 1 + r * 6, 1.6f, SOFT, READ_BG);
            g.drawWideLine(cx - 7, cy - 15, cx - 7, cy - 9, 3, SOFT, READ_BG);
            g.drawWideLine(cx + 7, cy - 15, cx + 7, cy - 9, 3, SOFT, READ_BG);
            return;

        case AppIcon::Weather:
            drawWeatherIcon(g, x, y, 34, 2, true, bg);
            return;

        case AppIcon::Focus:   // timer ring with progress
            g.drawSmoothArc(x, y, 14, 11, 0, 360, theme::CARD_EDGE, bg, false);
            g.drawSmoothArc(x, y, 14, 11, 180, 60, FG, bg, true);   // clockwise from 12 o'clock to ~8
            g.fillSmoothCircle(cx, cy, 3, SOFT, READ_BG);
            return;

        case AppIcon::Timer:   // hourglass
            g.fillTriangle(cx - 5, cy - 7, cx + 5, cy - 7, cx, cy - 1, SOFT);         // sand, top
            g.fillTriangle(cx - 7, cy + 11, cx + 7, cy + 11, cx, cy + 4, SOFT);       // sand, bottom
            g.drawWideLine(cx, cy - 1, cx, cy + 5, 1.2f, SOFT, READ_BG);              // falling stream
            g.drawWideLine(cx - 9, cy - 12, cx - 1.5f, cy, 2.5f, FG, READ_BG);        // glass outline
            g.drawWideLine(cx + 9, cy - 12, cx + 1.5f, cy, 2.5f, FG, READ_BG);
            g.drawWideLine(cx - 1.5f, cy, cx - 9, cy + 12, 2.5f, FG, READ_BG);
            g.drawWideLine(cx + 1.5f, cy, cx + 9, cy + 12, 2.5f, FG, READ_BG);
            g.drawWideLine(cx - 12, cy - 14, cx + 12, cy - 14, 3, FG, READ_BG);       // caps
            g.drawWideLine(cx - 12, cy + 14, cx + 12, cy + 14, 3, FG, READ_BG);
            return;

        case AppIcon::Music:   // two beamed notes
            g.fillSmoothCircle(cx - 8, cy + 9, 4.5f, FG, READ_BG);
            g.fillSmoothCircle(cx + 7, cy + 6, 4.5f, FG, READ_BG);
            g.drawWideLine(cx - 4, cy + 9, cx - 4, cy - 10, 2.5f, FG, READ_BG);
            g.drawWideLine(cx + 11, cy + 6, cx + 11, cy - 13, 2.5f, FG, READ_BG);
            g.drawWideLine(cx - 4, cy - 10, cx + 11, cy - 13, 4.5f, FG, READ_BG);
            return;

        case AppIcon::Pc:      // monitor with a small bar chart
            g.fillSmoothRoundRect(cx - 15, cy - 13, 30, 21, 4, FG, READ_BG);
            g.fillSmoothRoundRect(cx - 12, cy - 10, 24, 15, 2, bg, READ_BG);
            g.fillRect(cx - 8, cy - 2, 4, 5, SOFT);
            g.fillRect(cx - 2, cy - 6, 4, 9, SOFT);
            g.fillRect(cx + 4, cy - 4, 4, 7, SOFT);
            g.drawWideLine(cx, cy + 8, cx, cy + 12, 3, FG, READ_BG);
            g.drawWideLine(cx - 7, cy + 13, cx + 7, cy + 13, 3, FG, READ_BG);
            return;

        case AppIcon::Settings: {   // gear
            for (int i = 0; i < 8; ++i) {
                const float a = i * PI / 4;
                g.drawWideLine(cx + cosf(a) * 8, cy + sinf(a) * 8, cx + cosf(a) * 14, cy + sinf(a) * 14,
                               5.5f, FG, READ_BG);
            }
            g.fillSmoothCircle(cx, cy, 10.5f, FG, READ_BG);
            g.fillSmoothCircle(cx, cy, 4.5f, bg, READ_BG);
            return;
        }
    }
}

const char* weatherLabel(uint8_t code) {
    switch (code) {
        case 0: return "Clear";
        case 1: return "Mostly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: case 48: return "Fog";
        case 51: case 53: case 55: return "Drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: return "Light rain";
        case 63: return "Rain";
        case 65: return "Heavy rain";
        case 66: case 67: return "Freezing rain";
        case 71: return "Light snow";
        case 73: return "Snow";
        case 75: return "Heavy snow";
        case 77: return "Snow grains";
        case 80: case 81: case 82: return "Showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunder & hail";
    }
    return "Unknown";
}

}  // namespace ui
