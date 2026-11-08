#include "screens/music_screen.h"

#include "model.h"
#include "pc_link.h"

namespace {

using namespace theme;
using ui::Font;

constexpr ui::Rect INFO      = {0, 46, 320, 86};
constexpr ui::Rect APP_TAG   = {200, 0, 116, 44};
constexpr ui::Rect PREV_R    = {72, 136, 48, 48};
constexpr ui::Rect PLAY_R    = {128, 128, 64, 64};
constexpr ui::Rect NEXT_R    = {200, 136, 48, 48};
constexpr ui::Rect MUTE_R    = {10, 200, 46, 34};
constexpr ui::Rect DOWN_R    = {62, 200, 46, 34};
constexpr ui::Rect VOLBAR    = {114, 200, 144, 34};
constexpr ui::Rect UP_R      = {264, 200, 46, 34};
constexpr int8_t   VOL_STEP  = 4;

const model::Media& media() { return model::state().media; }

void prevNextGlyph(TFT_eSPI& g, float cx, float cy, bool next, uint16_t c) {
    const float d = next ? 1 : -1;
    g.fillTriangle(cx - 7 * d, cy - 8, cx - 7 * d, cy + 8, cx + 5 * d, cy, c);
    g.fillRect(next ? cx + 5 : cx - 7, cy - 8, 3, 17, c);
}

void speakerGlyph(TFT_eSPI& g, float cx, float cy, bool muted, uint16_t c, uint16_t bg) {
    g.fillRect(cx - 9, cy - 4, 5, 9, c);
    g.fillTriangle(cx - 5, cy, cx + 2, cy - 8, cx + 2, cy + 8, c);
    g.fillRect(cx - 5, cy - 4, 7, 9, c);
    if (muted) {
        g.drawWideLine(cx + 5, cy - 5, cx + 11, cy + 5, 2, STATUS_ERROR, bg);
        g.drawWideLine(cx + 5, cy + 5, cx + 11, cy - 5, 2, STATUS_ERROR, bg);
    } else {
        g.drawSmoothArc(cx + 2, cy, 10, 8, 225, 315, c, bg, true);   // small sound wave (right side)
    }
}

void minusPlus(TFT_eSPI& g, float cx, float cy, bool plus, uint16_t c, uint16_t bg) {
    g.drawWideLine(cx - 7, cy, cx + 7, cy, 2.5f, c, bg);
    if (plus) g.drawWideLine(cx, cy - 7, cx, cy + 7, 2.5f, c, bg);
}

}  // namespace

MusicScreen::MusicScreen() : buttons_(rects_, COUNT) {
    rects_[BACK] = ui::HEADER_BACK;
    rects_[PREV] = PREV_R;
    rects_[PLAY] = PLAY_R;
    rects_[NEXT] = NEXT_R;
    rects_[MUTE] = MUTE_R;
    rects_[VOL_DOWN] = DOWN_R;
    rects_[VOL_UP] = UP_R;
}

bool MusicScreen::available() const { return pc_link::connected() && media().valid; }

bool MusicScreen::idleReturnsHome() const { return !pc_link::connected(); }

void MusicScreen::drawTarget(TFT_eSPI& tft, int i, bool pressed) {
    if (i == BACK) { ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed); return; }
    const ui::Rect& r = rects_[i];
    const bool on = available();
    const bool playing = media().state == 1;
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        const float cx = w / 2.0f, cy = h / 2.0f;
        const uint16_t fg = on ? TEXT : CARD_EDGE;
        if (i == PLAY) {
            const uint16_t fill = pressed ? ACCENT_ALT : (on ? ACCENT : theme::CARD);
            g.fillSmoothCircle(cx, cy, w / 2 - 1, fill, BG);
            const uint16_t c = on && !pressed ? BG : TEXT;
            if (playing) {
                g.fillRect(cx - 8, cy - 11, 6, 22, c);
                g.fillRect(cx + 2, cy - 11, 6, 22, c);
            } else {
                g.fillTriangle(cx - 6, cy - 12, cx - 6, cy + 12, cx + 12, cy, c);
            }
        } else if (i == PREV || i == NEXT) {
            const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
            g.fillSmoothCircle(cx, cy, w / 2 - 1, fill, BG);
            prevNextGlyph(g, cx, cy, i == NEXT, fg);
        } else {
            const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
            g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
            if (i == MUTE) speakerGlyph(g, cx - 1, cy, media().muted, fg, fill);
            else minusPlus(g, cx, cy, i == VOL_UP, fg, fill);
        }
    });
}

