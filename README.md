# Desktop Companion

A desktop companion built on the ESP32-2432S028R "Cheap Yellow Display" (320×240 touchscreen),
connected to a Windows PC over USB.

## Layout

```
DesktopCompanion/
  firmware/          PlatformIO project (Arduino, TFT_eSPI)
    platformio.ini   board + TFT_eSPI display configuration
    include/         board_cyd.h (pin map), theme.h (design tokens), module headers
    src/             main.cpp, board + display modules
  docs/hardware.md   pin map, display config, troubleshooting
  assets/icons/
```

A Windows companion app (Python) is planned but not started.

## Status

| Milestone | State |
|---|---|
| 1. Display bring-up (landscape diagnostic screen) | **Verified on hardware** 2026-11-07 (single-USB board, `env:cyd`) |
| 2. Touch calibration | Not started |

## Build and flash

From `firmware/` (or use the PlatformIO toolbar in VS Code):

```
pio run -e cyd                 # build
pio run -e cyd -t upload       # flash via COM3
pio device monitor             # serial log @ 115200
```

**Uploading:** this board's auto-reset can't enter download mode. Start the upload, then
**hold BOOT** while it shows `Connecting....`. The upload waits indefinitely for this.

If colours look inverted, use `-e cyd2usb` instead (see docs/hardware.md).
