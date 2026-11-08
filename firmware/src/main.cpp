// Desktop Companion firmware.
// Current stage: Milestone 3c - weather screen (demo data).
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "board_cyd.h"
#include "demo_data.h"
#include "screens/calendar_screen.h"
#include "screens/dev_screens.h"
#include "screens/home_screen.h"
#include "screens/launcher_screen.h"
#include "screens/placeholder_screen.h"
#include "screens/settings_screen.h"
#include "screens/weather_screen.h"
#include "touch_calibration.h"
#include "touch_input.h"
#include "ui/screen.h"

namespace {

TFT_eSPI tft;  // pins/driver come from build_flags in platformio.ini

HomeScreen homeScreen;
CalendarScreen calendarScreen;
LauncherScreen launcherScreen;
SettingsScreen settingsScreen;
WeatherScreen weatherScreen;
PlaceholderScreen focusScreen("Focus", "Focus timer is on the roadmap");
PlaceholderScreen timerScreen("Timer", "Countdown timer is on the roadmap");
PlaceholderScreen musicScreen("Music", "Needs the optional PC companion");
PlaceholderScreen pcScreen("PC", "Needs the optional PC companion");
CalibrationScreen calibrationScreen;
TouchTestScreen touchTestScreen;

}  // namespace

void setup() {
    Serial.begin(115200);
    board::initPins();

    tft.init();
    tft.setRotation(board::DISPLAY_ROTATION);
    tft.fillScreen(theme::BG);
    touch_input::begin();

    touch_cal::Matrix cal;
    const bool haveCal = touch_cal::load(cal);
    if (haveCal) touch_input::setCalibration(cal);
    const bool forceCal = touch_input::irqActive();   // finger on the screen at boot = recalibrate

    // Until WiFi/NTP exist (Milestone 4): clock starts at build time, sample data.
    demo_data::setClockFromBuildTime();
    demo_data::load();

    using ui::ScreenId;
    ui::nav::add(ScreenId::Home, &homeScreen);
    ui::nav::add(ScreenId::Weather, &weatherScreen);
    ui::nav::add(ScreenId::Calendar, &calendarScreen);
    ui::nav::add(ScreenId::Launcher, &launcherScreen);
    ui::nav::add(ScreenId::Calibration, &calibrationScreen);
    ui::nav::add(ScreenId::TouchTest, &touchTestScreen);
    ui::nav::add(ScreenId::Focus, &focusScreen);
    ui::nav::add(ScreenId::Timer, &timerScreen);
    ui::nav::add(ScreenId::Music, &musicScreen);
    ui::nav::add(ScreenId::PcStats, &pcScreen);
    ui::nav::add(ScreenId::Settings, &settingsScreen);

    Serial.printf("[boot] Desktop Companion  heap=%lu largest=%lu  touch cal: %s%s\n",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getMaxAllocHeap()),
                  haveCal ? "stored" : "none (using defaults)", forceCal ? ", forced by touch" : "");

    ui::nav::begin(tft, !haveCal || forceCal ? ScreenId::Calibration : ScreenId::Home);
    board::setBacklight(true);   // reveal only after the first full frame is drawn
}

void loop() {
    ui::nav::loop(tft, millis());
}
