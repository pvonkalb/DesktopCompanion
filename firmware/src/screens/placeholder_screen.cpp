#include "screens/placeholder_screen.h"

namespace {

using namespace theme;
constexpr ui::Rect BACK = {8, 8, 104, 40};

}  // namespace

void PlaceholderScreen::drawBack(TFT_eSPI& tft, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    ui::region(tft, BACK.x, BACK.y, BACK.w, BACK.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
        // chevron
        g.drawWideLine(24, 13, 17, 20, 2.5f, TEXT, fill);
        g.drawWideLine(17, 20, 24, 27, 2.5f, TEXT, fill);
        ui::setFont(g, ui::Font::Body);
        ui::text(g, "Home", 34, h / 2 + 1, ML_DATUM, TEXT, fill);
    });
}

void PlaceholderScreen::enter(TFT_eSPI& tft, uint32_t) {
    backPressed_ = false;
    tft.fillScreen(BG);
    drawBack(tft, false);
    ui::region(tft, 0, 80, 320, 90, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        ui::setFont(g, ui::Font::Title);
        ui::text(g, title_, w / 2, 30, MC_DATUM, TEXT, BG);
        ui::setFont(g, ui::Font::Body);
        ui::text(g, note_, w / 2, 62, MC_DATUM, TEXT_MUTED, BG);
    });
}

void PlaceholderScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    using touch_input::EventType;
    if (ev.type == EventType::Press && BACK.hit(ev.x, ev.y)) {
        backPressed_ = true;
        drawBack(tft, true);
    } else if (ev.type == EventType::Move && backPressed_ && !BACK.hit(ev.x, ev.y)) {
        backPressed_ = false;
        drawBack(tft, false);
    } else if (ev.type == EventType::Release && backPressed_) {
        backPressed_ = false;
        drawBack(tft, false);
        if (BACK.hit(ev.x, ev.y)) ui::nav::go(ui::ScreenId::Home);
    }
}
