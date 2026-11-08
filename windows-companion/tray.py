"""Desk Companion - background tray app (no console window).

    .venv\\Scripts\\pythonw.exe tray.py                    # run in the tray
    .venv\\Scripts\\python.exe  tray.py --install-startup  # also start with Windows
    .venv\\Scripts\\python.exe  tray.py --remove-startup

Single instance, below-normal priority, pause releases the COM port.
"""

from __future__ import annotations

import argparse
import ctypes
import logging
import os
import sys
import threading
import time
import winreg
from ctypes import wintypes
from pathlib import Path

from PIL import Image, ImageDraw
import pystray

import paths
from main import APP_VERSION, Companion, load_config, play_alert_sound, setup_logging

log = logging.getLogger("tray")

APP_NAME = "Desk Companion"
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
RUN_VALUE = "DeskCompanion"
MUTEX_NAME = "Local\\DeskCompanion.SingleInstance"

CHARCOAL = (0x12, 0x14, 0x17, 255)
CYAN = (0x00, 0xD1, 0xFF, 255)
AMBER = (0xFF, 0xB0, 0x20, 255)
GREY = (0x8A, 0x92, 0x9C, 255)

# ---------------------------------------------------------------- Windows helpers

_mutex = None


def acquire_single_instance() -> bool:
    global _mutex
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateMutexW.restype = wintypes.HANDLE
    kernel32.CreateMutexW.argtypes = [wintypes.LPVOID, wintypes.BOOL, wintypes.LPCWSTR]
    _mutex = kernel32.CreateMutexW(None, False, MUTEX_NAME)
    return ctypes.get_last_error() != 183   # ERROR_ALREADY_EXISTS


def lower_priority() -> None:
    """Below-normal CPU priority so games always win.

    Types must be declared: GetCurrentProcess() returns the pseudo-handle -1, which
    ctypes' default int conversion truncates to 32 bits on 64-bit Windows, making the
    call fail silently (seen in testing).
    """
    BELOW_NORMAL_PRIORITY_CLASS = 0x00004000
    kernel32 = ctypes.windll.kernel32
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    kernel32.SetPriorityClass.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.SetPriorityClass.restype = wintypes.BOOL
    if kernel32.SetPriorityClass(kernel32.GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS):
        log.info("process priority: below normal")
    else:
        log.warning("could not lower process priority (error %d)", ctypes.GetLastError())


def message_box(text: str) -> None:
    ctypes.windll.user32.MessageBoxW(None, text, APP_NAME, 0x40)   # MB_ICONINFORMATION


def startup_command() -> str:
    here = Path(__file__).resolve().parent
    pythonw = here / ".venv" / "Scripts" / "pythonw.exe"
    return f'"{pythonw}" "{here / "tray.py"}"'


def startup_enabled() -> bool:
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY) as key:
            value, _ = winreg.QueryValueEx(key, RUN_VALUE)
            return value == startup_command()
    except OSError:
        return False


def set_startup(enabled: bool) -> None:
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
        if enabled:
            winreg.SetValueEx(key, RUN_VALUE, 0, winreg.REG_SZ, startup_command())
        else:
            try:
                winreg.DeleteValue(key, RUN_VALUE)
            except FileNotFoundError:
                pass
    log.info("start with Windows: %s", "on" if enabled else "off")


# ---------------------------------------------------------------- icon

def make_icon(ring: tuple, size: int = 64) -> Image.Image:
    """Charcoal rounded square with a coloured status ring - matches the device theme."""
    scale = 4   # draw large, downsample for smooth edges
    s = size * scale
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, s - 1, s - 1), radius=14 * scale, fill=CHARCOAL)
    pad, width = 13 * scale, 6 * scale
    d.ellipse((pad, pad, s - pad, s - pad), outline=ring, width=width)
    c, r = s / 2, 6 * scale
    d.ellipse((c - r, c - r, c + r, c + r), fill=ring)
    return img.resize((size, size), Image.LANCZOS)


ICONS = {"ok": make_icon(CYAN), "attention": make_icon(AMBER), "off": make_icon(GREY)}


