#pragma once
// Calendar: 7-day strip (dots = events, colour = source calendar) + agenda for
// the selected day. Arrows or a horizontal swipe change week; tapping the
// agenda pages through busy days.

#include <time.h>

#include "ui/screen.h"
#include "ui/widgets.h"

class CalendarScreen : public ui::Screen {
public:
    CalendarScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;

private:
    static constexpr int TARGETS = 11;   // back, prev, next, 7 day chips, agenda

    time_t dayAt(int index) const;       // local midnight of strip day 0..6
    int eventsOn(time_t dayStart, const int maxOut, int* out) const;
    int pageCount() const;

    void drawTitle(TFT_eSPI& tft);
    void drawStrip(TFT_eSPI& tft, int pressedChip = -1);
    void drawAgenda(TFT_eSPI& tft);
    void drawTarget(TFT_eSPI& tft, int index, bool pressed);
    void changeWeek(TFT_eSPI& tft, int delta);

    ui::Rect rects_[TARGETS];
    ui::ButtonGroup buttons_;
    int weekOffset_ = 0;
    int selected_ = 0;          // 0..6 within the strip
    int page_ = 0;
    int16_t pressX_ = 0, pressY_ = 0;
    time_t today_ = 0;
    int lastMinute_ = -1;
    uint32_t calendarVersion_ = 0;
    uint32_t lastCheckMs_ = 0;
};
