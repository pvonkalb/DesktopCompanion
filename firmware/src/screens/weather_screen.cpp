#include "screens/weather_screen.h"

#include <math.h>
#include <stdlib.h>

#include "model.h"
#include "timefmt.h"
#include "ui/icons.h"

namespace {

using namespace theme;
using ui::Font;

enum Target { BACK = 0, TAB_HOURLY = 1, TAB_DAILY = 2, CONTENT = 3 };

constexpr ui::Rect TABS        = {58, 6, 256, 32};   // no title: tabs fill the header
constexpr ui::Rect SUMMARY     = {0, 46, 320, 44};
constexpr ui::Rect HOURLY_CARD = {8, 92, 304, 144};
constexpr ui::Rect DAILY_CARD  = {8, 48, 304, 188};
constexpr int      PER_PAGE    = 6;
constexpr int16_t  SWIPE_PX    = 60;
constexpr uint8_t  RAIN_NOTABLE = 30;   // % at which rain is highlighted
const char* const  TAB_LABELS[] = {"Hourly", "7 Day"};
const char DEGREE[] = "\xC2\xB0";

void card(TFT_eSPI& g, int16_t w, int16_t h) {
    g.fillSmoothRoundRect(0, 0, w, h, 10, CARD_EDGE, BG);
    g.fillSmoothRoundRect(1, 1, w - 2, h - 2, 9, theme::CARD, CARD_EDGE);
}

void temp(char* out, size_t len, float c) { snprintf(out, len, "%d%s", ui::roundTemp(c), DEGREE); }

int hourlyPages() {
    const int n = model::state().weather.hourlyCount;
    return n <= PER_PAGE ? 1 : (n + PER_PAGE - 1) / PER_PAGE;
}

}  // namespace

WeatherScreen::WeatherScreen() : buttons_(rects_, TARGETS) {
    rects_[BACK] = ui::HEADER_BACK;
    rects_[TAB_HOURLY] = ui::segmentRect(TABS, 2, 0);
    rects_[TAB_DAILY] = ui::segmentRect(TABS, 2, 1);
    rects_[CONTENT] = HOURLY_CARD;   // only paging in the hourly view
}

void WeatherScreen::enter(TFT_eSPI& tft, uint32_t) {
    buttons_.reset();
    tab_ = 0;
    page_ = 0;
    tft.fillScreen(BG);
    ui::drawChevronButton(tft, ui::HEADER_BACK, true, false);
    drawTabs(tft);
    drawContent(tft);
}

void WeatherScreen::drawTabs(TFT_eSPI& tft, int pressed) {
    ui::drawSegmented(tft, TABS, TAB_LABELS, 2, tab_, pressed);
}

void WeatherScreen::drawContent(TFT_eSPI& tft) {
    weatherVersion_ = model::state().weather.version;
    tft.fillRect(0, ui::HEADER_H, 320, 240 - ui::HEADER_H, BG);
    if (tab_ == 0) {
        drawSummary(tft);
        drawHourly(tft);
    } else {
        drawDaily(tft);
    }
}

void WeatherScreen::drawSummary(TFT_eSPI& tft) {
    const model::Weather& w = model::state().weather;
    ui::region(tft, SUMMARY.x, SUMMARY.y, SUMMARY.w, SUMMARY.h, BG, [&](TFT_eSPI& g, int16_t width, int16_t) {
        if (w.source == model::Source::None) return;
        char buf[24];
        ui::drawWeatherIcon(g, 26, 22, 32, w.code, w.isDay, BG);
        temp(buf, sizeof(buf), w.tempC);
        ui::setFont(g, Font::Title);
        ui::text(g, buf, 48, 30, L_BASELINE, TEXT, BG);
        const int16_t condX = 48 + g.textWidth(buf) + 8;

        snprintf(buf, sizeof(buf), "H %d%s  L %d%s", ui::roundTemp(w.highC), DEGREE, ui::roundTemp(w.lowC), DEGREE);
        ui::setFont(g, Font::Label);
        const int16_t hlW = g.textWidth(buf);
        ui::text(g, buf, width - 10, 30, R_BASELINE, TEXT_MUTED, BG);

        ui::setFont(g, Font::Body);
        char cond[32];
        ui::fitText(g, ui::weatherLabel(w.code), width - 10 - hlW - 10 - condX, cond, sizeof(cond));
        ui::text(g, cond, condX, 30, L_BASELINE, TEXT_MUTED, BG);
    });
}