# ---------------------------------------------------------------- tray app

class TrayApp:
    def __init__(self, companion: Companion):
        self.companion = companion
        self.icon = pystray.Icon(RUN_VALUE, ICONS["off"], APP_NAME, menu=self._menu())
        self._last_state = None
        companion.on_alert = self._notify   # timer / focus alerts from the device

    def _test_alert(self) -> None:
        play_alert_sound()
        self._notify("Desk Companion", "This is what a timer alert sounds like.")

    def _notify(self, title: str, text: str) -> None:
        if self.icon.HAS_NOTIFICATION:
            self.icon.notify(text or title, title)

    def _status_text(self) -> str:
        st = self.companion.status
        if st.paused:
            return "Paused - COM port released"
        if st.signin_needed:
            return "Google sign-in needed"
        if not st.device_connected:
            return "Device not connected"
        if st.last_sync:
            return f"Synced {st.events} events at {st.last_sync:%H:%M}"
        return "Connected - syncing..."

    def _state(self) -> str:
        st = self.companion.status
        if st.paused or not st.device_connected:
            return "off"
        if st.signin_needed or st.error:
            return "attention"
        return "ok"

    def _menu(self) -> pystray.Menu:
        Item = pystray.MenuItem
        return pystray.Menu(
            Item(lambda _: self._status_text(), None, enabled=False),
            pystray.Menu.SEPARATOR,
            Item("Sync now", lambda: self.companion.sync_now(), default=True),
            Item("Pause (free COM port)", self._toggle_pause, checked=lambda _: self.companion.status.paused),
            Item("Sign in to Google again", lambda: self.companion.sign_in_again()),
            Item("Test alert sound", lambda: self._test_alert()),
            Item("Open log", lambda: os.startfile(paths.data_dir() / "companion.log")),
            Item("Start with Windows", self._toggle_startup, checked=lambda _: startup_enabled()),
            pystray.Menu.SEPARATOR,
            Item(f"Quit (v{APP_VERSION})", self._quit),
        )

    def _toggle_pause(self) -> None:
        if self.companion.status.paused:
            self.companion.resume()
        else:
            self.companion.pause()

    def _toggle_startup(self) -> None:
        set_startup(not startup_enabled())

    def _quit(self) -> None:
        log.info("quit from tray")
        self.companion.stop()
        self.icon.stop()

    def _watch(self) -> None:
        """Keep the icon colour and tooltip in step with the companion."""
        while self.icon.visible or not self.companion._stop.is_set():
            state, text = self._state(), self._status_text()
            if (state, text) != self._last_state:
                self._last_state = (state, text)
                self.icon.icon = ICONS[state]
                self.icon.title = f"{APP_NAME} - {text}"[:127]
            time.sleep(2)
            if self.companion._stop.is_set():
                return

    def run(self) -> None:
        worker = threading.Thread(target=self.companion.run, name="companion", daemon=True)
        worker.start()
        threading.Thread(target=self._watch, name="tray-watch", daemon=True).start()
        self.icon.run()   # blocks until Quit
        worker.join(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(description="Desk Companion tray app")
    parser.add_argument("--install-startup", action="store_true", help="start with Windows (per user)")
    parser.add_argument("--remove-startup", action="store_true", help="stop starting with Windows")
    args = parser.parse_args()

    setup_logging(verbose=False, console=False)
    if args.install_startup or args.remove_startup:
        set_startup(args.install_startup)
        print(f"Start with Windows: {'on' if args.install_startup else 'off'}  ({startup_command()})")
        return 0

    if not acquire_single_instance():
        message_box("Desk Companion is already running - look for its icon in the system tray.")
        return 0
    lower_priority()

    config = load_config()
    from google_calendar import GoogleCalendarSource
    source = GoogleCalendarSource(config.get("calendars"), int(config.get("days", 14)))
    log.info("Desk Companion %s (tray) started", APP_VERSION)
    TrayApp(Companion(source, config)).run()
    log.info("Desk Companion stopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
