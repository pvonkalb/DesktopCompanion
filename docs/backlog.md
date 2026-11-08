# Backlog

Ideas worth keeping that aren't scheduled yet. Scheduled work lives in the README status table.

## Maybe later

| Idea | Notes |
|---|---|
| Temperature-coloured 7-day range bars | Blue (cool) to amber (warm) gradient instead of flat cyan. Deferred because the flat accent matches the current colour scheme. |
| Photo-frame mode | Show pictures from the SD card when idle / at night. Needs touch moved to software SPI (see hardware.md). |
| Offline cache on SD | Keep last weather + calendar so a reboot without WiFi still shows data. |
| Auto-dimming | Use the on-board light sensor (GPIO34) to dim the backlight at night. |
| Calendar over WiFi (no PC) | Google Apps Script "relay" under the user's account returns the next 14 days as small JSON; device fetches it over HTTPS. Lets the calendar work away from the PC (kitchen). The script URL is the secret (stored via the setup portal, revocable by redeploying). Google's keyboardless-device sign-in can't be used: it doesn't allow Calendar scopes. |

## To do when the personal website exists

- **Publish the Google OAuth app** (now in *Testing*: the sign-in expires every 7 days).
  Publishing requires a public home page + privacy policy URL on the Branding page. Add a
  Desk Companion project page and a short privacy note ("reads calendar read-only on your
  own PC; nothing leaves your PC except to the device over USB") to the website, enter
  both URLs, then Audience → Publish app. No logo, so no verification review is needed.

## Constraints for future features

- **PC stats must be anti-cheat safe.** Avoid kernel-driver sensor stacks (WinRing0 /
  LibreHardwareMonitor / OpenHardwareMonitor): some anti-cheat systems (e.g. Riot Vanguard,
  FACEIT) block or flag them. Prefer Windows performance counters (CPU %, RAM, network) and
  NVIDIA NVML (GPU % / temperature). CPU temperature: optional, or omitted.
- The companion should run at below-normal process priority (gaming PC).

## Decided

- Stopwatch lives as a second tab inside the Timer screen (`Timer | Stopwatch`), not its own tile.
