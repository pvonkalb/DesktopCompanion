#include "screens/timer_screen.h"

#include <time.h>

#include "timefmt.h"
#include "timers.h"

namespace {

using namespace theme;
using ui::Font;
using timers::State;

constexpr ui::Rect TABS      = {58, 6, 256, 32};
constexpr ui::Rect HIDDEN    = {-200, -200, 0, 0};
constexpr ui::Rect TIME_NARROW = {64, 48, 192, 86};    // between the -/+ buttons
constexpr ui::Rect TIME_WIDE   = {0, 48, 320, 96};    // stopwatch: centred in the free space
constexpr int16_t  TIME_BASELINE = 64;                  // within the time region
constexpr ui::Rect INFO      = {0, 136, 320, 44};
constexpr ui::Rect LAPS      = {0, 144, 320, 38};   // stopwatch laps (below the time)
constexpr ui::Rect MINUS_R   = {8, 66, 52, 52};
constexpr ui::Rect PLUS_R    = {260, 66, 52, 52};
constexpr ui::Rect LEFT_R    = {14, 184, 142, 46};
constexpr ui::Rect RIGHT_R   = {164, 184, 142, 46};
constexpr ui::Rect FULL_R    = {14, 184, 292, 46};
constexpr int16_t  PRESET_X0 = 14, PRESET_Y = 140, PRESET_W = 52, PRESET_H = 34, PRESET_GAP = 8;
const uint8_t PRESET_MIN[] = {1, 5, 10, 15, 30};
const char* const TAB_LABELS[] = {"Timer", "Stopwatch"};
constexpr uint32_t MINUTE = 60000;

void roundButton(TFT_eSPI& tft, const ui::Rect& r, bool plus, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothCircle(w / 2, h / 2, w / 2 - 1, fill, BG);
        g.drawWideLine(w / 2 - 10, h / 2, w / 2 + 10, h / 2, 3, TEXT, fill);
        if (plus) g.drawWideLine(w / 2, h / 2 - 10, w / 2, h / 2 + 10, 3, TEXT, fill);
    });
}

void button(TFT_eSPI& tft, const ui::Rect& r, const char* label, bool accent, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : (accent ? ACCENT : theme::CARD);
    const uint16_t fg = accent && !pressed ? BG : TEXT;
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 12, fill, BG);
        ui::setFont(g, Font::Title);
        ui::text(g, label, w / 2, h / 2 + 1, MC_DATUM, fg, fill);
    });
}

}  // namespace

TimerScreen::TimerScreen() : buttons_(rects_, COUNT) {
    for (auto& r : rects_) r = HIDDEN;
    rects_[BACK] = ui::HEADER_BACK;
    rects_[TAB_TIMER] = ui::segmentRect(TABS, 2, 0);
    rects_[TAB_SW] = ui::segmentRect(TABS, 2, 1);
}

bool TimerScreen::idleReturnsHome() const {
    if (timers::alert() != timers::Alert::None) return false;
    if (tab_ == 0) return timers::countdown().state == State::Idle;          // stay while it counts
    const timers::Stopwatch& sw = timers::stopwatch();
    return !sw.running && sw.accumulatedMs == 0;
}

int TimerScreen::layoutKey(uint32_t now) const {
    if (tab_ == 0) return 100 + static_cast<int>(timers::countdown().state);
    const timers::Stopwatch& sw = timers::stopwatch();
    return 200 + (sw.running ? 1 : (timers::stopwatchElapsed(now) > 0 ? 2 : 0));
}

void TimerScreen::labels(const char*& left, const char*& right, const char*& full, bool& leftAccent,
                         bool& rightAccent) const {
    left = right = full = "";
    leftAccent = rightAccent = false;
    if (tab_ == 0) {
        switch (timers::countdown().state) {
            case State::Idle: full = "Start"; break;
            case State::Running: left = "Pause"; right = "Reset"; break;
            case State::Paused: left = "Resume"; right = "Reset"; leftAccent = true; break;
            case State::Done: left = "+1 min"; right = "Done"; rightAccent = true; break;
        }
    } else {
        const timers::Stopwatch& sw = timers::stopwatch();
        if (sw.running) { left = "Lap"; right = "Stop"; }
        else if (sw.accumulatedMs > 0) { left = "Reset"; right = "Resume"; rightAccent = true; }
        else full = "Start";
    }
}

void TimerScreen::layout() {
    for (int i = MINUS; i < COUNT; ++i) rects_[i] = HIDDEN;
    const char *l, *r, *f;
    bool la, ra;
    labels(l, r, f, la, ra);
    if (f[0]) rects_[FULL] = FULL_R;
    else { rects_[LEFT] = LEFT_R; rects_[RIGHT] = RIGHT_R; }
    if (tab_ == 0 && timers::countdown().state == State::Idle) {
        rects_[MINUS] = MINUS_R;
        rects_[PLUS] = PLUS_R;
        for (int i = 0; i < 5; ++i)
            rects_[PRESET0 + i] = {static_cast<int16_t>(PRESET_X0 + i * (PRESET_W + PRESET_GAP)), PRESET_Y,
                                   PRESET_W, PRESET_H};
    }
}

