# Hardware: ESP32-2432S028R ("Cheap Yellow Display")

| Item | Value |
|---|---|
| MCU | ESP32-WROOM-32 (240 MHz dual core, 4 MB flash, no PSRAM) |
| Display | 2.8" 320×240 TFT, ILI9341 (single-USB board) or ST7789 (two-USB board) |
| Touch | XPT2046 resistive controller |
| USB | CH340 USB-serial bridge (Windows driver: "USB-SERIAL CH340", COM3 on the dev PC) |

## Pin map

### Display (HSPI)
| Signal | GPIO | Notes |
|---|---|---|
| SCLK | 14 | |
| MOSI | 13 | |
| MISO | 12 | Strapping pin (flash voltage). Safe here because the panel doesn't drive it at boot |
| CS | 15 | Strapping pin; fine as CS |
| DC | 2 | Strapping pin; fine as DC |
| RST | n/a | Wired to the ESP32 EN/reset line (`TFT_RST=-1`) |
| Backlight | 21 | Active HIGH, driven by LEDC PWM (5 kHz, 8-bit) for night dimming and fade transitions; owned by the firmware, not TFT_eSPI |

### Touch, XPT2046 (separate bus, VSPI)
| Signal | GPIO | Notes |
|---|---|---|
| CLK | 25 | |
| MOSI | 32 | |
| MISO | 39 | Input-only pin |
| CS | 33 | |
| IRQ | 36 | Input-only pin, LOW while touched |

**Important:** touch does **not** share the display's SPI bus. TFT_eSPI's built-in
touch support assumes a shared bus, so it cannot be used. Touch will use the
`XPT2046_Touchscreen` library on its own `SPIClass(VSPI)`. That's also why
`USE_HSPI_PORT` is set: it moves the display onto HSPI and leaves VSPI free.

**Verified 2026-11-07** (raw touch diagnostic, `XPT2046_Touchscreen` v1.4 on VSPI, library rotation 1):
IRQ goes LOW on touch; press and release are detected correctly.

| | Raw range seen at the panel edges | Direction vs. screen |
|---|---|---|
| X | 287 – 3678 | same (raw rises left → right) |
| Y | 294 – 3796 | same (raw rises top → bottom) |

(The first manual test seemed to show an inverted Y, but the calibration logs prove it isn't.
Calibration measures direction itself, so this never needs hard-coding.)

### Touch calibration
4 corner targets are fitted with a least-squares affine transform (this corrects inverted and
swapped axes and slight rotation). A 5th centre tap must land within 12 px, or calibration
reruns. The result is stored in NVS (`touch/cal`), so it survives reboots and reflashing.
To force recalibration, **touch the screen while the board boots**.

Dev unit results, 3 runs: centre error 5.1 / 7.0 / 6.6 px. Raw readings at the 24 px-inset
targets: top-left ≈ (440, 570), bottom-right ≈ (3440, 3370).

Using a stylus needs slightly firmer pressure than a finger (threshold `PRESSURE_MIN = 300`
in `touch_input.cpp`). That's acceptable since the UI is designed for fingers.

Pressure (z) while touched: min 562, 10th percentile 1387, median 2110. Noise threshold is 300.
The registry build of `XPT2046_Touchscreen` is an old snapshot without `begin(SPIClass&)`,
so `platformio.ini` pins the GitHub `v1.4` tag.

### Other on-board peripherals
| Peripheral | GPIO | Notes |
|---|---|---|
| RGB LED | R=4, G=16, B=17 | Active LOW; driven HIGH (off) at boot |
| Light sensor (LDR) | 34 | Analog input, **darker = higher**. Dev unit: room light 0, covered ~210-240. Night dimming: night above 150, day below 60, 8 s hold |
| Speaker connector | 26 | DAC/PWM |
| SD card | SCK 18, MISO 19, MOSI 23, CS 5 | **16 GB card installed** on the dev unit. Unused so far, see note below |

**SD vs. touch bus conflict:** the SD slot is wired to the default VSPI pins, but touch
already uses the VSPI peripheral on *different* pins (25/32/39/33). One SPI peripheral can't
drive two pin sets at once. Plan for when SD is needed: move the XPT2046 to a software
(bit-banged) SPI, since it only needs ~2 MHz, and give VSPI to the SD card.
Candidate uses: offline weather/calendar cache, logs, photo-frame mode.

## Display configuration

TFT_eSPI is configured via `build_flags` in `firmware/platformio.ini`
(`USER_SETUP_LOADED`), so the library files are never edited.

- `env:cyd`: `ILI9341_2_DRIVER` (original single micro-USB board)
- `env:cyd2usb`: `ST7789_DRIVER` + `TFT_INVERSION_ON` + BGR order (micro-USB + USB-C board)
- SPI clock: 40 MHz (conservative; 55 MHz is often fine and can be tried later)
- Rotation 1 = landscape, 320×240

**Verified 2026-11-07** on the dev unit: a single-USB board, `env:cyd` (ILI9341_2_DRIVER).
Colours, inversion, full-edge border, orientation and partial-redraw animation were all correct.
Full-screen fill takes 31 ms at 40 MHz SPI, which matches the theoretical ~30.7 ms.

## Reading the diagnostic screen

| Symptom | Cause | Fix |
|---|---|---|
| RED swatch shows blue (or BLUE shows red) | RGB/BGR order wrong | Add or remove `-DTFT_RGB_ORDER=TFT_BGR` |
| BLACK swatch is white, background is light | Inversion wrong | Toggle `-DTFT_INVERSION_ON` / try `env:cyd2usb` |
| Cyan border is missing on one or more edges, or there is a strip of noise | Wrong driver or offsets | Try the other env |
| Text is mirrored, or TL/TR are swapped | Wrong rotation | Try `DISPLAY_ROTATION = 3` in `board_cyd.h` |
| Screen stays black but serial log appears | Backlight or driver issue | Check GPIO21; try other env |
| Screen stays white | Panel not initialised: wrong pins or driver | Try other env |
| Image upside down but otherwise correct | Preference only | Rotation 3 instead of 1 |

## Troubleshooting USB

- In Device Manager the board should appear as **USB-SERIAL CH340 (COM3)** under *Ports*.
- If it doesn't appear at all, the cable is probably charge-only. Use a known data cable.
- **This board's automatic switch into download mode does not work** (verified 2026-11-07).
  esptool can reset the chip but can't hold the boot pin (IO0) low, so a normal upload fails with
  `Wrong boot mode detected (0x13)`. Pressing BOOT/RST *before* the upload doesn't help either,
  because esptool's own reset undoes it. What works: start the upload, then **hold BOOT** while
  it shows `Connecting....` (tap RST while holding if needed) and release once writing starts.
  With `--connect-attempts 0`, esptool keeps retrying until you do this.
- Close any serial monitor (including PuTTY) before uploading. Only one program can open COM3.
