#pragma once
// Wi-Fi + location setup. Opens the phone setup portal and walks through:
// portal instructions (QR to join) -> connecting -> finding location -> done.

#include "ui/screen.h"
#include "ui/widgets.h"

class SetupScreen : public ui::Screen {
public:
    SetupScreen();
    void enter(TFT_eSPI& tft, uint32_t nowMs) override;
    void handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t nowMs) override;
    void update(TFT_eSPI& tft, uint32_t nowMs) override;
    bool idleReturnsHome() const override;

    enum class Phase : uint8_t { Portal, Connecting, Finding, Done, NotFound, Closed };

private:
    Phase currentPhase() const;
    void drawBody(TFT_eSPI& tft);
    void drawPortal(TFT_eSPI& tft);
    void drawMessage(TFT_eSPI& tft, const char* title, const char* line1, const char* line2, uint16_t accent);
    void drawRetry(TFT_eSPI& tft, bool pressed);

    ui::Rect rects_[2];   // back, retry
    ui::ButtonGroup buttons_;
    Phase phase_ = Phase::Portal;
    bool submittedOnce_ = false;
    uint32_t phaseSinceMs_ = 0;
};