void MusicScreen::drawInfo(TFT_eSPI& tft) {
    const model::Media& m = media();
    ui::region(tft, APP_TAG.x, APP_TAG.y, APP_TAG.w, APP_TAG.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        if (!available() || !m.app[0]) return;
        ui::setFont(g, Font::Label);
        char buf[24];
        ui::fitText(g, m.app, w - 4, buf, sizeof(buf));
        ui::text(g, buf, w - 2, h / 2 + 1, MR_DATUM, TEXT_MUTED, BG);
    });
    ui::region(tft, INFO.x, INFO.y, INFO.w, INFO.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        constexpr int16_t X = 16;
        if (!pc_link::connected()) {
            ui::setFont(g, Font::Title);
            ui::text(g, "PC not connected", X, 30, L_BASELINE, TEXT, BG);
            ui::setFont(g, Font::Body);
            ui::text(g, "Start the Desk Companion app on your PC", X, 56, L_BASELINE, TEXT_MUTED, BG);
            return;
        }
        if (!m.valid || (!m.title[0] && m.state == 0)) {
            ui::setFont(g, Font::Title);
            ui::text(g, "Nothing playing", X, 30, L_BASELINE, TEXT, BG);
            ui::setFont(g, Font::Body);
            ui::text(g, "Play something on your PC", X, 56, L_BASELINE, TEXT_MUTED, BG);
            return;
        }
        char l1[64], l2[64], artist[48];
        ui::setFont(g, Font::Title);
        const int lines = ui::wrap2(g, m.title, w - 2 * X, l1, l2, sizeof(l1));
        ui::text(g, l1, X, 24, L_BASELINE, TEXT, BG);
        if (lines > 1) ui::text(g, l2, X, 48, L_BASELINE, TEXT, BG);
        ui::setFont(g, Font::Body);
        ui::fitText(g, m.artist, w - 2 * X, artist, sizeof(artist));
        ui::text(g, artist, X, lines > 1 ? 72 : 48, L_BASELINE, TEXT_MUTED, BG);
    });
}

void MusicScreen::drawVolume(TFT_eSPI& tft) {
    const model::Media& m = media();
    ui::region(tft, VOLBAR.x, VOLBAR.y, VOLBAR.w, VOLBAR.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        if (!available() || m.volume < 0) return;
        constexpr int16_t BAR_W = 96;
        const int16_t y = h / 2 - 2;
        g.fillSmoothRoundRect(4, y, BAR_W, 5, 2, CARD_EDGE, BG);
        const int16_t fill = m.volume * BAR_W / 100;
        if (fill > 3) g.fillSmoothRoundRect(4, y, fill, 5, 2, m.muted ? TEXT_MUTED : ACCENT, CARD_EDGE);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", m.volume);
        ui::setFont(g, Font::Label);
        ui::text(g, buf, w - 4, h / 2 + 1, MR_DATUM, m.muted ? TEXT_MUTED : TEXT, BG);
    });
}

void MusicScreen::drawAll(TFT_eSPI& tft) {
    buttons_.reset();
    mediaVersion_ = media().version;
    linkKey_ = (pc_link::connected() ? 1 : 0) | (media().valid ? 2 : 0);
    drawInfo(tft);
    for (int i = PREV; i < COUNT; ++i) drawTarget(tft, i, false);
    drawVolume(tft);
}

void MusicScreen::enter(TFT_eSPI& tft, uint32_t) {
    tft.fillScreen(BG);
    ui::drawChevronButton(tft, ui::HEADER_BACK, true, false);
    ui::drawHeaderTitle(tft, "Music", APP_TAG.x - 4);
    drawAll(tft);
}

void MusicScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    const int hit = buttons_.handle(ev, [&](int i, bool pressed) { drawTarget(tft, i, pressed); });
    if (hit < 0) return;
    if (hit == BACK) { ui::nav::back(); return; }
    if (!pc_link::connected()) return;

    model::Media& m = model::state().media;   // optimistic update; the PC confirms within ~1 s
    switch (hit) {
        case PLAY: pc_link::command("playpause"); if (m.state) m.state = m.state == 1 ? 2 : 1; break;
        case PREV: pc_link::command("prev"); break;
        case NEXT: pc_link::command("next"); break;
        case MUTE: pc_link::command("mute"); m.muted = !m.muted; break;
        case VOL_DOWN: pc_link::command("vol_down"); if (m.volume >= 0) m.volume = max(0, m.volume - VOL_STEP); break;
        case VOL_UP:
            pc_link::command("vol_up");
            if (m.volume >= 0) m.volume = min(100, m.volume + VOL_STEP);
            m.muted = false;
            break;
    }
    ++m.version;
}

void MusicScreen::update(TFT_eSPI& tft, uint32_t now) {
    if (buttons_.pressed() >= 0 || now - lastCheck_ < 150) return;
    lastCheck_ = now;
    const int key = (pc_link::connected() ? 1 : 0) | (media().valid ? 2 : 0);
    if (key != linkKey_ || media().version != mediaVersion_) drawAll(tft);
}
