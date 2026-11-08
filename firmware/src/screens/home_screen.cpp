#include "screens/home_screen.h"

#include <time.h>

#include <string.h>

#include "model.h"
#include "net/network.h"
#include "pc_link.h"
#include "settings.h"
#include "timefmt.h"
#include "ui/icons.h"
#include "ui/ui.h"

namespace {

using namespace theme;
using ui::Font;

// Layout (320 x 240). Baselines are shared across columns so rows line up.
constexpr ui::Rect CLOCK_AREA   = {0, 0, 192, 114};
constexpr ui::Rect WEATHER_AREA = {192, 0, 128, 114};
constexpr ui::Rect UPNEXT       = {8, 118, 304, 114};
constexpr int16_t  MARGIN       = 14;
constexpr int16_t  BIG_BASELINE = 64;    // clock digits + temperature
constexpr int16_t  SUB_BASELINE = 88;    // date + condition
constexpr int16_t  HL_BASELINE  = 106;   // today's high / low
constexpr int16_t  CARD_RADIUS  = 10;
constexpr int16_t  ROW_TOP      = 32;
constexpr int16_t  ROW_H        = 40;
constexpr int      ROWS         = 2;
constexpr int16_t  ICON_SIZE    = 34;
constexpr uint32_t CHECK_MS     = 250;

const char DEGREE[] = "\xC2\xB0";   // "°"

}  // namespace

void HomeScreen::enter(TFT_eSPI& tft, uint32_t nowMs) {
    tft.fillScreen(BG);
    cardPressed_ = weatherPressed_ = clockPressed_ = false;
    refreshStatus();
    drawClock(tft);
    drawWeather(tft);
    drawEvents(tft);
    lastCheckMs_ = nowMs;
}

void HomeScreen::drawClock(TFT_eSPI& tft) {
    struct tm t;
    const bool ok = timefmt::local(t);
    lastMinute_ = ok ? t.tm_min : -1;

    ui::region(tft, CLOCK_AREA.x, CLOCK_AREA.y, CLOCK_AREA.w, CLOCK_AREA.h, BG,
               [&](TFT_eSPI& g, int16_t, int16_t) {
        char hm[8] = "--:--", ampm[4] = "", date[32] = "Waiting for time";
        if (ok) {
            timefmt::clock(t, hm, sizeof(hm), ampm, sizeof(ampm));
            timefmt::date(t, date, sizeof(date));
        }
        ui::setFont(g, Font::Clock);
        ui::text(g, hm, MARGIN, BIG_BASELINE, L_BASELINE, TEXT, BG);
        const int16_t ampmX = MARGIN + g.textWidth(hm) + 6;
        if (ampm[0]) {
            ui::setFont(g, Font::Label);
            ui::text(g, ampm, ampmX, BIG_BASELINE, L_BASELINE, TEXT_MUTED, BG);
        }
        ui::setFont(g, Font::Body);
        ui::text(g, date, MARGIN, SUB_BASELINE, L_BASELINE, TEXT_MUTED, BG);

        if (status_[0]) {   // connectivity / data problems only; hidden when all is well
            ui::setFont(g, Font::Label);
            g.fillSmoothCircle(MARGIN + 3, 11, 3, statusColour_, BG);
            ui::text(g, status_, MARGIN + 11, 16, L_BASELINE, statusColour_, BG);
        }
    });
}

bool HomeScreen::refreshStatus() {
    char s[sizeof(status_)] = "";
    uint16_t colour = TEXT_MUTED;
    if (!net::statusLine(s, sizeof(s), colour)) {
        s[0] = '\0';
        const model::Calendar& cal = model::state().calendar;   // PC off and calendar getting old
        const time_t now = time(nullptr);
        if (cal.source == model::Source::Live && !pc_link::connected() && cal.receivedAt > 0 && timefmt::valid() &&
            now - cal.receivedAt > static_cast<time_t>(settings::CALENDAR_STALE_S)) {
            snprintf(s, sizeof(s), "Calendar from %ldh ago", static_cast<long>((now - cal.receivedAt) / 3600));
            colour = TEXT_MUTED;
        }
    }
    if (strcmp(s, status_) == 0 && colour == statusColour_) return false;
    snprintf(status_, sizeof(status_), "%s", s);
    statusColour_ = colour;
    return true;
}