void TimerScreen::drawTarget(TFT_eSPI& tft, int i, bool pressed) {
    const char *l, *r, *f;
    bool la, ra;
    labels(l, r, f, la, ra);
    if (i == BACK) ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed);
    else if (i == TAB_TIMER || i == TAB_SW) ui::drawSegmented(tft, TABS, TAB_LABELS, 2, tab_, pressed ? i - TAB_TIMER : -1);
    else if (i == MINUS || i == PLUS) roundButton(tft, rects_[i], i == PLUS, pressed);
    else if (i >= PRESET0 && i <= PRESET4) {
        const int k = i - PRESET0;
        const bool selected = timers::countdown().durationMs == PRESET_MIN[k] * MINUTE;
        const uint16_t fill = pressed ? ACCENT_ALT : (selected ? ACCENT : theme::CARD);
        const ui::Rect& rc = rects_[i];
        ui::region(tft, rc.x, rc.y, rc.w, rc.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
            g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
            char buf[8];
            snprintf(buf, sizeof(buf), "%um", PRESET_MIN[k]);
            ui::setFont(g, Font::Body);
            ui::text(g, buf, w / 2, h / 2 + 1, MC_DATUM, selected && !pressed ? BG : TEXT, fill);
        });
    } else if (i == LEFT) button(tft, rects_[i], l, la, pressed);
    else if (i == RIGHT) button(tft, rects_[i], r, ra, pressed);
    else if (i == FULL) button(tft, rects_[i], f, true, pressed);
}

void TimerScreen::drawTime(TFT_eSPI& tft, uint32_t now) {
    char buf[16];
    uint16_t colour = TEXT;
    if (tab_ == 0) {
        const timers::Countdown& cd = timers::countdown();
        ui::formatCountdown(timers::countdownRemaining(now), buf, sizeof(buf));
        if (cd.state == State::Paused) colour = TEXT_MUTED;
        if (cd.state == State::Done) colour = flash_ ? STATUS_WARN : TEXT_MUTED;
    } else {
        ui::formatStopwatch(timers::stopwatchElapsed(now), buf, sizeof(buf));
        if (!timers::stopwatch().running && timers::stopwatch().accumulatedMs > 0) colour = TEXT_MUTED;
    }
    snprintf(lastTime_, sizeof(lastTime_), "%s", buf);
    const ui::Rect& r = tab_ == 0 ? TIME_NARROW : TIME_WIDE;
    // Stopwatch: centre the digits between header and buttons; lift them once laps appear.
    const int16_t baseline = tab_ == 0 ? TIME_BASELINE : (timers::stopwatch().lapCount ? 70 : 84);
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        ui::setFont(g, Font::Clock);
        ui::monoText(g, buf, w / 2, baseline, colour, BG);
    });
}

void TimerScreen::drawInfo(TFT_eSPI& tft, uint32_t now) {
    if (tab_ == 0 && timers::countdown().state == State::Idle) return;   // presets live here
    if (tab_ == 1 && timers::stopwatch().lapCount == 0) return;          // nothing below the time yet
    const ui::Rect& area = tab_ == 0 ? INFO : LAPS;
    ui::region(tft, area.x, area.y, area.w, area.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        char buf[40];
        if (tab_ == 0) {
            const timers::Countdown& cd = timers::countdown();
            if (cd.state == State::Done) {
                ui::setFont(g, Font::Title);
                ui::text(g, "Time's up", w / 2, 24, MC_DATUM, flash_ ? STATUS_WARN : TEXT, BG);
                return;
            }
            const float frac = cd.durationMs ? fminf(1.0f, timers::countdownRemaining(now) / static_cast<float>(cd.durationMs)) : 0;
            constexpr int16_t BAR_X = 40, BAR_W = 240;
            g.fillSmoothRoundRect(BAR_X, 6, BAR_W, 6, 3, CARD_EDGE, BG);
            const int16_t fill = static_cast<int16_t>(frac * BAR_W);
            if (fill > 3) g.fillSmoothRoundRect(BAR_X, 6, fill, 6, 3, cd.state == State::Paused ? TEXT_MUTED : ACCENT, CARD_EDGE);
            ui::setFont(g, Font::Label);
            if (cd.state == State::Paused) {
                snprintf(buf, sizeof(buf), "Paused");
            } else {
                char t[12];
                timefmt::timeOfDay(time(nullptr) + timers::countdownRemaining(now) / 1000, t, sizeof(t));
                snprintf(buf, sizeof(buf), "Ends %s", timefmt::valid() ? t : "");
            }
            ui::text(g, buf, w / 2, 32, C_BASELINE, TEXT_MUTED, BG);
        } else {
            const timers::Stopwatch& sw = timers::stopwatch();
            ui::setFont(g, Font::Body);
            const int shown = min<int>(2, min<int>(sw.lapCount, timers::MAX_LAPS));
            for (int i = 0; i < shown; ++i) {
                char t[16];
                ui::formatStopwatch(sw.laps[i], t, sizeof(t));
                snprintf(buf, sizeof(buf), "Lap %u", sw.lapCount - i);
                const int16_t y = 15 + i * 19;
                ui::text(g, buf, 90, y, L_BASELINE, TEXT_MUTED, BG);
                ui::text(g, t, 230, y, R_BASELINE, i == 0 ? TEXT : TEXT_MUTED, BG);
            }
        }
    });
}