void WeatherScreen::drawHourly(TFT_eSPI& tft) {
    const model::Weather& w = model::state().weather;
    const int pages = hourlyPages();
    if (page_ >= pages) page_ = 0;

    ui::region(tft, HOURLY_CARD.x, HOURLY_CARD.y, HOURLY_CARD.w, HOURLY_CARD.h, BG,
               [&](TFT_eSPI& g, int16_t width, int16_t h) {
        card(g, width, h);
        if (w.hourlyCount == 0) {
            ui::setFont(g, Font::Body);
            ui::text(g, "No forecast yet", width / 2, h / 2, MC_DATUM, TEXT_MUTED, theme::CARD);
            return;
        }
        const int16_t colW = (width - 4) / PER_PAGE;
        const time_t now = time(nullptr);
        for (int c = 0; c < PER_PAGE; ++c) {
            const int i = page_ * PER_PAGE + c;
            if (i >= w.hourlyCount) break;
            const model::HourForecast& f = w.hourly[i];
            const int16_t cx = 2 + c * colW + colW / 2;
            char buf[12];

            const bool isNow = now >= f.time && now < f.time + 3600;
            if (isNow) snprintf(buf, sizeof(buf), "Now");
            else timefmt::hourLabel(f.time, buf, sizeof(buf));
            ui::setFont(g, Font::Label);
            ui::text(g, buf, cx, 22, C_BASELINE, isNow ? ACCENT : TEXT_MUTED, theme::CARD);

            ui::drawWeatherIcon(g, cx, 50, 30, f.code, f.isDay, theme::CARD);

            temp(buf, sizeof(buf), f.tempC);
            ui::setFont(g, Font::Body);
            ui::text(g, buf, cx, 90, C_BASELINE, TEXT, theme::CARD);

            snprintf(buf, sizeof(buf), "%u%%", f.rainChance);
            ui::setFont(g, Font::Label);
            ui::text(g, buf, cx, 112, C_BASELINE, f.rainChance >= RAIN_NOTABLE ? ACCENT : TEXT_MUTED,
                     theme::CARD);
        }
        if (pages > 1) {   // page dots
            for (int p = 0; p < pages; ++p)
                g.fillSmoothCircle(width / 2 + (p - (pages - 1) / 2.0f) * 12, h - 13, 3,
                                   p == page_ ? ACCENT : CARD_EDGE, theme::CARD);
        }
    });
}

