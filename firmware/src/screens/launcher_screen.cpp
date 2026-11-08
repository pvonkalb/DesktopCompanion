#include "screens/launcher_screen.h"

#include "ui/icons.h"

namespace {

using namespace theme;

struct App {
    const char* label;
    ui::AppIcon icon;
    ui::ScreenId screen;
};

const App APPS_LIST[] = {
    {"Calendar", ui::AppIcon::Calendar, ui::ScreenId::Calendar},
    {"Weather", ui::AppIcon::Weather, ui::ScreenId::Weather},
    {"Focus", ui::AppIcon::Focus, ui::ScreenId::Focus},
    {"Timer", ui::AppIcon::Timer, ui::ScreenId::Timer},
    {"Music", ui::AppIcon::Music, ui::ScreenId::Music},
    {"PC", ui::AppIcon::Pc, ui::ScreenId::PcStats},
    {"Settings", ui::AppIcon::Settings, ui::ScreenId::Settings},
};

constexpr int16_t TILE_W = 70, TILE_H = 88, GAP = 8, TOP = 52, LEFT = 8;
constexpr int16_t TILE_RADIUS = 12;

}  // namespace

LauncherScreen::LauncherScreen() : buttons_(rects_, APPS + 1) {
    rects_[0] = ui::HEADER_BACK;
    for (int i = 0; i < APPS; ++i) {
        const int row = i / 4, col = i % 4;
        const int16_t rowOffset = row == 0 ? 0 : (TILE_W + GAP) / 2;   // centre the 3-tile row
        rects_[i + 1] = {static_cast<int16_t>(LEFT + rowOffset + col * (TILE_W + GAP)),
                         static_cast<int16_t>(TOP + row * (TILE_H + GAP)), TILE_W, TILE_H};
    }
}

void LauncherScreen::drawTile(TFT_eSPI& tft, int app, bool pressed) {
    const ui::Rect& r = rects_[app + 1];
    const App& a = APPS_LIST[app];
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, TILE_RADIUS, pressed ? ACCENT : CARD_EDGE, BG);
        g.fillSmoothRoundRect(1, 1, w - 2, h - 2, TILE_RADIUS - 1, fill, pressed ? ACCENT : CARD_EDGE);
        ui::drawAppIcon(g, w / 2, 36, a.icon, fill);
        ui::setFont(g, ui::Font::Label);
        ui::text(g, a.label, w / 2, h - 14, C_BASELINE, TEXT, fill);
    });
}

void LauncherScreen::enter(TFT_eSPI& tft, uint32_t) {
    buttons_.reset();
    tft.fillScreen(BG);
    ui::drawHeader(tft, "Apps", false);
    for (int i = 0; i < APPS; ++i) drawTile(tft, i, false);
}

void LauncherScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    const int hit = buttons_.handle(ev, [&](int i, bool pressed) {
        if (i == 0) ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed);
        else drawTile(tft, i - 1, pressed);
    });
    if (hit == 0) ui::nav::back();
    else if (hit > 0) ui::nav::go(APPS_LIST[hit - 1].screen);
}
