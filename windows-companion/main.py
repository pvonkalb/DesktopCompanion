"""Desk Companion - Windows companion.

Keeps the device's calendar in sync over USB. Runs until stopped; survives the
device being unplugged/replugged and Windows sleep/resume.

    python main.py            # Google Calendar
    python main.py --demo     # sample events, no Google account needed
"""

from __future__ import annotations

import argparse
import json
import logging
import logging.handlers
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime

import paths
from serial_manager import SerialLink

APP_VERSION = "0.1.0"
PING_EVERY_S = 5
ACK_TIMEOUT_S = 3
RECONNECT_EVERY_S = 3
STATS_EVERY_S = 2
DEFAULT_CONFIG = {"port": "auto", "calendars": [], "days": 14, "sync_minutes": 5}

log = logging.getLogger("companion")


def load_config() -> dict:
    cfg = dict(DEFAULT_CONFIG)
    path = paths.config_file()
    if path:
        try:
            cfg.update(json.loads(path.read_text(encoding="utf-8")))
            log.info("config: %s", path)
        except (OSError, ValueError) as e:
            log.error("ignoring unreadable config %s: %s", path, e)
    return cfg


def setup_logging(verbose: bool, console: bool = True) -> None:
    fmt = logging.Formatter("%(asctime)s %(levelname)-7s %(name)-8s %(message)s", "%H:%M:%S")
    logfile = logging.handlers.RotatingFileHandler(paths.data_dir() / "companion.log", maxBytes=1_000_000,
                                                   backupCount=2, encoding="utf-8")
    logfile.setFormatter(fmt)
    logfile.setLevel(logging.DEBUG)   # includes the device's own debug log lines
    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    root.addHandler(logfile)
    if console and sys.stdout is not None:   # pythonw has no console
        stream = logging.StreamHandler(sys.stdout)
        stream.setFormatter(fmt)
        stream.setLevel(logging.DEBUG if verbose else logging.INFO)
        root.addHandler(stream)
    # Third-party HTTP/OAuth libraries log request bodies and token responses at DEBUG,
    # which includes access/refresh tokens and the client secret. Never let that reach
    # the log file: cap them at WARNING (and they log nothing secret at that level).
    for noisy in ("googleapiclient", "google", "google_auth_oauthlib", "requests_oauthlib", "oauthlib",
                  "requests", "urllib3", "httplib2"):
        logging.getLogger(noisy).setLevel(logging.WARNING)


def chime_file() -> "Path":
    """Our own two-note chime (generated once into the data dir), so the alert doesn't
    depend on the user's Windows sound scheme - the "System Notification" sound is often
    quiet or set to none."""
    import math
    import struct
    import wave

    path = paths.data_dir() / "chime.wav"
    if path.exists():
        return path
    rate, frames = 44100, bytearray()
    notes = [(880.0, 0.0), (1318.5, 0.16), (880.0, 0.75), (1318.5, 0.91)]   # A5-E6, twice
    length = 1.6
    for i in range(int(rate * length)):
        t = i / rate
        sample = 0.0
        for freq, start in notes:
            if t >= start:
                dt = t - start
                env = math.exp(-dt * 6.0) * min(1.0, dt * 200)   # quick attack, soft decay
                sample += env * (math.sin(2 * math.pi * freq * dt) + 0.2 * math.sin(4 * math.pi * freq * dt))
        frames += struct.pack("<h", int(max(-1.0, min(1.0, sample * 0.32)) * 32767))
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))
    return path


def play_alert_sound() -> None:
    """The device has no speaker, so timer alerts chime on the PC (async, non-blocking)."""
    try:
        import winsound
        winsound.PlaySound(str(chime_file()), winsound.SND_FILENAME | winsound.SND_ASYNC)
        log.info("alert chime played")
    except Exception as e:   # no audio device etc.
        log.warning("alert sound unavailable: %s", e)


@dataclass
class Status:
    """What the tray shows. Written by the companion thread, read by the UI."""
    device_connected: bool = False
    device_fw: str = ""
    last_sync: datetime | None = None
    events: int = 0
    error: str = ""
    signin_needed: bool = False
    paused: bool = False


