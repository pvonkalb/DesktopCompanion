#include "screens/settings_screen.h"

#include <Arduino.h>

#include "net/network.h"
#include "settings.h"
#include "user_prefs.h"

namespace {

using namespace theme;
using ui::Font;

enum Row { WIFI, IDLE, CALIBRATE, TOUCH_TEST };

struct RowDef {
    const char* label;
    ui::ScreenId target;   // Home = no navigation (row changes a value instead)
};

const RowDef ROWS[] = {
    {"Wi-Fi & location", ui::ScreenId::Setup},
    {"Return to Home", ui::ScreenId::Home},
    {"Recalibrate touch", ui::ScreenId::Calibration},
    {"Touch test", ui::ScreenId::TouchTest},
};

constexpr int16_t ROW_H = 38, GAP = 4, LEFT = 8, WIDTH = 304, TOP = 50;
constexpr ui::Rect FOOTER = {0, 216, 320, 24};

const char* signalWord(int rssi) {
    if (rssi >= -60) return "strong";
    if (rssi >= -72) return "good";
    return "weak";
}

}  // namespace

SettingsScreen::SettingsScreen() : buttons_(rects_, ACTIONS + 1) {
    rects_[0] = ui::HEADER_BACK;
    for (int i = 0; i < ACTIONS; ++i)
        rects_[i + 1] = {LEFT, static_cast<int16_t>(TOP + i * (ROW_H + GAP)), WIDTH, ROW_H};
}

void SettingsScreen::drawAction(TFT_eSPI& tft, int i, bool pressed) {
    const ui::Rect& r = rects_[i + 1];
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
        ui::setFont(g, Font::Body);
        ui::text(g, ROWS[i].label, 16, h / 2 + 1, ML_DATUM, TEXT, fill);

        char hint[40];
        uint16_t hintColour = pressed ? TEXT : TEXT_MUTED;
        switch (i) {
            case WIFI: snprintf(hint, sizeof(hint), "%s", net::locationName()[0] ? net::locationName() : "Not set"); break;
            case IDLE:
                snprintf(hint, sizeof(hint), "%s", user_prefs::idleReturnLabel());
                hintColour = pressed ? TEXT : ACCENT;   // a value you change by tapping
                break;
            case CALIBRATE: snprintf(hint, sizeof(hint), "5 taps"); break;
            default: snprintf(hint, sizeof(hint), "Draw"); break;
        }
        const bool navigates = ROWS[i].target != ui::ScreenId::Home;
        const int16_t hintRight = navigates ? w - 32 : w - 16;
        ui::setFont(g, i == IDLE ? Font::Body : Font::Label);
        char fitted[40];
        ui::fitText(g, hint, hintRight - 170, fitted, sizeof(fitted));
        ui::text(g, fitted, hintRight, h / 2 + 1, MR_DATUM, hintColour, fill);
        if (navigates) ui::drawChevron(g, w - 18, h / 2.0f, 5, false, pressed ? TEXT : TEXT_MUTED, fill);
    });
}

void SettingsScreen::drawInfo(TFT_eSPI& tft) {
    ui::region(tft, FOOTER.x, FOOTER.y, FOOTER.w, FOOTER.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        char wifi[56];
        switch (net::link()) {
            case net::Link::Online:
                snprintf(wifi, sizeof(wifi), "Wi-Fi %s \xC2\xB7 %s", net::ssid(), signalWord(net::rssi()));
                break;
            case net::Link::Offline: snprintf(wifi, sizeof(wifi), "Wi-Fi %s \xC2\xB7 offline", net::ssid()); break;
            case net::Link::Connecting: snprintf(wifi, sizeof(wifi), "Wi-Fi connecting\xE2\x80\xA6"); break;
            case net::Link::NotConfigured: snprintf(wifi, sizeof(wifi), "Wi-Fi not set up"); break;
        }
        ui::setFont(g, Font::Label);
        char fitted[56];
        ui::fitText(g, wifi, w - 32, fitted, sizeof(fitted));
        ui::text(g, fitted, w / 2, h / 2, MC_DATUM, TEXT_MUTED, BG);
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
    if (hit == 0) {
        ui::nav::back();
    } else if (hit - 1 == IDLE) {
        user_prefs::cycleIdleReturn();
        drawAction(tft, IDLE, false);
    } else if (hit > 0) {
        ui::nav::go(ROWS[hit - 1].target);
    }
}
