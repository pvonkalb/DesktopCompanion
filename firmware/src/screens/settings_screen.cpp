#include "screens/settings_screen.h"

#include <Arduino.h>

#include "settings.h"

namespace {

using namespace theme;
using ui::Font;

struct Action {
    const char* label;
    const char* hint;
    ui::ScreenId target;
};

const Action ACTION_LIST[] = {
    {"Recalibrate touch", "5 taps", ui::ScreenId::Calibration},
    {"Touch test", "Draw + accuracy", ui::ScreenId::TouchTest},
};

constexpr int16_t ROW_H = 44, LEFT = 8, WIDTH = 304, ACTIONS_TOP = 52;
constexpr ui::Rect INFO = {8, 152, 304, 82};

}  // namespace

SettingsScreen::SettingsScreen() : buttons_(rects_, ACTIONS + 1) {
    rects_[0] = ui::HEADER_BACK;
    for (int i = 0; i < ACTIONS; ++i)
        rects_[i + 1] = {LEFT, static_cast<int16_t>(ACTIONS_TOP + i * (ROW_H + 4)), WIDTH, ROW_H};
}

void SettingsScreen::drawAction(TFT_eSPI& tft, int i, bool pressed) {
    const ui::Rect& r = rects_[i + 1];
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
        ui::setFont(g, Font::Body);
        ui::text(g, ACTION_LIST[i].label, 16, h / 2 + 1, ML_DATUM, TEXT, fill);
        ui::setFont(g, Font::Label);
        ui::text(g, ACTION_LIST[i].hint, w - 32, h / 2 + 1, MR_DATUM, pressed ? TEXT : TEXT_MUTED, fill);
        ui::drawChevron(g, w - 18, h / 2.0f, 5, false, pressed ? TEXT : TEXT_MUTED, fill);
    });
}

void SettingsScreen::drawInfo(TFT_eSPI& tft) {
    ui::region(tft, INFO.x, INFO.y, INFO.w, INFO.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, CARD_EDGE, BG);
        g.fillSmoothRoundRect(1, 1, w - 2, h - 2, 9, theme::CARD, CARD_EDGE);
        char units[32], mem[24];
        snprintf(units, sizeof(units), "%s \xC2\xB7 %s", settings::FAHRENHEIT ? "\xC2\xB0" "F" : "\xC2\xB0" "C",
                 settings::USE_24H ? "24-hour" : "12-hour");
        snprintf(mem, sizeof(mem), "%lu KB free", static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
        const char* rows[][2] = {{"Units", units}, {"Firmware", __DATE__}, {"Memory", mem}};
        for (int i = 0; i < 3; ++i) {
            const int16_t y = 22 + i * 25;
            ui::setFont(g, Font::Label);
            ui::text(g, rows[i][0], 16, y, L_BASELINE, TEXT_MUTED, theme::CARD);
            ui::setFont(g, Font::Body);
            ui::text(g, rows[i][1], w - 16, y, R_BASELINE, TEXT, theme::CARD);
        }
        (void)h;
    });
}

void SettingsScreen::enter(TFT_eSPI& tft, uint32_t) {
    buttons_.reset();
    tft.fillScreen(BG);
    ui::drawHeader(tft, "Settings", false);
    for (int i = 0; i < ACTIONS; ++i) drawAction(tft, i, false);
    drawInfo(tft);
}

void SettingsScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    const int hit = buttons_.handle(ev, [&](int i, bool pressed) {
        if (i == 0) ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed);
        else drawAction(tft, i - 1, pressed);
    });
    if (hit == 0) ui::nav::back();
    else if (hit > 0) ui::nav::go(ACTION_LIST[hit - 1].target);
}
