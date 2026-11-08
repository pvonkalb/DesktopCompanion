#pragma once
// Stand-in for screens that aren't built yet: header with back, centred note.

#include "ui/screen.h"
#include "ui/widgets.h"

class PlaceholderScreen : public ui::Screen {
public:
    PlaceholderScreen(const char* title, const char* note)
        : title_(title), note_(note), buttons_(&ui::HEADER_BACK, 1) {}
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;

private:
    const char* title_;
    const char* note_;
    ui::ButtonGroup buttons_;
};
