#include "screens/settings_screen.h"

#include <Arduino.h>

#include "net/network.h"
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
    {"Wi-Fi & location", "Phone setup", ui::ScreenId::Setup},
    {"Recalibrate touch", "5 taps", ui::ScreenId::Calibration},
    {"Touch test", "Draw", ui::ScreenId::TouchTest},
};

constexpr int16_t ROW_H = 40, GAP = 4, LEFT = 8, WIDTH = 304, ACTIONS_TOP = 50;
constexpr ui::Rect INFO = {8, 184, 304, 52};

const char* signalWord(int rssi) {
    if (rssi >= -60) return "strong";
    if (rssi >= -72) return "good";
    return "weak";
}

}  // namespace

SettingsScreen::SettingsScreen() : buttons_(rects_, ACTIONS + 1) {
    rects_[0] = ui::HEADER_BACK;
    for (int i = 0; i < ACTIONS; ++i)
        rects_[i + 1] = {LEFT, static_cast<int16_t>(ACTIONS_TOP + i * (ROW_H + GAP)), WIDTH, ROW_H};
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

        char wifi[48], loc[48];
        switch (net::link()) {
            case net::Link::Online:
                snprintf(wifi, sizeof(wifi), "%s \xC2\xB7 %s", net::ssid(), signalWord(net::rssi()));
                break;
            case net::Link::Offline: snprintf(wifi, sizeof(wifi), "%s \xC2\xB7 offline", net::ssid()); break;
            case net::Link::Connecting: snprintf(wifi, sizeof(wifi), "Connecting\xE2\x80\xA6"); break;
            case net::Link::NotConfigured: snprintf(wifi, sizeof(wifi), "Not set up"); break;
        }
        snprintf(loc, sizeof(loc), "%s", net::locationName()[0] ? net::locationName() : "Not set");

        const char* rows[][2] = {{"Location", loc}, {"Wi-Fi", wifi}};
        for (int i = 0; i < 2; ++i) {
            const int16_t y = 21 + i * 21;
            ui::setFont(g, Font::Label);
            ui::text(g, rows[i][0], 16, y, L_BASELINE, TEXT_MUTED, theme::CARD);
            ui::setFont(g, Font::Body);
            char buf[48];
            ui::fitText(g, rows[i][1], w - 16 - 90, buf, sizeof(buf));
            ui::text(g, buf, w - 16, y, R_BASELINE, TEXT, theme::CARD);
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
