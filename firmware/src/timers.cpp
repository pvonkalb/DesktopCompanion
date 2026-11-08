#include "timers.h"

#include <Arduino.h>

#include "board_cyd.h"

namespace timers {
namespace {

Countdown cd;
Stopwatch sw;
Focus fc;

Alert currentAlert = Alert::None;
bool alertFresh = false;
uint32_t alertSinceMs = 0;
bool ledOn = false;

constexpr uint32_t MINUTE = 60UL * 1000;
constexpr uint32_t ALERT_MAX_MS = 60UL * 1000;   // LED stops after a minute; screen stays until dismissed
constexpr uint32_t BLINK_MS = 400;
constexpr uint32_t MAX_COUNTDOWN = 99UL * 60 * MINUTE;

void raise(Alert a, uint32_t now) {
    currentAlert = a;
    alertFresh = true;
    alertSinceMs = now;
    Serial.printf("[timer] alert: %s\n", a == Alert::Countdown ? "countdown finished" : "focus phase finished");
}

void advanceFocusPhase() {
    if (fc.phase == Phase::Focus) {
        ++fc.completed;
        fc.phase = fc.completed >= SESSIONS_PER_CYCLE ? Phase::LongBreak : Phase::ShortBreak;
    } else {
        if (fc.phase == Phase::LongBreak) fc.completed = 0;
        fc.phase = Phase::Focus;
    }
    fc.state = State::Idle;   // next phase waits for the user to start it
    fc.remainingMs = focusPhaseLength(fc);
}

}  // namespace

// ---------------------------------------------------------------- countdown
Countdown& countdown() { return cd; }

uint32_t countdownRemaining(uint32_t now) {
    switch (cd.state) {
        case State::Running: return static_cast<int32_t>(cd.endMs - now) > 0 ? cd.endMs - now : 0;
        case State::Paused: return cd.remainingMs;
        case State::Done: return 0;
        default: return cd.durationMs;
    }
}

void countdownSet(uint32_t ms) {
    if (cd.state != State::Idle) return;
    cd.durationMs = constrain(ms, MINUTE, MAX_COUNTDOWN);
}

void countdownStart(uint32_t now) {
    const uint32_t ms = cd.state == State::Paused ? cd.remainingMs : cd.durationMs;
    cd.endMs = now + ms;
    cd.state = State::Running;
}

void countdownPause(uint32_t now) {
    if (cd.state != State::Running) return;
    cd.remainingMs = countdownRemaining(now);
    cd.state = State::Paused;
}

void countdownReset() {
    cd.state = State::Idle;
    if (currentAlert == Alert::Countdown) dismissAlert();
}

void countdownAddMinute(uint32_t now) {
    switch (cd.state) {
        case State::Running: cd.endMs += MINUTE; break;
        case State::Paused: cd.remainingMs += MINUTE; break;
        case State::Done:
            if (currentAlert == Alert::Countdown) dismissAlert();
            cd.endMs = now + MINUTE;
            cd.state = State::Running;
            break;
        case State::Idle: countdownSet(cd.durationMs + MINUTE); break;
    }
}

// ---------------------------------------------------------------- stopwatch
Stopwatch& stopwatch() { return sw; }

uint32_t stopwatchElapsed(uint32_t now) { return sw.accumulatedMs + (sw.running ? now - sw.startMs : 0); }

void stopwatchStart(uint32_t now) {
    if (sw.running) return;
    sw.startMs = now;
    sw.running = true;
}

void stopwatchStop(uint32_t now) {
    if (!sw.running) return;
    sw.accumulatedMs += now - sw.startMs;
    sw.running = false;
}

void stopwatchLap(uint32_t now) {
    const uint32_t t = stopwatchElapsed(now);
    for (int i = MAX_LAPS - 1; i > 0; --i) sw.laps[i] = sw.laps[i - 1];
    sw.laps[0] = t - sw.lastLapAt;
    sw.lastLapAt = t;
    ++sw.lapCount;
}

void stopwatchReset() { sw = Stopwatch(); }

// ---------------------------------------------------------------- focus
Focus& focus() { return fc; }

uint32_t focusPhaseLength(const Focus& f) {
    switch (f.phase) {
        case Phase::Focus: return f.focusMinutes * MINUTE;
        case Phase::ShortBreak: return 5 * MINUTE;
        case Phase::LongBreak: return 15 * MINUTE;
    }
    return 25 * MINUTE;
}

uint32_t focusRemaining(uint32_t now) {
    switch (fc.state) {
        case State::Running: return static_cast<int32_t>(fc.endMs - now) > 0 ? fc.endMs - now : 0;
        case State::Paused: return fc.remainingMs;
        default: return fc.remainingMs ? fc.remainingMs : focusPhaseLength(fc);
    }
}

void focusCycleMinutes() {
    if (fc.state != State::Idle || fc.phase != Phase::Focus) return;
    static const uint8_t OPTIONS[] = {15, 25, 30, 45, 50};
    int i = 0;
    while (i < 5 && OPTIONS[i] != fc.focusMinutes) ++i;
    fc.focusMinutes = OPTIONS[(i + 1) % 5];
    fc.remainingMs = 0;
}

void focusStart(uint32_t now) {
    if (currentAlert == Alert::Focus) dismissAlert();
    const uint32_t ms = fc.state == State::Paused ? fc.remainingMs : focusRemaining(now);
    fc.endMs = now + ms;
    fc.state = State::Running;
}

void focusPause(uint32_t now) {
    if (fc.state != State::Running) return;
    fc.remainingMs = focusRemaining(now);
    fc.state = State::Paused;
}

void focusSkip(uint32_t) {
    if (currentAlert == Alert::Focus) dismissAlert();
    advanceFocusPhase();
}

void focusReset() {
    if (currentAlert == Alert::Focus) dismissAlert();
    const uint8_t minutes = fc.focusMinutes;
    fc = Focus();
    fc.focusMinutes = minutes;
}

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::Focus: return "Focus";
        case Phase::ShortBreak: return "Short break";
        case Phase::LongBreak: return "Long break";
    }
    return "";
}

// ---------------------------------------------------------------- alerts
void loop(uint32_t now) {
    if (cd.state == State::Running && static_cast<int32_t>(cd.endMs - now) <= 0) {
        cd.state = State::Done;
        raise(Alert::Countdown, now);
    }
    if (fc.state == State::Running && static_cast<int32_t>(fc.endMs - now) <= 0) {
        advanceFocusPhase();
        raise(Alert::Focus, now);
    }

    // RGB LED: blink amber-ish (red+green) while an alert is active, max one minute.
    const bool blinking = currentAlert != Alert::None && now - alertSinceMs < ALERT_MAX_MS;
    const bool want = blinking && ((now - alertSinceMs) / BLINK_MS) % 2 == 0;
    if (want != ledOn) {
        ledOn = want;
        board::setLed(want, want, false);
    }
}

Alert alert() { return currentAlert; }

bool alertIsNew() {
    const bool fresh = alertFresh;
    alertFresh = false;
    return fresh;
}

void dismissAlert() {
    currentAlert = Alert::None;
    alertFresh = false;
    if (ledOn) {
        ledOn = false;
        board::setLed(false, false, false);
    }
}

}  // namespace timers
