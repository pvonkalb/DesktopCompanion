"""Framing for the USB serial protocol (see docs/protocol.md).

Each message is one line:  @<compact json>|<CRC-16/CCITT-FALSE of the json, 4 hex>\\n
Lines without the leading '@' are the firmware's debug log.
"""

from __future__ import annotations

import json

PROTOCOL_VERSION = 1
MAX_LINE = 6144

# Glyphs the device fonts can draw beyond ASCII (tools/make_fonts.py EXTRAS).
_DEVICE_EXTRAS = set("°·•–…")
_REPLACEMENTS = str.maketrans({
    "‘": "'", "’": "'", "“": '"', "”": '"',
    "—": "–", " ": " ",
})


def crc16_ccitt(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF. b'123456789' -> 0x29B1."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def encode(message: dict) -> bytes:
    body = json.dumps(message, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    line = b"@" + body + b"|" + f"{crc16_ccitt(body):04X}".encode("ascii") + b"\n"
    if len(line) > MAX_LINE:
        raise ValueError(f"message too long for the device ({len(line)} > {MAX_LINE} bytes)")
    return line


def decode(line: bytes) -> dict | None:
    """Return the message dict, or None for log lines / corrupt frames."""
    line = line.rstrip(b"\r\n")
    if not line.startswith(b"@"):
        return None
    body, sep, crc_hex = line[1:].rpartition(b"|")
    if not sep or len(crc_hex) != 4:
        return None
    try:
        if int(crc_hex, 16) != crc16_ccitt(body):
            return None
        message = json.loads(body.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return None
    return message if isinstance(message, dict) and "t" in message else None


def device_text(text: str, max_bytes: int = 47) -> str:
    """Make a title drawable by the device fonts and fit its 47-byte field."""
    text = text.translate(_REPLACEMENTS)
    text = "".join(ch for ch in text if (" " <= ch <= "~") or ch in _DEVICE_EXTRAS)
    text = " ".join(text.split())
    encoded = text.encode("utf-8")
    if len(encoded) <= max_bytes:
        return text
    return encoded[:max_bytes].decode("utf-8", errors="ignore").rstrip()
