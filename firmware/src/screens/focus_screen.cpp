#include "screens/focus_screen.h"

#include <time.h>

#include "timefmt.h"
#include "timers.h"

namespace {

using namespace theme;
using ui::Font;
using timers::Phase;
using timers::State;

constexpr ui::Rect RING      = {22, 68, 148, 148};
constexpr int16_t  RING_R    = 70, RING_IR = 61;
constexpr ui::Rect INFO      = {184, 50, 132, 82};
constexpr ui::Rect PRIMARY_R = {184, 136, 126, 46};
constexpr ui::Rect SECOND_R  = {184, 188, 126, 46};

bool alerting() { return timers::alert() == timers::Alert::Focus; }

uint16_t phaseColour(Phase p) { return p == Phase::Focus ? ACCENT : STATUS_OK; }

void button(TFT_eSPI& tft, const ui::Rect& r, const char* label, bool accent, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : (accent ? ACCENT : theme::CARD);
    ui::region(tft, r.x, r.y, r.w, r.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 12, fill, BG);
        ui::setFont(g, Font::Body);
        ui::text(g, label, w / 2, h / 2 + 1, MC_DATUM, accent && !pressed ? BG : TEXT, fill);
    });
}

// Arc from 12 o'clock, clockwise, covering `frac` of the circle. TFT_eSPI angles
// start at 6 o'clock, so 12 o'clock is 180; split arcs that pass 360.
void arc(TFT_eSPI& g, int16_t cx, int16_t cy, float frac, uint16_t colour) {
    if (frac <= 0.004f) return;
    const uint32_t sweep = static_cast<uint32_t>(fminf(frac, 1.0f) * 360.0f);
    const uint32_t end = 180 + sweep;
    if (end <= 360) {
        g.drawSmoothArc(cx, cy, RING_R, RING_IR, 180, end, colour, BG, false);
    } else {
        g.drawSmoothArc(cx, cy, RING_R, RING_IR, 180, 360, colour, BG, false);
        g.drawSmoothArc(cx, cy, RING_R, RING_IR, 0, end - 360, colour, BG, false);
    }
}

}  // namespace

FocusScreen::FocusScreen() : buttons_(rects_, COUNT) {
    rects_[BACK] = ui::HEADER_BACK;
    rects_[PRIMARY] = PRIMARY_R;
    rects_[SECONDARY] = SECOND_R;
}

bool FocusScreen::idleReturnsHome() const {
    return !alerting() && timers::focus().state == State::Idle;   // stay while a session runs
}

int FocusScreen::stateKey() const {
    const timers::Focus& f = timers::focus();
    return static_cast<int>(f.state) * 1000 + static_cast<int>(f.phase) * 100 + f.focusMinutes + (alerting() ? 10000 : 0) +
           f.completed * 100000;
}

void FocusScreen::labels(const char*& primary, const char*& secondary, bool& primaryAccent) const {
    const timers::Focus& f = timers::focus();
    primaryAccent = false;
    secondary = "";
    switch (f.state) {
        case State::Running: primary = "Pause"; secondary = "Skip"; break;
        case State::Paused: primary = "Resume"; secondary = "Reset"; primaryAccent = true; break;
        default:
            primaryAccent = true;
            if (alerting()) {
                primary = f.phase == Phase::Focus ? "Start focus" : "Start break";
                secondary = "Dismiss";
            } else {
                primary = "Start";
                if (f.phase == Phase::Focus) {
                    static char buf[16];
                    snprintf(buf, sizeof(buf), "%u min", f.focusMinutes);
                    secondary = buf;
                } else {
                    secondary = "Skip break";
                }
            }
    }
}

void FocusScreen::drawTarget(TFT_eSPI& tft, int i, bool pressed) {
    const char *p, *s;
    bool pa;
    labels(p, s, pa);
    if (i == BACK) ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed);
    else if (i == PRIMARY) button(tft, PRIMARY_R, p, pa, pressed);
    else button(tft, SECOND_R, s, false, pressed);
}