void HomeScreen::drawWeather(TFT_eSPI& tft) {
    const model::Weather& w = model::state().weather;
    weatherVersion_ = w.version;

    ui::region(tft, WEATHER_AREA.x, WEATHER_AREA.y, WEATHER_AREA.w, WEATHER_AREA.h, BG,
               [&](TFT_eSPI& g, int16_t width, int16_t) {
        const int16_t right = width - MARGIN;
        if (w.source == model::Source::None) {
            ui::setFont(g, Font::Label);
            ui::text(g, "No weather yet", right, SUB_BASELINE, R_BASELINE, TEXT_MUTED, BG);
            return;
        }
        char buf[24];
        snprintf(buf, sizeof(buf), "%d%s", ui::roundTemp(w.tempC), DEGREE);
        ui::setFont(g, Font::Temp);
        const int16_t tempW = g.textWidth(buf);
        ui::text(g, buf, right, BIG_BASELINE, R_BASELINE, TEXT, BG);

        int16_t iconX = right - tempW - 6 - ICON_SIZE / 2;
        if (iconX < ICON_SIZE / 2 + 2) iconX = ICON_SIZE / 2 + 2;
        ui::drawWeatherIcon(g, iconX, BIG_BASELINE - 15, ICON_SIZE, w.code, w.isDay, BG);

        ui::setFont(g, Font::Label);
        ui::text(g, ui::weatherLabel(w.code), right, SUB_BASELINE, R_BASELINE, TEXT_MUTED, BG);
        snprintf(buf, sizeof(buf), "H %d%s  L %d%s", ui::roundTemp(w.highC), DEGREE,
                 ui::roundTemp(w.lowC), DEGREE);
        ui::text(g, buf, right, HL_BASELINE, R_BASELINE, TEXT_MUTED, BG);
    });
}

void HomeScreen::drawEvents(TFT_eSPI& tft) {
    const model::Calendar& cal = model::state().calendar;
    calendarVersion_ = cal.version;
    const time_t now = time(nullptr);

    // Upcoming = not yet finished, in start order (events[] is kept sorted).
    const model::Event* rows[ROWS] = {};
    int n = 0;
    for (int i = 0; i < cal.count && n < ROWS; ++i)
        if (cal.events[i].end > now) rows[n++] = &cal.events[i];

    ui::region(tft, UPNEXT.x, UPNEXT.y, UPNEXT.w, UPNEXT.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, CARD_RADIUS, CARD_EDGE, BG);
        g.fillSmoothRoundRect(1, 1, w - 2, h - 2, CARD_RADIUS - 1, theme::CARD, CARD_EDGE);

        ui::setFont(g, Font::Label);
        ui::text(g, "UP NEXT", MARGIN, 22, L_BASELINE, TEXT_MUTED, theme::CARD);

        if (cal.source == model::Source::Demo) {   // be honest about where data comes from
            const char* tag = "Demo data";
            const int16_t tw = g.textWidth(tag);
            ui::text(g, tag, w - MARGIN, 22, R_BASELINE, STATUS_WARN, theme::CARD);
            g.fillSmoothCircle(w - MARGIN - tw - 8, 17, 3, STATUS_WARN, theme::CARD);
        }

        if (n == 0) {
            ui::setFont(g, Font::Body);
            ui::text(g, cal.source == model::Source::None ? "Start the PC companion" : "Nothing coming up",
                     w / 2, 70, MC_DATUM, TEXT_MUTED, theme::CARD);
            return;
        }

        for (int i = 0; i < n; ++i) {
            const model::Event& e = *rows[i];
            const int16_t top = ROW_TOP + i * ROW_H;
            char timeBuf[16], titleBuf[64], whenBuf[40];

            g.fillSmoothRoundRect(MARGIN, top + 5, 4, 30, 2, ui::calendarColour(e.calendar), theme::CARD);

            ui::setFont(g, Font::Label);
            if (e.allDay) snprintf(timeBuf, sizeof(timeBuf), "All day");
            else timefmt::timeOfDay(e.start, timeBuf, sizeof(timeBuf));
            const int16_t timeW = g.textWidth(timeBuf);
            ui::text(g, timeBuf, w - MARGIN, top + 19, R_BASELINE, TEXT_MUTED, theme::CARD);

            const bool soon = (e.start <= now) || (e.start - now < 3600 && timefmt::dayDiff(now, e.start) == 0);
            timefmt::when(e.start, e.end, e.allDay, now, whenBuf, sizeof(whenBuf));
            ui::text(g, whenBuf, MARGIN + 14, top + 36, L_BASELINE, soon ? ACCENT : TEXT_MUTED, theme::CARD);

            ui::setFont(g, Font::Body);
            const int16_t titleX = MARGIN + 14;
            ui::fitText(g, e.title, w - MARGIN - timeW - 12 - titleX, titleBuf, sizeof(titleBuf));
            ui::text(g, titleBuf, titleX, top + 19, L_BASELINE, TEXT, theme::CARD);

            if (i + 1 < n) g.drawFastHLine(titleX, top + ROW_H, w - titleX - MARGIN, CARD_EDGE);
        }
    });
}

