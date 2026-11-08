#include "screens/pc_screen.h"

#include "model.h"
#include "pc_link.h"

namespace {

using namespace theme;
using ui::Font;

constexpr int16_t TILE_W = 148, TILE_H = 88, X0 = 8, Y0 = 50, GAP_X = 8, GAP_Y = 6;
constexpr int8_t  HOT = 90;   // % at which a bar turns amber

ui::Rect tileRect(int i) {
    return {static_cast<int16_t>(X0 + (i % 2) * (TILE_W + GAP_X)), static_cast<int16_t>(Y0 + (i / 2) * (TILE_H + GAP_Y)),
            TILE_W, TILE_H};
}

void formatRate(uint32_t bps, char* out, size_t len) {
    if (bps >= 1000000) snprintf(out, len, "%.1f MB/s", bps / 1e6f);
    else if (bps >= 1000) snprintf(out, len, "%lu KB/s", static_cast<unsigned long>(bps / 1000));
    else snprintf(out, len, "%lu B/s", static_cast<unsigned long>(bps));
}

void arrow(TFT_eSPI& g, float x, float y, bool up, uint16_t c) {
    if (up) g.fillTriangle(x, y - 6, x - 6, y + 4, x + 6, y + 4, c);
    else g.fillTriangle(x, y + 6, x - 6, y - 4, x + 6, y - 4, c);
}

}  // namespace

PcScreen::PcScreen() : buttons_(back_, 1) { back_[0] = ui::HEADER_BACK; }

bool PcScreen::idleReturnsHome() const { return !pc_link::connected(); }

void PcScreen::drawTile(TFT_eSPI& tft, int index) {
    const model::PcStats& s = model::state().pc;
    const ui::Rect r = tileRect(index);
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, theme::CARD, BG);
        static const char* const LABELS[] = {"CPU", "GPU", "MEMORY", "NETWORK"};
        ui::setFont(g, Font::Label);
        ui::text(g, LABELS[index], 12, 20, L_BASELINE, TEXT_MUTED, theme::CARD);

        char buf[24];
        if (index == 3) {   // network: two rows
            ui::setFont(g, Font::Body);
            arrow(g, 18, 45, false, ACCENT);
            formatRate(s.downBps, buf, sizeof(buf));
            ui::text(g, buf, 32, 50, L_BASELINE, TEXT, theme::CARD);
            arrow(g, 18, 70, true, STATUS_OK);
            formatRate(s.upBps, buf, sizeof(buf));
            ui::text(g, buf, 32, 75, L_BASELINE, TEXT, theme::CARD);
            return;
        }

        const int8_t pct = index == 0 ? s.cpu : index == 1 ? s.gpu : s.ram;
        if (index == 1 && s.gpuTemp >= 0) {
            snprintf(buf, sizeof(buf), "%d\xC2\xB0" "C", s.gpuTemp);
            ui::text(g, buf, w - 12, 20, R_BASELINE, s.gpuTemp >= 80 ? STATUS_WARN : TEXT_MUTED, theme::CARD);
        }
        if (index == 2 && s.ramTotalGb) {
            snprintf(buf, sizeof(buf), "%.0f / %u GB", s.ramUsedGb, s.ramTotalGb);
            ui::text(g, buf, w - 12, 20, R_BASELINE, TEXT_MUTED, theme::CARD);
        }
        ui::setFont(g, Font::Temp);
        if (pct < 0) snprintf(buf, sizeof(buf), "--");
        else snprintf(buf, sizeof(buf), "%d%%", pct);
        ui::text(g, buf, 12, 60, L_BASELINE, pct < 0 ? TEXT_MUTED : TEXT, theme::CARD);

        constexpr int16_t BAR_X = 12, BAR_Y = 72, BAR_W = TILE_W - 24;
        g.fillSmoothRoundRect(BAR_X, BAR_Y, BAR_W, 6, 3, CARD_EDGE, theme::CARD);
        if (pct > 2)
            g.fillSmoothRoundRect(BAR_X, BAR_Y, pct * BAR_W / 100, 6, 3, pct >= HOT ? STATUS_WARN : ACCENT, CARD_EDGE);
        (void)h;
    });
}

void PcScreen::drawTiles(TFT_eSPI& tft) {
    version_ = model::state().pc.version;
    linkKey_ = (pc_link::connected() ? 1 : 0) | (model::state().pc.valid ? 2 : 0);
    if (linkKey_ != 3) {
        tft.fillRect(0, ui::HEADER_H, 320, 240 - ui::HEADER_H, BG);
        ui::region(tft, 0, 100, 320, 60, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
            ui::setFont(g, Font::Title);
            ui::text(g, pc_link::connected() ? "Waiting for PC stats" : "PC not connected", w / 2, 18, MC_DATUM, TEXT, BG);
            ui::setFont(g, Font::Body);
            ui::text(g, "Start the Desk Companion app on your PC", w / 2, 44, MC_DATUM, TEXT_MUTED, BG);
        });
        return;
    }
    for (int i = 0; i < 4; ++i) drawTile(tft, i);
}

void PcScreen::enter(TFT_eSPI& tft, uint32_t) {
    buttons_.reset();
    tft.fillScreen(BG);
    ui::drawHeader(tft, "PC", false);
    linkKey_ = -1;
    drawTiles(tft);
}

void PcScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    const int hit = buttons_.handle(ev, [&](int, bool pressed) { ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed); });
    if (hit == 0) ui::nav::back();
}

void PcScreen::update(TFT_eSPI& tft, uint32_t now) {
    if (buttons_.pressed() >= 0 || now - lastCheck_ < 200) return;
    lastCheck_ = now;
    const int key = (pc_link::connected() ? 1 : 0) | (model::state().pc.valid ? 2 : 0);
    if (key != linkKey_) {
        if (linkKey_ != 3 && key == 3) tft.fillRect(0, ui::HEADER_H, 320, 240 - ui::HEADER_H, BG);
        drawTiles(tft);
    } else if (key == 3 && model::state().pc.version != version_) {
        drawTiles(tft);
    }
}
