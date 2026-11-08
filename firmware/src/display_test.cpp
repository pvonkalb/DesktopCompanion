#include "display_test.h"

#include "board_cyd.h"
#include "theme.h"

namespace display_test {
namespace {

using namespace theme;

constexpr int16_t W = board::SCREEN_W;
constexpr int16_t H = board::SCREEN_H;

// Info card geometry
constexpr int16_t CARD_X = 12, CARD_Y = 158, CARD_W = W - 24, CARD_H = 64;
constexpr int16_t LABEL_Y = CARD_Y + 8;
constexpr int16_t VALUE_Y = CARD_Y + 26;
constexpr int16_t BAR_X = CARD_X + 10, BAR_Y = CARD_Y + CARD_H - 10;
constexpr int16_t BAR_W = CARD_W - 20, BAR_H = 4;
constexpr uint32_t BAR_PERIOD_MS = 2000;
constexpr uint32_t BAR_FRAME_MS  = 16;   // ~60 fps cap

uint32_t fillMs = 0;
uint32_t lastUptimeSec = UINT32_MAX;
uint32_t lastBarFrame = 0;
int16_t barFilled = 0;

void drawCornerMarkers(TFT_eSPI& tft) {
    tft.drawRect(0, 0, W, H, ACCENT);   // must touch all four physical edges
    tft.setTextFont(1);
    tft.setTextColor(ACCENT, BG);
    tft.setTextDatum(TL_DATUM); tft.drawString("TL", 3, 3);
    tft.setTextDatum(TR_DATUM); tft.drawString("TR", W - 3, 3);
    tft.setTextDatum(BL_DATUM); tft.drawString("BL", 3, H - 2);
    tft.setTextDatum(BR_DATUM); tft.drawString("BR", W - 3, H - 2);
}

void drawHeader(TFT_eSPI& tft) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TEXT, BG);
    tft.drawString("Desktop Companion", 12, 14, 4);
    tft.setTextColor(TEXT_MUTED, BG);
    tft.drawString("Display diagnostic  -  Milestone 1", 12, 42, 2);
}

// If RED shows blue -> RGB/BGR order is wrong. If BLACK shows white -> inversion is wrong.
void drawColourBars(TFT_eSPI& tft) {
    struct Swatch { uint16_t colour; const char* name; };
    const Swatch swatches[] = {
        {TFT_RED, "RED"}, {TFT_GREEN, "GREEN"}, {TFT_BLUE, "BLUE"},
        {TFT_WHITE, "WHITE"}, {TFT_BLACK, "BLACK"},
    };
    constexpr int16_t y = 66, h = 36, sw = 56, gap = 4, x0 = 12;
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TEXT_MUTED, BG);
    for (int i = 0; i < 5; ++i) {
        const int16_t x = x0 + i * (sw + gap);
        tft.fillRoundRect(x, y, sw, h, 4, swatches[i].colour);
        if (swatches[i].colour == TFT_BLACK) tft.drawRoundRect(x, y, sw, h, 4, CARD_EDGE);
        tft.drawString(swatches[i].name, x + sw / 2, y + h + 4, 2);
    }
}

void drawGreyRamp(TFT_eSPI& tft) {
    constexpr int16_t x0 = 12, y = 128, h = 20, steps = 37, stepW = 8;  // 296 px wide
    for (int i = 0; i < steps; ++i) {
        const uint8_t v = static_cast<uint8_t>(i * 255 / (steps - 1));
        tft.fillRect(x0 + i * stepW, y, stepW, h, tft.color565(v, v, v));
    }
}

void drawInfoCard(TFT_eSPI& tft) {
    tft.fillRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, RADIUS, CARD);
    tft.drawRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, RADIUS, CARD_EDGE);

    tft.setTextColor(TEXT_MUTED, CARD);
    tft.setTextDatum(TL_DATUM); tft.drawString("FULL FILL", CARD_X + 10, LABEL_Y, 2);
    tft.setTextDatum(TC_DATUM); tft.drawString("RESOLUTION", W / 2, LABEL_Y, 2);
    tft.setTextDatum(TR_DATUM); tft.drawString("UPTIME", CARD_X + CARD_W - 10, LABEL_Y, 2);

    tft.setTextColor(TEXT, CARD);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu ms", static_cast<unsigned long>(fillMs));
    tft.setTextDatum(TL_DATUM); tft.drawString(buf, CARD_X + 10, VALUE_Y, 4);
    snprintf(buf, sizeof(buf), "%dx%d", tft.width(), tft.height());
    tft.setTextDatum(TC_DATUM); tft.drawString(buf, W / 2, VALUE_Y, 4);

    tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, CARD_EDGE);
}

void updateUptime(TFT_eSPI& tft, uint32_t nowMs) {
    const uint32_t sec = nowMs / 1000;
    if (sec == lastUptimeSec) return;          // redraw only when the value changes
    lastUptimeSec = sec;
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
             static_cast<unsigned long>(sec / 3600),
             static_cast<unsigned long>((sec / 60) % 60),
             static_cast<unsigned long>(sec % 60));
    tft.setTextColor(TEXT, CARD);              // opaque background erases old glyphs
    tft.setTextDatum(TR_DATUM);
    tft.setTextPadding(tft.textWidth("00:00:00", 4));
    tft.drawString(buf, CARD_X + CARD_W - 10, VALUE_Y, 4);
    tft.setTextPadding(0);
}

// Sweeping progress bar: draws only the newly filled pixels each frame.
void updateBar(TFT_eSPI& tft, uint32_t nowMs) {
    if (nowMs - lastBarFrame < BAR_FRAME_MS) return;
    lastBarFrame = nowMs;
    const int16_t target = static_cast<int16_t>((nowMs % BAR_PERIOD_MS) * BAR_W / BAR_PERIOD_MS);
    if (target < barFilled) {                  // wrapped: clear the track
        tft.fillRect(BAR_X, BAR_Y, BAR_W, BAR_H, CARD_EDGE);
        barFilled = 0;
    }
    if (target > barFilled) {
        tft.fillRect(BAR_X + barFilled, BAR_Y, target - barFilled, BAR_H, ACCENT);
        barFilled = target;
    }
}

}  // namespace

void begin(TFT_eSPI& tft) {
    // SPI throughput benchmark (backlight still off, so this is invisible).
    constexpr int kFills = 4;
    const uint32_t t0 = millis();
    for (int i = 0; i < kFills; ++i) tft.fillScreen(i & 1 ? CARD : BG);
    fillMs = (millis() - t0) / kFills;

    tft.fillScreen(BG);
    drawCornerMarkers(tft);
    drawHeader(tft);
    drawColourBars(tft);
    drawGreyRamp(tft);
    drawInfoCard(tft);
    updateUptime(tft, millis());
}

void update(TFT_eSPI& tft, uint32_t nowMs) {
    updateUptime(tft, nowMs);
    updateBar(tft, nowMs);
}

uint32_t fillTimeMs() { return fillMs; }

}  // namespace display_test
