# Desktop Companion

A standalone smart display built on the ESP32-2432S028R "Cheap Yellow Display"
(320×240 resistive touchscreen). It's an always-on clock, weather and shared-calendar glance
screen for the desk or kitchen, with optional PC stats and media controls when it's
plugged into a Windows PC.

![Font preview](assets/fonts/preview.png)

## Layout

```
DesktopCompanion/
  firmware/            PlatformIO project (Arduino, TFT_eSPI)
    platformio.ini     board + TFT_eSPI display configuration
    include/, src/     modules (see docs/architecture.md)
    scripts/           build helpers
  tools/make_fonts.py  TTF -> anti-aliased TFT_eSPI fonts
  assets/fonts/        Inter (SIL OFL) + generated preview
  docs/
    architecture.md    system + firmware design
    hardware.md        pin map, display/touch config, troubleshooting
    backlog.md         ideas parked for later
```

`windows-companion/` (Python) syncs Google Calendar to the device over USB; see its README.
Protocol: docs/protocol.md.

## Status

| Milestone | State |
|---|---|
| 1. Display bring-up (landscape diagnostic screen) | **Verified on hardware** 2026-11-07 (single-USB board, `env:cyd`) |
| 2. Touch input + calibration (NVS-stored, 5-point verified) | **Verified on hardware** 2026-11-07 (5–7 px centre error) |
| 3a. Screen framework, fonts, Home screen (demo data) | **Verified on hardware** 2026-11-07 |
| 3b. Calendar (week strip + agenda), Apps launcher, Settings, back navigation | **Verified on hardware** 2026-11-07 |
| 3c. Weather screen: Hourly (12 h) + 7 Day tabs | **Verified on hardware** 2026-11-07 |
| 4. WiFi + location setup from a phone, NTP time, live Open-Meteo weather | **Verified on hardware** 2026-11-07 |
| 5. Google Calendar via the Windows companion (USB protocol v1) | **Verified on hardware** 2026-11-07 |
| 6. Companion tray app: background, starts with Windows, below-normal priority | **Verified** 2026-11-07 |
| 7. Timer + stopwatch, focus (pomodoro); alerts on screen, RGB LED and PC chime | **Verified on hardware** 2026-11-07 |
| 8. Music controls + PC stats (anti-cheat safe), Return-to-Home setting | **Verified on hardware** 2026-11-07 |
| 9. Wireless updates (OTA), night auto-dim, screen fade transitions, versioning | **Verified on hardware** 2026-11-07 |
| 10. Portfolio write-up | Next |

## Build and flash

From `firmware/` (or use the PlatformIO toolbar in VS Code). First copy
`firmware/secrets.example.ini` to `firmware/secrets.ini` and set a long random OTA password.

```
pio run -e cyd_ota -t upload   # normal: wireless update over the home network
pio run -e cyd -t upload       # first install / recovery: USB (COM3)
```

**Wireless (OTA):** password-protected, local network only; the device shows a progress
screen. The first time, allow Python through Windows Firewall when asked.

**USB:** this board's auto-reset can't enter download mode. Start the upload, then
**hold BOOT** while it shows `Connecting....`. Stop the companion tray app first (it owns COM3).

**First-time setup:** the device opens a setup Wi-Fi network and shows a QR code. Join it with a phone,
pick your Wi-Fi and enter a city or ZIP code. Change it later under Settings > Wi-Fi & location.

**Recalibrate touch:** Settings > Recalibrate touch, or hold a finger on the screen while the board powers up.

If colours look inverted, use `-e cyd2usb` instead (see docs/hardware.md).
