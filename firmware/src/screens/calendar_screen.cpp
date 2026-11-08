#include "screens/calendar_screen.h"

#include <stdlib.h>

#include "model.h"
#include "timefmt.h"

namespace {

using namespace theme;
using ui::Font;

enum Target { BACK = 0, PREV = 1, NEXT = 2, CHIP0 = 3, AGENDA = 10 };

constexpr ui::Rect PREV_RECT   = {226, 4, 42, 36};
constexpr ui::Rect NEXT_RECT   = {272, 4, 42, 36};
constexpr int16_t  STRIP_Y     = 46;
constexpr int16_t  STRIP_H     = 58;
constexpr int16_t  CHIP_X0     = 8;
constexpr int16_t  CHIP_W      = 40;
constexpr int16_t  CHIP_GAP    = 4;
constexpr int16_t  CHIP_H      = 56;
constexpr ui::Rect AGENDA_RECT = {8, 108, 304, 130};
constexpr int      ROWS        = 3;
constexpr int16_t  ROW_TOP     = 30;
constexpr int16_t  ROW_H       = 33;
constexpr int16_t  SWIPE_PX    = 60;
constexpr uint16_t SELECT_BG   = rgb(0x173643);

}  // namespace

CalendarScreen::CalendarScreen() : buttons_(rects_, TARGETS) {
    rects_[BACK] = ui::HEADER_BACK;
    rects_[PREV] = PREV_RECT;
    rects_[NEXT] = NEXT_RECT;
    for (int i = 0; i < 7; ++i)
        rects_[CHIP0 + i] = {static_cast<int16_t>(CHIP_X0 + i * (CHIP_W + CHIP_GAP)), STRIP_Y, CHIP_W, CHIP_H};
    rects_[AGENDA] = AGENDA_RECT;
}

time_t CalendarScreen::dayAt(int index) const {
    return timefmt::dayStart(timefmt::weekStart(today_), weekOffset_ * 7 + index);
}

int CalendarScreen::eventsOn(time_t start, const int maxOut, int* out) const {
    const time_t end = timefmt::dayStart(start, 1);
    const model::Calendar& cal = model::state().calendar;
    int n = 0;
    for (int i = 0; i < cal.count; ++i) {
        const model::Event& e = cal.events[i];
        if (e.start < end && e.end > start) {   // overlaps the day (handles multi-day)
            if (n < maxOut) out[n] = i;
            ++n;
        }
    }
    return n;
}

int CalendarScreen::pageCount() const {
    int idx[model::MAX_EVENTS];
    const int n = eventsOn(dayAt(selected_), model::MAX_EVENTS, idx);
    return n <= ROWS ? 1 : (n + ROWS - 1) / ROWS;
}

void CalendarScreen::enter(TFT_eSPI& tft, uint32_t nowMs) {
    const time_t now = time(nullptr);
    today_ = timefmt::dayStart(now);
    weekOffset_ = 0;
    selected_ = timefmt::dayDiff(timefmt::weekStart(now), now);
    page_ = 0;
    buttons_.reset();
    calendarVersion_ = model::state().calendar.version;
    struct tm t;
    lastMinute_ = timefmt::local(t) ? t.tm_min : -1;
    lastCheckMs_ = nowMs;

    tft.fillScreen(BG);
    ui::drawChevronButton(tft, ui::HEADER_BACK, true, false);
    ui::drawChevronButton(tft, PREV_RECT, true, false);
    ui::drawChevronButton(tft, NEXT_RECT, false, false);
    drawTitle(tft);
    drawStrip(tft);
    drawAgenda(tft);
}

void CalendarScreen::drawTitle(TFT_eSPI& tft) {
    char buf[24];
    timefmt::monthYear(dayAt(selected_), buf, sizeof(buf));
    ui::drawHeaderTitle(tft, buf, PREV_RECT.x - 6);
}

