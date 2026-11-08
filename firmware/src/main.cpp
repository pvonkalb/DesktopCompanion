// Desktop Companion firmware.
// Current stage: Milestone 7 - timer, stopwatch and focus.
#include <Arduino.h>
#include <TFT_eSPI.h>

#include "board_cyd.h"
#include "demo_data.h"
#include "net/network.h"
#include "pc_link.h"
#include "screens/calendar_screen.h"
#include "screens/dev_screens.h"
#include "screens/focus_screen.h"
#include "screens/home_screen.h"
#include "screens/timer_screen.h"
#include "timers.h"
#include "screens/launcher_screen.h"
#include "screens/placeholder_screen.h"
#include "screens/settings_screen.h"
#include "screens/setup_screen.h"
#include "screens/weather_screen.h"
#include "touch_calibration.h"
#include "model.h"
#include "settings.h"
#include "timefmt.h"
#include "touch_input.h"
#include "ui/screen.h"

namespace {

TFT_eSPI tft;  // pins/driver come from build_flags in platformio.ini

HomeScreen homeScreen;
CalendarScreen calendarScreen;
LauncherScreen launcherScreen;
SettingsScreen settingsScreen;
WeatherScreen weatherScreen;
SetupScreen setupScreen;
FocusScreen focusScreen;
TimerScreen timerScreen;
PlaceholderScreen musicScreen("Music", "Needs the optional PC companion");
PlaceholderScreen pcScreen("PC", "Needs the optional PC companion");
CalibrationScreen calibrationScreen;
TouchTestScreen touchTestScreen;

bool demoCalendarLoaded = false;
bool setupShownForPortal = false;

}  // namespace

void setup() {
    Serial.setRxBufferSize(8192);   // calendar snapshots arrive in bursts while the UI draws
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

    // Time + weather come from WiFi; the calendar comes from the Windows companion
    // over USB (last snapshot restored from flash until the PC sends a fresh one).
    net::begin();
    pc_link::begin();

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
    ui::nav::add(ScreenId::Setup, &setupScreen);

    Serial.printf("[boot] Desktop Companion  heap=%lu largest=%lu  touch cal: %s%s\n",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getMaxAllocHeap()),
                  haveCal ? "stored" : "none (using defaults)", forceCal ? ", forced by touch" : "");

    ui::nav::begin(tft, !haveCal || forceCal ? ScreenId::Calibration : ScreenId::Home);
    board::setBacklight(true);   // reveal only after the first full frame is drawn
}

void loop() {
    const uint32_t now = millis();
    net::loop(now);
    pc_link::loop(now);

    if (settings::DEMO_CALENDAR && !demoCalendarLoaded && timefmt::valid() &&
        model::state().calendar.source == model::Source::None) {   // UI testing without a PC
        demo_data::loadCalendar();
        demoCalendarLoaded = true;
    }

    // Show the setup screen whenever the portal opens on its own (first boot,
    // WiFi unreachable) - but never interrupt touch calibration.
    using ui::ScreenId;
    if (net::portalActive()) {
        const ScreenId cur = ui::nav::current();
        if (!setupShownForPortal && cur != ScreenId::Calibration && cur != ScreenId::Setup) {
            ui::nav::go(ScreenId::Setup);
            setupShownForPortal = true;
        }
    } else {
        setupShownForPortal = false;
    }

    // Timers run regardless of screen; when one finishes, show it (unless calibrating)
    // and let the PC companion play a sound / notification.
    timers::loop(now);
    if (timers::alertIsNew()) {
        const bool countdown = timers::alert() == timers::Alert::Countdown;
        const ScreenId target = countdown ? ScreenId::Timer : ScreenId::Focus;
        if (ui::nav::current() != ScreenId::Calibration && ui::nav::current() != target) ui::nav::go(target);
        if (countdown) {
            pc_link::notify("timer", "Time's up", "Your timer has finished.");
        } else {
            const bool breakNext = timers::focus().phase != timers::Phase::Focus;
            pc_link::notify("focus", breakNext ? "Break time" : "Focus time",
                            breakNext ? "Focus session complete - take a break." : "Break's over - ready to focus?");
        }
    }

    ui::nav::loop(tft, now);
}
