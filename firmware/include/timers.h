#pragma once
// Countdown timer, stopwatch and focus (pomodoro) cycles.
// Screen-independent: they keep running while you navigate, and raise an alert
// (screen + RGB LED) when a countdown or focus phase finishes. Driven by millis().

#include <stdint.h>

namespace timers {

enum class State : uint8_t { Idle, Running, Paused, Done };

// ---------------- countdown
struct Countdown {
    State    state = State::Idle;
    uint32_t durationMs = 10UL * 60 * 1000;
    uint32_t endMs = 0;          // valid while Running
    uint32_t remainingMs = 0;    // valid while Paused
};

Countdown& countdown();
uint32_t countdownRemaining(uint32_t nowMs);
void countdownSet(uint32_t durationMs);   // Idle only
void countdownStart(uint32_t nowMs);
void countdownPause(uint32_t nowMs);
void countdownReset();
void countdownAddMinute(uint32_t nowMs);  // works while Running/Paused/Done

// ---------------- stopwatch
constexpr int MAX_LAPS = 3;
struct Stopwatch {
    bool     running = false;
    uint32_t startMs = 0;        // when the current run segment began
    uint32_t accumulatedMs = 0;  // from previous segments
    uint32_t laps[MAX_LAPS] = {};   // most recent first (lap split times)
    uint8_t  lapCount = 0;          // total laps taken (may exceed MAX_LAPS)
    uint32_t lastLapAt = 0;         // elapsed time at the previous lap
};

Stopwatch& stopwatch();
uint32_t stopwatchElapsed(uint32_t nowMs);
void stopwatchStart(uint32_t nowMs);
void stopwatchStop(uint32_t nowMs);
void stopwatchLap(uint32_t nowMs);
void stopwatchReset();

// ---------------- focus
enum class Phase : uint8_t { Focus, ShortBreak, LongBreak };
constexpr uint8_t SESSIONS_PER_CYCLE = 4;

struct Focus {
    State    state = State::Idle;
    Phase    phase = Phase::Focus;
    uint8_t  focusMinutes = 25;
    uint8_t  completed = 0;      // focus sessions finished in this cycle
    uint32_t endMs = 0;
    uint32_t remainingMs = 0;
};

Focus& focus();
uint32_t focusPhaseLength(const Focus& f);
uint32_t focusRemaining(uint32_t nowMs);
void focusCycleMinutes();                 // Idle only: 15 -> 25 -> 30 -> 45 -> 50 -> 15
void focusStart(uint32_t nowMs);          // start / resume the current phase
void focusPause(uint32_t nowMs);
void focusSkip(uint32_t nowMs);           // end the current phase now (no alert)
void focusReset();
const char* phaseName(Phase p);

// ---------------- alerts
enum class Alert : uint8_t { None, Countdown, Focus };

void loop(uint32_t nowMs);     // completion checks + LED blinking (non-blocking)
Alert alert();
bool alertIsNew();             // true once per alert (used to jump to the right screen)
void dismissAlert();

}  // namespace timers