void CalendarScreen::drawStrip(TFT_eSPI& tft, int pressedChip) {
    ui::region(tft, 0, STRIP_Y, 320, STRIP_H, BG, [&](TFT_eSPI& g, int16_t, int16_t) {
        for (int i = 0; i < 7; ++i) {
            const time_t day = dayAt(i);
            const int16_t x = CHIP_X0 + i * (CHIP_W + CHIP_GAP);
            const bool sel = i == selected_;
            const bool isToday = day == today_;
            const bool past = day < today_;
            uint16_t bg = BG;

            if (sel || i == pressedChip) {
                bg = i == pressedChip ? ACCENT_ALT : SELECT_BG;
                g.fillSmoothRoundRect(x, 0, CHIP_W, CHIP_H, 10, sel ? ACCENT : bg, BG);
                g.fillSmoothRoundRect(x + 1, 1, CHIP_W - 2, CHIP_H - 2, 9, bg, sel ? ACCENT : bg);
            }

            struct tm lt;
            localtime_r(&day, &lt);
            ui::setFont(g, Font::Label);
            ui::text(g, timefmt::weekdayShort(lt.tm_wday), x + CHIP_W / 2, 17, C_BASELINE,
                     sel ? TEXT : TEXT_MUTED, bg);
            char num[4];
            snprintf(num, sizeof(num), "%d", lt.tm_mday);
            ui::setFont(g, Font::Title);
            ui::text(g, num, x + CHIP_W / 2, 40, C_BASELINE,
                     isToday ? ACCENT : (past && !sel ? TEXT_MUTED : TEXT), bg);

            // up to 3 dots, one per distinct calendar with events that day
            int idx[model::MAX_EVENTS];
            const int n = eventsOn(day, model::MAX_EVENTS, idx);
            uint8_t cals[3];
            int nc = 0;
            for (int k = 0; k < n && k < model::MAX_EVENTS && nc < 3; ++k) {
                const uint8_t c = model::state().calendar.events[idx[k]].calendar;
                bool seen = false;
                for (int j = 0; j < nc; ++j) seen |= cals[j] == c;
                if (!seen) cals[nc++] = c;
            }
            const float dotsX = x + CHIP_W / 2.0f - (nc - 1) * 4.0f;
            for (int j = 0; j < nc; ++j)
                g.fillSmoothCircle(dotsX + j * 8, 48, 2.5f, ui::calendarColour(cals[j]), bg);
        }
    });
}

void CalendarScreen::drawAgenda(TFT_eSPI& tft) {
    const model::Calendar& cal = model::state().calendar;
    const time_t day = dayAt(selected_);
    const time_t now = time(nullptr);
    int idx[model::MAX_EVENTS];
    const int total = min(eventsOn(day, model::MAX_EVENTS, idx), model::MAX_EVENTS);
    const int pages = total <= ROWS ? 1 : (total + ROWS - 1) / ROWS;
    if (page_ >= pages) page_ = 0;

    ui::region(tft, AGENDA_RECT.x, AGENDA_RECT.y, AGENDA_RECT.w, AGENDA_RECT.h, BG,
               [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, CARD_EDGE, BG);
        g.fillSmoothRoundRect(1, 1, w - 2, h - 2, 9, theme::CARD, CARD_EDGE);

        char buf[48];
        ui::setFont(g, Font::Label);
        timefmt::dayHeading(day, buf, sizeof(buf));
        ui::text(g, buf, 14, 22, L_BASELINE, day == today_ ? ACCENT : TEXT_MUTED, theme::CARD);
        if (pages > 1) {
            snprintf(buf, sizeof(buf), "%d / %d", page_ + 1, pages);
            ui::text(g, buf, w - 26, 22, R_BASELINE, TEXT_MUTED, theme::CARD);
            ui::drawChevron(g, w - 16, 17.5f, 4, false, TEXT_MUTED, theme::CARD);
        } else if (cal.source == model::Source::Demo) {
            ui::text(g, "Demo data", w - 14, 22, R_BASELINE, STATUS_WARN, theme::CARD);
        }

        if (total == 0) {
            ui::setFont(g, Font::Body);
            ui::text(g, cal.source == model::Source::None ? "Start the PC companion" : "Nothing planned",
                     w / 2, 76, MC_DATUM, TEXT_MUTED, theme::CARD);
            return;
        }

        const int first = page_ * ROWS;
        const int count = min(ROWS, total - first);
        for (int r = 0; r < count; ++r) {
            const model::Event& e = cal.events[idx[first + r]];
            const int16_t top = ROW_TOP + r * ROW_H;
            const bool over = e.end <= now;
            const bool live = e.start <= now && now < e.end && !e.allDay;

            g.fillSmoothRoundRect(14, top + 5, 4, 23, 2, over ? CARD_EDGE : ui::calendarColour(e.calendar),
                                  theme::CARD);

            ui::setFont(g, Font::Label);
            if (e.allDay) snprintf(buf, sizeof(buf), "All day");
            else if (live) snprintf(buf, sizeof(buf), "Now");
            else if (e.start < day) snprintf(buf, sizeof(buf), "Cont.");      // started on an earlier day
            else timefmt::timeOfDay(e.start, buf, sizeof(buf));
            ui::text(g, buf, 26, top + 21, L_BASELINE, live ? ACCENT : TEXT_MUTED, theme::CARD);

            ui::setFont(g, Font::Body);
            char title[64];
            ui::fitText(g, e.title, w - 14 - 88, title, sizeof(title));
            ui::text(g, title, 88, top + 21, L_BASELINE, over ? TEXT_MUTED : TEXT, theme::CARD);

            if (r + 1 < count) g.drawFastHLine(26, top + ROW_H, w - 40, CARD_EDGE);
        }
    });
}