class Companion:
    def __init__(self, source, config: dict):
        self._source = source
        self._sync_every = max(1, int(config["sync_minutes"])) * 60
        self._link = SerialLink(config["port"], self._on_message)
        self._seq = 0
        self._acks: dict[int, tuple[threading.Event, list]] = {}
        self._acks_lock = threading.Lock()
        self._device_hello = threading.Event()
        self._next_sync = 0.0
        self._stop = threading.Event()
        self._paused = threading.Event()
        self.status = Status()
        self.alert_sound = bool(config.get("alert_sound", True))
        self.on_alert = None   # set by the tray: (title, text) -> show a Windows notification
        from media import MediaController
        from pc_stats import StatsCollector
        self._stats = StatsCollector()
        self._next_stats = 0.0
        self._media = MediaController(self._send_media)

    # ---- control (any thread)
    def sync_now(self) -> None:
        self._next_sync = 0

    def pause(self) -> None:
        """Release the COM port (e.g. to flash firmware)."""
        self._paused.set()
        self.status.paused = True

    def resume(self) -> None:
        self._paused.clear()
        self.status.paused = False
        self._next_sync = 0

    def stop(self) -> None:
        self._stop.set()

    def sign_in_again(self) -> None:
        """Forget the saved Google sign-in; the next sync opens the browser."""
        token = paths.data_dir() / "token.json"
        token.unlink(missing_ok=True)
        if hasattr(self._source, "reset_auth"):
            self._source.reset_auth()
        self._next_sync = 0

    def _send_media(self, state: dict) -> None:
        """Called by the media thread when now-playing / volume changes."""
        if self._link.is_open and self._device_hello.is_set():
            self._link.send({"t": "media", **state})

    # ---- incoming (reader thread)
    def _on_message(self, msg: dict) -> None:
        kind = msg.get("t")
        if kind == "cmd":   # button pressed on the device
            ok = self._media.command(str(msg.get("a", "")))
            if "s" in msg:
                self._link.send({"t": "ack" if ok else "nak", "s": msg["s"], **({} if ok else {"err": "unknown"})})
            return
        if kind == "hello":
            log.info("device: %s firmware %s (protocol v%s)", msg.get("dev"), msg.get("fw"), msg.get("v"))
            self._device_hello.set()
            self.status.device_connected = True
            self.status.device_fw = str(msg.get("fw", ""))
            self._next_sync = 0   # (re)booted device: send the calendar straight away
            self._media.resend()
        elif kind == "alert":
            title, text = str(msg.get("ti", "Desk Companion")), str(msg.get("tx", ""))
            log.info("device alert (%s): %s - %s", msg.get("k"), title, text)
            if self.alert_sound:
                play_alert_sound()
            if self.on_alert:
                try:
                    self.on_alert(title, text)
                except Exception:
                    log.exception("alert notification failed")
        elif kind in ("ack", "nak"):
            with self._acks_lock:
                waiter = self._acks.get(msg.get("s"))
            if waiter:
                waiter[1].append(msg)
                waiter[0].set()
            if kind == "nak":
                log.warning("device rejected message %s: %s", msg.get("s"), msg.get("err"))

    # ---- outgoing
    def _request(self, message: dict) -> bool:
        """Send with a sequence number and wait for the device's ack."""
        self._seq = (self._seq + 1) % 1_000_000
        seq = self._seq
        message = {**message, "s": seq}
        done, replies = threading.Event(), []
        with self._acks_lock:
            self._acks[seq] = (done, replies)
        try:
            if not self._link.send(message):
                return False
            if not done.wait(ACK_TIMEOUT_S):
                log.warning("no ack for %s #%d", message["t"], seq)
                return False
            return replies[0].get("t") == "ack"
        finally:
            with self._acks_lock:
                self._acks.pop(seq, None)

    def _sync_calendar(self) -> None:
        try:
            payload = self._source.snapshot()
        except Exception as e:   # network down, token revoked, etc. - keep the link alive
            if type(e).__name__ == "SignInRequired":
                log.error("%s. Retrying in 15 minutes - or use 'Sign in to Google' to try now.", e)
                self.status.signin_needed = True
                self.status.error = "Google sign-in needed"
                self._next_sync = time.monotonic() + 15 * 60   # don't reopen a browser tab every minute
                return
            log.error("calendar fetch failed: %s", e)
            self.status.error = "Calendar fetch failed"
            self._next_sync = time.monotonic() + 60
            return
        self.status.signin_needed = False
        if self._request({"t": "cal", **payload}):
            log.info("calendar sent: %d events", len(payload["ev"]))
            self.status.last_sync = datetime.now()
            self.status.events = len(payload["ev"])
            self.status.error = ""
            self._next_sync = time.monotonic() + self._sync_every
        else:
            self.status.error = "Device didn't confirm the calendar"
            self._next_sync = time.monotonic() + 15

    def run(self) -> None:
        last_ping = 0.0
        while not self._stop.is_set():
            if self._paused.is_set():
                if self._link.is_open:
                    self._link.close()
                    log.info("paused - COM port released")
                self.status.device_connected = False
                self._stop.wait(0.5)
                continue
            if not self._link.is_open:
                self._device_hello.clear()
                self.status.device_connected = False
                if not self._link.open():
                    self._stop.wait(RECONNECT_EVERY_S)
                    continue
                self._link.send({"t": "hello", "v": 1, "app": "windows-companion", "ver": APP_VERSION})
                self._next_sync = 0

            now = time.monotonic()
            if now - last_ping >= PING_EVERY_S:
                last_ping = now
                if not self._request({"t": "ping"}) and not self._link.is_open:
                    continue
            if now >= self._next_stats and self._device_hello.is_set():
                self._next_stats = now + STATS_EVERY_S
                self._link.send({"t": "stats", **self._stats.sample()})
            if now >= self._next_sync:
                self._sync_calendar()
            self.status.device_connected = (self._link.is_open and self._device_hello.is_set()
                                            and time.monotonic() - self._link.last_rx < 15)
            self._stop.wait(0.25)
        self._link.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="Desk Companion Windows companion")
    parser.add_argument("--demo", action="store_true", help="send sample events instead of Google Calendar")
    parser.add_argument("--port", help="serial port (default: auto-detect the CYD)")
    parser.add_argument("-v", "--verbose", action="store_true", help="show device log lines on the console")
    args = parser.parse_args()

    setup_logging(args.verbose)
    config = load_config()
    if args.port:
        config["port"] = args.port
    log.info("Desk Companion %s - data dir %s", APP_VERSION, paths.data_dir())

    if args.demo:
        from google_calendar import DemoCalendarSource
        source = DemoCalendarSource()
    else:
        from google_calendar import GoogleCalendarSource
        source = GoogleCalendarSource(config.get("calendars"), int(config.get("days", 14)))

    try:
        Companion(source, config).run()
    except KeyboardInterrupt:
        log.info("stopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
