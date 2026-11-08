# Desktop Companion: case study

*A glanceable smart display for the desk, built on a $15 ESP32 board, with a gaming-safe
Windows companion.*

## The goal

I wanted something between a clock and a phone: a small always-on screen that tells me the
time, the weather and what's next on our family calendar, runs timers, and controls music,
without picking up a phone or alt-tabbing out of a game. It also had to look like a finished
product, not a hobby project: dark, minimal, readable from across the room, and with no
flicker or jank on a 320×240 resistive touchscreen.

## The hardware

The ESP32-2432S028R ("Cheap Yellow Display") packs an ESP32, a 2.8" 320×240 TFT, resistive touch,
an RGB LED and a light sensor onto one board for about $15. The catch: its parts are wired in
non-obvious ways (touch on a separate SPI bus, reset tied to the USB serial lines), and only
~110 KB of contiguous RAM is available for drawing.

## Architecture

- **Device-first.** The ESP32 gets time (NTP) and weather (Open-Meteo, over verified HTTPS)
  over Wi-Fi by itself, so it's useful without a PC. Network work runs on the second CPU core;
  results are handed to the UI loop under a mutex, so touch stays responsive.
- **An optional PC companion** (Python, system tray) adds Google Calendar, music controls and
  PC stats over USB, using a small versioned protocol: one JSON message per line with a CRC,
  sharing the wire with human-readable logs.
- **No secrets in code.** Wi-Fi and location are entered from a phone through a QR-code setup
  page; Google sign-in lives only on the PC; the wireless-update password is in a gitignored file.

## Making it feel finished

- **Typography:** the stock fonts looked dated, so I wrote a converter that renders the Inter
  typeface into the display library's anti-aliased format at exactly the sizes the layout uses,
  plus fixed-width digits so ticking timers don't jiggle.
- **No flicker:** every dynamic region is drawn off-screen and pushed in one transfer; screens
  redraw only what changed. Screen changes dip the backlight and fade it back in, which hides
  the redraw and reads as a smooth transition.
- **Night mode:** the backlight runs on PWM; the light sensor (calibrated from measured data)
  dims the screen in a dark room, with hysteresis so a passing shadow doesn't flip it.
- **Wireless updates:** after the first install, new firmware goes over Wi-Fi with a progress
  screen. No more holding the BOOT button.

## Hard problems, solved on real hardware

Every feature was tested on the device and debugged from logs. A few stories:

**1. The stylus that tapped twice.** Users reported landing on the wrong screen. Logging every
touch with its pressure showed light stylus contact (pressure ≈ 450) drifting and briefly
"lifting", splitting one gesture into two. The fix combined pressure hysteresis (firm to start,
lighter to hold), a stability check on the first samples, padded touch targets and a short
input lockout after screen changes. Result: 22 taps, 22 correct navigations.

**2. "Connection reset" only during setup.** Weather lookups failed only while the phone
setup page was open. The log showed the home router at 192.168.4.1, which is exactly the
ESP32's default setup-network address. The device was sending its own traffic to itself. Moving
the setup network to 10.42.0.1 fixed it.

**3. Random reboots.** The device restarted during weather fetches. A secure (TLS) handshake
is several seconds of maths on this chip; at normal priority it starved the idle task and the
watchdog reset the board. Running the network worker at idle priority fixed it with no
loss of speed.

**4. A secret in a log file.** While testing Google sign-in, the HTTP library's debug output
wrote OAuth tokens into the companion's local log. I deleted the log, revoked the access,
replaced the OAuth client, capped those libraries' logging, and added a regression test that
fails if a token ever reaches the log again. (The same discipline caught the update tool echoing
the OTA password, which was rotated over the air.)

**5. Gaming-safe by design.** The PC runs anti-cheat software (Riot Vanguard, Easy Anti-Cheat).
So the companion uses no kernel drivers (no WinRing0-style sensor stacks), no simulated key
presses (music is controlled through Windows' own media-session API), and no hooks or overlays.
It runs at below-normal priority, and CPU temperature was deliberately left out.

## Results

- A daily-use device: clock, weather, shared calendar, timers, music and PC stats.
- Firmware ~1.3 MB, ~20 % RAM; UI never blocks on the network.
- Touch accuracy 5–7 px after calibration on a resistive panel.
- Updates over Wi-Fi in ~35 s; the PC companion keeps running throughout.

## Tech

C++ (Arduino-ESP32, FreeRTOS, TFT_eSPI, ArduinoJson, WiFiManager, ArduinoOTA) · Python (pyserial,
Google Calendar API, WinRT media control, pycaw, psutil, NVML, pystray) · PlatformIO · Open-Meteo.

*Built iteratively with an AI pair-programmer (Claude Code), milestone by milestone, with every
feature verified on hardware.*
