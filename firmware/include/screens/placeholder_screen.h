#pragma once
// Stand-in for screens that aren't built yet: title, note, large Home button.

#include "ui/screen.h"
#include "ui/ui.h"

class PlaceholderScreen : public ui::Screen {
public:
    PlaceholderScreen(const char* title, const char* note) : title_(title), note_(note) {}
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;

private:
    void drawBack(TFT_eSPI& tft, bool pressed);
    const char* title_;
    const char* note_;
    bool backPressed_ = false;
};
