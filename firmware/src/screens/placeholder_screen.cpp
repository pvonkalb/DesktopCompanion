#include "screens/placeholder_screen.h"

void PlaceholderScreen::enter(TFT_eSPI& tft, uint32_t) {
    using namespace theme;
    buttons_.reset();
    tft.fillScreen(BG);
    ui::drawHeader(tft, title_, false);
    ui::region(tft, 0, 100, 320, 60, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        ui::setFont(g, ui::Font::Body);
        ui::text(g, "Coming soon", w / 2, 18, MC_DATUM, TEXT, BG);
        ui::setFont(g, ui::Font::Label);
        ui::text(g, note_, w / 2, 42, MC_DATUM, TEXT_MUTED, BG);
    });
}

void PlaceholderScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    const int hit = buttons_.handle(ev, [&](int, bool pressed) {
        ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed);
    });
    if (hit == 0) ui::nav::back();
}