void HomeScreen::drawCardOutline(TFT_eSPI& tft, bool pressed) {
    if (pressed) tft.drawRoundRect(UPNEXT.x, UPNEXT.y, UPNEXT.w, UPNEXT.h, CARD_RADIUS, ACCENT);
    else drawEvents(tft);
}

void HomeScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    using touch_input::EventType;
    switch (ev.type) {
        case EventType::Press:
            cardPressed_ = UPNEXT.hit(ev.x, ev.y);
            weatherPressed_ = !cardPressed_ && WEATHER_AREA.hit(ev.x, ev.y);   // padded targets overlap: card wins
            clockPressed_ = !cardPressed_ && !weatherPressed_ && CLOCK_AREA.contains(ev.x, ev.y);
            if (cardPressed_) drawCardOutline(tft, true);
            break;
        case EventType::Move:
            if (cardPressed_ && !UPNEXT.hit(ev.x, ev.y)) { cardPressed_ = false; drawCardOutline(tft, false); }
            if (weatherPressed_ && !WEATHER_AREA.hit(ev.x, ev.y)) weatherPressed_ = false;
            if (clockPressed_ && !CLOCK_AREA.hit(ev.x, ev.y)) clockPressed_ = false;
            break;
        case EventType::Release:
            if (cardPressed_) drawCardOutline(tft, false);
            break;
        case EventType::Tap:
            if (cardPressed_ && UPNEXT.hit(ev.x, ev.y)) ui::nav::go(ui::ScreenId::Calendar);
            else if (weatherPressed_ && WEATHER_AREA.hit(ev.x, ev.y)) ui::nav::go(ui::ScreenId::Weather);
            else if (clockPressed_ && CLOCK_AREA.hit(ev.x, ev.y)) ui::nav::go(ui::ScreenId::Launcher);
            cardPressed_ = weatherPressed_ = clockPressed_ = false;
            break;
        case EventType::LongPress:
            ui::nav::go(ui::ScreenId::Launcher);
            break;
    }
}

void HomeScreen::update(TFT_eSPI& tft, uint32_t nowMs) {
    if (nowMs - lastCheckMs_ < CHECK_MS) return;
    lastCheckMs_ = nowMs;

    struct tm t;
    const int minute = timefmt::local(t) ? t.tm_min : -1;
    const model::State& st = model::state();

    const bool statusChanged = refreshStatus();
    if (minute != lastMinute_ || statusChanged) {
        drawClock(tft);
        if (minute != lastMinute_ && !cardPressed_) drawEvents(tft);   // countdowns change every minute
    } else if (st.calendar.version != calendarVersion_ && !cardPressed_) {
        drawEvents(tft);
    }
    if (st.weather.version != weatherVersion_) drawWeather(tft);
}
