#pragma once
// Reusable widgets: screen header with back button, chevron buttons, and a
// button-group helper implementing the standard press behaviour:
//   press inside -> highlight, slide off -> cancel, release inside -> activate.

#include <TFT_eSPI.h>

#include "touch_input.h"
#include "ui/ui.h"

namespace ui {

constexpr int16_t HEADER_H = 44;
constexpr Rect    HEADER_BACK = {6, 4, 44, 36};

void drawChevron(TFT_eSPI& g, float cx, float cy, float size, bool pointLeft, uint16_t colour, uint16_t bg);
void drawChevronButton(TFT_eSPI& tft, const Rect& r, bool pointLeft, bool pressed);
void drawHeaderTitle(TFT_eSPI& tft, const char* title, int16_t rightLimit = 314);   // title area only
void drawHeader(TFT_eSPI& tft, const char* title, bool backPressed);                  // back + title

class ButtonGroup {
public:
    ButtonGroup(const Rect* rects, uint8_t count) : rects_(rects), count_(count) {}

    // draw(index, pressed) is called whenever a button's visual state changes.
    // Returns the index of the activated button, or -1.
    template <typename Draw>
    int handle(const touch_input::Event& ev, Draw&& draw) {
        using touch_input::EventType;
        switch (ev.type) {
            case EventType::Press:
                pressed_ = indexAt(ev.x, ev.y);
                if (pressed_ >= 0) draw(pressed_, true);
                break;
            case EventType::Move:
                if (pressed_ >= 0 && !rects_[pressed_].hit(ev.x, ev.y)) {
                    draw(pressed_, false);
                    pressed_ = -1;
                }
                break;
            case EventType::Release:
                if (pressed_ >= 0) {
                    const int i = pressed_;
                    pressed_ = -1;
                    draw(i, false);
                    if (rects_[i].hit(ev.x, ev.y)) return i;
                }
                break;
            default:
                break;
        }
        return -1;
    }

    void reset() { pressed_ = -1; }
    int pressed() const { return pressed_; }

private:
    int indexAt(int16_t x, int16_t y) const {
        for (uint8_t i = 0; i < count_; ++i)   // exact bounds first, then padded
            if (rects_[i].contains(x, y)) return i;
        for (uint8_t i = 0; i < count_; ++i)
            if (rects_[i].hit(x, y)) return i;
        return -1;
    }

    const Rect* rects_;
    uint8_t count_;
    int pressed_ = -1;
};

}  // namespace ui