void TimerScreen::drawBody(TFT_eSPI& tft, uint32_t now) {
    tft.fillRect(0, ui::HEADER_H, 320, 240 - ui::HEADER_H, BG);
    buttons_.reset();
    layout();
    drawTarget(tft, TAB_TIMER, false);
    for (int i = MINUS; i < COUNT; ++i)
        if (rects_[i].w > 0) drawTarget(tft, i, false);
    drawTime(tft, now);
    drawInfo(tft, now);
    lastKey_ = layoutKey(now);
}

void TimerScreen::enter(TFT_eSPI& tft, uint32_t now) {
    const timers::Stopwatch& sw = timers::stopwatch();
    if (timers::alert() == timers::Alert::Countdown) tab_ = 0;
    else if (timers::countdown().state == State::Idle && (sw.running || sw.accumulatedMs > 0)) tab_ = 1;
    else if (timers::countdown().state != State::Idle && !sw.running) tab_ = 0;
    tft.fillScreen(BG);
    ui::drawChevronButton(tft, ui::HEADER_BACK, true, false);
    drawBody(tft, now);
}

void TimerScreen::activate(TFT_eSPI& tft, int i, uint32_t now) {
    const char *l, *r, *f;
    bool la, ra;
    labels(l, r, f, la, ra);
    const auto is = [](const char* a, const char* b) { return strcmp(a, b) == 0; };
    const char* label = i == LEFT ? l : i == RIGHT ? r : i == FULL ? f : "";

    switch (i) {
        case BACK: ui::nav::back(); return;
        case TAB_TIMER: case TAB_SW: tab_ = i - TAB_TIMER; break;
        case MINUS: timers::countdownSet(timers::countdown().durationMs - MINUTE); break;
        case PLUS: timers::countdownSet(timers::countdown().durationMs + MINUTE); break;
        default:
            if (i >= PRESET0 && i <= PRESET4) { timers::countdownSet(PRESET_MIN[i - PRESET0] * MINUTE); break; }
            if (tab_ == 0) {
                if (is(label, "Start") || is(label, "Resume")) timers::countdownStart(now);
                else if (is(label, "Pause")) timers::countdownPause(now);
                else if (is(label, "Reset") || is(label, "Done")) timers::countdownReset();
                else if (is(label, "+1 min")) timers::countdownAddMinute(now);
            } else {
                if (is(label, "Start") || is(label, "Resume")) timers::stopwatchStart(now);
                else if (is(label, "Stop")) timers::stopwatchStop(now);
                else if (is(label, "Lap")) timers::stopwatchLap(now);
                else if (is(label, "Reset")) timers::stopwatchReset();
            }
    }
    drawBody(tft, now);
}

void TimerScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t now) {
    const int hit = buttons_.handle(ev, [&](int i, bool pressed) { drawTarget(tft, i, pressed); });
    if (hit >= 0) activate(tft, hit, now);
}

void TimerScreen::update(TFT_eSPI& tft, uint32_t now) {
    if (buttons_.pressed() >= 0) return;   // don't redraw under a finger
    if (layoutKey(now) != lastKey_) {      // state changed elsewhere (e.g. countdown finished)
        drawBody(tft, now);
        return;
    }
    const bool done = tab_ == 0 && timers::countdown().state == State::Done;
    const uint32_t period = done ? 500 : (tab_ == 1 && timers::stopwatch().running ? 100 : 250);
    if (now - lastTimeDraw_ < period) return;
    lastTimeDraw_ = now;
    if (done) {
        flash_ = !flash_;
        drawTime(tft, now);
        drawInfo(tft, now);
        return;
    }
    char buf[16];
    if (tab_ == 0) ui::formatCountdown(timers::countdownRemaining(now), buf, sizeof(buf));
    else ui::formatStopwatch(timers::stopwatchElapsed(now), buf, sizeof(buf));
    if (strcmp(buf, lastTime_) != 0) {
        drawTime(tft, now);
        if (tab_ == 0 && timers::countdown().state == State::Running) drawInfo(tft, now);
    }
}