void CalendarScreen::drawTarget(TFT_eSPI& tft, int index, bool pressed) {
    switch (index) {
        case BACK: ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed); break;
        case PREV: ui::drawChevronButton(tft, PREV_RECT, true, pressed); break;
        case NEXT: ui::drawChevronButton(tft, NEXT_RECT, false, pressed); break;
        case AGENDA:
            if (pageCount() > 1) {
                if (pressed) tft.drawRoundRect(AGENDA_RECT.x, AGENDA_RECT.y, AGENDA_RECT.w, AGENDA_RECT.h, 10, ACCENT);
                else drawAgenda(tft);
            }
            break;
        default: drawStrip(tft, pressed ? index - CHIP0 : -1); break;
    }
}

void CalendarScreen::changeWeek(TFT_eSPI& tft, int delta) {
    weekOffset_ += delta;
    page_ = 0;
    drawTitle(tft);
    drawStrip(tft);
    drawAgenda(tft);
}

void CalendarScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    using touch_input::EventType;
    if (ev.type == EventType::Press) { pressX_ = ev.x; pressY_ = ev.y; }

    if (ev.type == EventType::Release) {   // horizontal swipe = change week
        const int dx = ev.x - pressX_, dy = ev.y - pressY_;
        if (abs(dx) >= SWIPE_PX && abs(dx) > 2 * abs(dy)) {
            const int p = buttons_.pressed();
            buttons_.reset();
            if (p >= 0) drawTarget(tft, p, false);
            changeWeek(tft, dx < 0 ? 1 : -1);
            return;
        }
    }

    const int hit = buttons_.handle(ev, [&](int i, bool pressed) { drawTarget(tft, i, pressed); });
    if (hit < 0) return;

    if (hit == BACK) {
        ui::nav::back();
    } else if (hit == PREV || hit == NEXT) {
        changeWeek(tft, hit == PREV ? -1 : 1);
    } else if (hit == AGENDA) {
        const int pages = pageCount();
        if (pages > 1) { page_ = (page_ + 1) % pages; drawAgenda(tft); }
    } else {
        const int day = hit - CHIP0;
        if (day != selected_) {
            const time_t before = dayAt(selected_);
            selected_ = day;
            page_ = 0;
            drawStrip(tft);
            drawAgenda(tft);
            if (timefmt::dayDiff(before, dayAt(selected_)) != 0) drawTitle(tft);   // month may change
        }
    }
}

void CalendarScreen::update(TFT_eSPI& tft, uint32_t nowMs) {
    if (nowMs - lastCheckMs_ < 1000) return;
    lastCheckMs_ = nowMs;

    struct tm t;
    const int minute = timefmt::local(t) ? t.tm_min : -1;
    const bool calChanged = model::state().calendar.version != calendarVersion_;
    if (minute == lastMinute_ && !calChanged) return;
    lastMinute_ = minute;
    calendarVersion_ = model::state().calendar.version;
    if (buttons_.pressed() >= 0) return;   // don't redraw under a finger

    const time_t todayNow = timefmt::dayStart(time(nullptr));
    if (todayNow != today_ || calChanged) {   // midnight passed or new data
        today_ = todayNow;
        drawStrip(tft);
    }
    drawAgenda(tft);   // past/now styling changes over time
}
