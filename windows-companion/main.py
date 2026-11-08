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

import paths
from serial_manager import SerialLink

APP_VERSION = "0.1.0"
PING_EVERY_S = 5
ACK_TIMEOUT_S = 3
RECONNECT_EVERY_S = 3
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


def setup_logging(verbose: bool) -> None:
    fmt = logging.Formatter("%(asctime)s %(levelname)-7s %(name)-8s %(message)s", "%H:%M:%S")
    console = logging.StreamHandler(sys.stdout)
    console.setFormatter(fmt)
    console.setLevel(logging.DEBUG if verbose else logging.INFO)
    logfile = logging.handlers.RotatingFileHandler(paths.data_dir() / "companion.log", maxBytes=1_000_000,
                                                   backupCount=2, encoding="utf-8")
    logfile.setFormatter(fmt)
    logfile.setLevel(logging.DEBUG)   # includes the device's own debug log lines
    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    root.addHandler(console)
    root.addHandler(logfile)
    # Third-party HTTP/OAuth libraries log request bodies and token responses at DEBUG,
    # which includes access/refresh tokens and the client secret. Never let that reach
    # the log file: cap them at WARNING (and they log nothing secret at that level).
    for noisy in ("googleapiclient", "google", "google_auth_oauthlib", "requests_oauthlib", "oauthlib",
                  "requests", "urllib3", "httplib2"):
        logging.getLogger(noisy).setLevel(logging.WARNING)


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

    # ---- incoming (reader thread)
    def _on_message(self, msg: dict) -> None:
        kind = msg.get("t")
        if kind == "hello":
            log.info("device: %s firmware %s (protocol v%s)", msg.get("dev"), msg.get("fw"), msg.get("v"))
            self._device_hello.set()
            self._next_sync = 0   # (re)booted device: send the calendar straight away
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
                log.error("%s. Retrying in 15 minutes - or restart the companion to try again now.", e)
                self._next_sync = time.monotonic() + 15 * 60   # don't reopen a browser tab every minute
                return
            log.error("calendar fetch failed: %s", e)
            self._next_sync = time.monotonic() + 60
            return
        if self._request({"t": "cal", **payload}):
            log.info("calendar sent: %d events", len(payload["ev"]))
            self._next_sync = time.monotonic() + self._sync_every
        else:
            self._next_sync = time.monotonic() + 15

    def run(self) -> None:
        last_ping = 0.0
        while True:
            if not self._link.is_open:
                self._device_hello.clear()
                if not self._link.open():
                    time.sleep(RECONNECT_EVERY_S)
                    continue
                self._link.send({"t": "hello", "v": 1, "app": "windows-companion", "ver": APP_VERSION})
                self._next_sync = 0

            now = time.monotonic()
            if now - last_ping >= PING_EVERY_S:
                last_ping = now
                if not self._request({"t": "ping"}) and not self._link.is_open:
                    continue
            if now >= self._next_sync:
                self._sync_calendar()
            time.sleep(0.25)


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