void FocusScreen::drawRing(TFT_eSPI& tft, uint32_t now) {
    const timers::Focus& f = timers::focus();
    const uint32_t remaining = timers::focusRemaining(now);
    const float frac = remaining / static_cast<float>(timers::focusPhaseLength(f));
    char t[12];
    ui::formatCountdown(remaining, t, sizeof(t));
    snprintf(lastTime_, sizeof(lastTime_), "%s", t);

    ui::region(tft, RING.x, RING.y, RING.w, RING.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        const int16_t cx = w / 2, cy = h / 2;
        g.drawSmoothArc(cx, cy, RING_R, RING_IR, 0, 360, CARD_EDGE, BG, false);
        const uint16_t c = f.state == State::Paused ? TEXT_MUTED : phaseColour(f.phase);
        arc(g, cx, cy, frac, c);

        // session dots
        for (int i = 0; i < timers::SESSIONS_PER_CYCLE; ++i)
            g.fillSmoothCircle(cx + (i - 1.5f) * 12, cy - 26, 3.5f, i < f.completed ? ACCENT : CARD_EDGE, BG);

        ui::setFont(g, Font::Temp);
        ui::monoText(g, t, cx, cy + 14, f.state == State::Paused ? TEXT_MUTED : TEXT, BG);
        ui::setFont(g, Font::Label);
        ui::text(g, f.phase == Phase::Focus ? "FOCUS" : "BREAK", cx, cy + 36, C_BASELINE,
                 phaseColour(f.phase), BG);
    });
}

void FocusScreen::drawText(TFT_eSPI& tft, uint32_t now) {
    const timers::Focus& f = timers::focus();
    ui::region(tft, INFO.x, INFO.y, INFO.w, INFO.h, BG, [&](TFT_eSPI& g, int16_t, int16_t) {
        char buf[40];
        ui::setFont(g, Font::Title);
        if (alerting()) {
            ui::text(g, f.phase == Phase::Focus ? "Focus time" : "Break time", 0, 24, L_BASELINE,
                     flash_ ? phaseColour(f.phase) : TEXT, BG);
        } else {
            ui::text(g, timers::phaseName(f.phase), 0, 24, L_BASELINE, TEXT, BG);
        }
        ui::setFont(g, Font::Label);
        if (f.phase == Phase::Focus)
            snprintf(buf, sizeof(buf), "Session %u of %u", min<int>(f.completed + 1, timers::SESSIONS_PER_CYCLE),
                     timers::SESSIONS_PER_CYCLE);
        else
            snprintf(buf, sizeof(buf), "%u of %u done", f.completed ? f.completed : timers::SESSIONS_PER_CYCLE,
                     timers::SESSIONS_PER_CYCLE);
        ui::text(g, buf, 0, 46, L_BASELINE, TEXT_MUTED, BG);

        if (f.state == State::Running && timefmt::valid()) {
            char t[12];
            timefmt::timeOfDay(time(nullptr) + timers::focusRemaining(now) / 1000, t, sizeof(t));
            snprintf(buf, sizeof(buf), "Ends %s", t);
        } else if (f.state == State::Paused) {
            snprintf(buf, sizeof(buf), "Paused");
        } else {
            snprintf(buf, sizeof(buf), alerting() ? "Ready when you are" : "Ready");
        }
        ui::text(g, buf, 0, 66, L_BASELINE, TEXT_MUTED, BG);
    });
}

void FocusScreen::drawAll(TFT_eSPI& tft, uint32_t now) {
    buttons_.reset();
    drawRing(tft, now);
    drawText(tft, now);
    drawTarget(tft, PRIMARY, false);
    drawTarget(tft, SECONDARY, false);
    lastKey_ = stateKey();
}

void FocusScreen::enter(TFT_eSPI& tft, uint32_t now) {
    tft.fillScreen(BG);
    ui::drawHeader(tft, "Focus", false);
    drawAll(tft, now);
}

void FocusScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t now) {
    const int hit = buttons_.handle(ev, [&](int i, bool pressed) { drawTarget(tft, i, pressed); });
    if (hit < 0) return;
    if (hit == BACK) { ui::nav::back(); return; }

    const timers::Focus& f = timers::focus();
    if (hit == PRIMARY) {
        if (f.state == State::Running) timers::focusPause(now);
        else timers::focusStart(now);   // start / resume / start next phase (clears the alert)
    } else {
        switch (f.state) {
            case State::Running: timers::focusSkip(now); break;
            case State::Paused: timers::focusReset(); break;
            default:
                if (alerting()) timers::dismissAlert();
                else if (f.phase == Phase::Focus) timers::focusCycleMinutes();
                else timers::focusSkip(now);
        }
    }
    drawAll(tft, now);
}

void FocusScreen::update(TFT_eSPI& tft, uint32_t now) {
    if (buttons_.pressed() >= 0) return;
    if (stateKey() != lastKey_) { drawAll(tft, now); return; }
    if (now - lastTick_ < (alerting() ? 500 : 250)) return;
    lastTick_ = now;
    if (alerting()) {
        flash_ = !flash_;
        drawText(tft, now);
        return;
    }
    char t[12];
    ui::formatCountdown(timers::focusRemaining(now), t, sizeof(t));
    if (strcmp(t, lastTime_) != 0) drawRing(tft, now);
}