void WeatherScreen::drawDaily(TFT_eSPI& tft) {
    const model::Weather& w = model::state().weather;
    const ui::Rect& c = DAILY_CARD;
    // Frame drawn directly; rows rendered individually (a full-card sprite would
    // need ~114 KB contiguous RAM, which the ESP32 heap may not have).
    tft.fillSmoothRoundRect(c.x, c.y, c.w, c.h, 10, CARD_EDGE, BG);
    tft.fillSmoothRoundRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2, 9, theme::CARD, CARD_EDGE);
    if (w.dailyCount == 0) {
        ui::region(tft, c.x + 10, c.y + c.h / 2 - 12, c.w - 20, 24, theme::CARD, [&](TFT_eSPI& g, int16_t width, int16_t h) {
            ui::setFont(g, Font::Body);
            ui::text(g, "No forecast yet", width / 2, h / 2, MC_DATUM, TEXT_MUTED, theme::CARD);
        });
        return;
    }
    float weekLo = w.daily[0].lowC, weekHi = w.daily[0].highC;
    for (int i = 1; i < w.dailyCount; ++i) {
        weekLo = fminf(weekLo, w.daily[i].lowC);
        weekHi = fmaxf(weekHi, w.daily[i].highC);
    }
    const float span = fmaxf(1.0f, weekHi - weekLo);

    constexpr int16_t ROW_H = 26, TOP = 3, INSET = 2;
    constexpr int16_t BAR_X = 178 - INSET, BAR_W = 76;
    const time_t today = timefmt::dayStart(time(nullptr));
    for (int i = 0; i < w.dailyCount; ++i) {
        const model::DayForecast& d = w.daily[i];
        ui::region(tft, c.x + INSET, c.y + TOP + i * ROW_H, c.w - 2 * INSET, ROW_H, theme::CARD,
                   [&](TFT_eSPI& g, int16_t width, int16_t) {
            constexpr int16_t base = 18, mid = 13;
            char buf[12];
            struct tm lt;
            localtime_r(&d.date, &lt);
            ui::setFont(g, Font::Body);
            ui::text(g, d.date == today ? "Today" : timefmt::weekdayTitle(lt.tm_wday), 12 - INSET, base,
                     L_BASELINE, d.date == today ? ACCENT : TEXT, theme::CARD);

            ui::drawWeatherIcon(g, 80 - INSET, mid, 22, d.code, true, theme::CARD);

            if (d.rainChance >= 10) {
                snprintf(buf, sizeof(buf), "%u%%", d.rainChance);
                ui::setFont(g, Font::Label);
                ui::text(g, buf, 100 - INSET, base - 1, L_BASELINE,
                         d.rainChance >= RAIN_NOTABLE ? ACCENT : TEXT_MUTED, theme::CARD);
            }

            ui::setFont(g, Font::Body);
            temp(buf, sizeof(buf), d.lowC);
            ui::text(g, buf, BAR_X - 8, base, R_BASELINE, TEXT_MUTED, theme::CARD);
            temp(buf, sizeof(buf), d.highC);
            ui::text(g, buf, width - 12 + INSET, base, R_BASELINE, TEXT, theme::CARD);

            g.fillSmoothRoundRect(BAR_X, mid - 2, BAR_W, 5, 2, CARD_EDGE, theme::CARD);
            const int16_t x0 = BAR_X + static_cast<int16_t>((d.lowC - weekLo) / span * BAR_W);
            const int16_t x1 = BAR_X + static_cast<int16_t>((d.highC - weekLo) / span * BAR_W);
            g.fillSmoothRoundRect(x0, mid - 2, max<int16_t>(5, x1 - x0), 5, 2, ACCENT, CARD_EDGE);
        });
    }
}

void WeatherScreen::drawTarget(TFT_eSPI& tft, int index, bool pressed) {
    switch (index) {
        case BACK: ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed); break;
        case TAB_HOURLY:
        case TAB_DAILY: drawTabs(tft, pressed ? index - TAB_HOURLY : -1); break;
        default: break;   // content paging has no pressed visual
    }
}

void WeatherScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    using touch_input::EventType;
    if (ev.type == EventType::Press) { pressX_ = ev.x; pressY_ = ev.y; }

    if (ev.type == EventType::Release && tab_ == 0 && hourlyPages() > 1) {   // swipe pages the hourly view
        const int dx = ev.x - pressX_, dy = ev.y - pressY_;
        if (abs(dx) >= SWIPE_PX && abs(dx) > 2 * abs(dy) && pressY_ >= ui::HEADER_H) {
            const int p = buttons_.pressed();
            buttons_.reset();
            if (p >= 0) drawTarget(tft, p, false);
            const int pages = hourlyPages();
            page_ = (page_ + (dx < 0 ? 1 : pages - 1)) % pages;
            drawHourly(tft);
            return;
        }
    }

    const int hit = buttons_.handle(ev, [&](int i, bool pressed) { drawTarget(tft, i, pressed); });
    if (hit == BACK) {
        ui::nav::back();
    } else if (hit == TAB_HOURLY || hit == TAB_DAILY) {
        const int newTab = hit - TAB_HOURLY;
        if (newTab != tab_) {
            tab_ = newTab;
            page_ = 0;
            drawTabs(tft);
            drawContent(tft);
        }
    } else if (hit == CONTENT && tab_ == 0 && hourlyPages() > 1) {
        page_ = (page_ + 1) % hourlyPages();
        drawHourly(tft);
    }
}

void WeatherScreen::update(TFT_eSPI& tft, uint32_t) {
    if (model::state().weather.version != weatherVersion_ && buttons_.pressed() < 0) drawContent(tft);
}
