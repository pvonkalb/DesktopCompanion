"""USB serial connection to the device: discovery, reset-free open, reconnects.

The CYD's USB bridge wires RTS to the ESP32 reset line, so the port must be
opened with DTR/RTS de-asserted or every connection reboots the device.
"""

from __future__ import annotations

import logging
import threading
import time
from collections.abc import Callable

import serial
from serial.tools import list_ports

import protocol

log = logging.getLogger("serial")
device_log = logging.getLogger("device")

BAUD = 115200
KNOWN_BRIDGES = {(0x1A86, 0x7523), (0x10C4, 0xEA60)}   # CH340 (CYD), CP210x


def find_port(preferred: str | None = None) -> str | None:
    ports = list(list_ports.comports())
    if preferred and preferred.lower() != "auto":
        return preferred if any(p.device == preferred for p in ports) else None
    for p in ports:
        if (p.vid, p.pid) in KNOWN_BRIDGES:
            return p.device
    return None


class SerialLink:
    """Owns the port. Incoming messages go to `on_message`; device log lines to the log."""

    def __init__(self, port: str | None, on_message: Callable[[dict], None]):
        self._preferred = port
        self._on_message = on_message
        self._serial: serial.Serial | None = None
        self._lock = threading.Lock()
        self._reader: threading.Thread | None = None
        self.last_rx = 0.0

    @property
    def is_open(self) -> bool:
        return self._serial is not None and self._serial.is_open

    def open(self) -> bool:
        port = find_port(self._preferred)
        if not port:
            return False
        s = serial.Serial()
        s.port = port
        s.baudrate = BAUD
        s.timeout = 0.2
        s.write_timeout = 2
        s.dtr = False   # must be set BEFORE open(): asserting either line resets the ESP32
        s.rts = False
        try:
            s.open()
        except serial.SerialException as e:
            log.warning("cannot open %s: %s", port, e)
            return False
        self._serial = s
        self._reader = threading.Thread(target=self._read_loop, name="serial-reader", daemon=True)
        self._reader.start()
        log.info("connected on %s", port)
        return True

    def close(self) -> None:
        s, self._serial = self._serial, None
        if s:
            try:
                s.close()
            except serial.SerialException:
                pass
            log.info("disconnected")

    def send(self, message: dict) -> bool:
        data = protocol.encode(message)
        with self._lock:
            if not self.is_open:
                return False
            try:
                self._serial.write(data)
                return True
            except (serial.SerialException, OSError) as e:
                log.warning("write failed: %s", e)
        self.close()
        return False

    def _read_loop(self) -> None:
        buf = bytearray()
        while self.is_open:
            try:
                chunk = self._serial.read(512)
            except (serial.SerialException, OSError, TypeError, AttributeError) as e:
                log.warning("read failed: %s", e)
                self.close()
                return
            if not chunk:
                continue
            buf += chunk
            while (nl := buf.find(b"\n")) >= 0:
                line, buf = bytes(buf[:nl]), buf[nl + 1:]
                message = protocol.decode(line)
                if message is not None:
                    self.last_rx = time.monotonic()
                    try:
                        self._on_message(message)
                    except Exception:   # a handler bug must not kill the reader
                        log.exception("message handler failed")
                elif line.strip():
                    device_log.debug(line.decode("utf-8", errors="replace").rstrip())
            if len(buf) > protocol.MAX_LINE * 2:   # garbage without newlines
                buf.clear()
