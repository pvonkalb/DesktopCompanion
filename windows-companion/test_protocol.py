"""Protocol self-tests: python -m pytest windows-companion  (or run this file directly)."""

import protocol


def test_crc_check_value():
    assert protocol.crc16_ccitt(b"123456789") == 0x29B1   # CRC-16/CCITT-FALSE standard check value


def test_round_trip():
    msg = {"t": "cal", "s": 7, "ev": [{"ti": "Dinner | late", "st": 1, "en": 2, "ad": 0, "c": 1}]}
    line = protocol.encode(msg)
    assert line.startswith(b"@") and line.endswith(b"\n")
    assert protocol.decode(line) == msg   # '|' inside a string must not confuse the framing


def test_rejects_corruption_and_log_lines():
    line = bytearray(protocol.encode({"t": "ping", "s": 1}))
    line[5] ^= 0x01
    assert protocol.decode(bytes(line)) is None
    assert protocol.decode(b"[net] online: 'x' 1.2.3.4\n") is None


def test_device_text():
    assert protocol.device_text("Mom’s  birthday \U0001F382") == "Mom's birthday"
    long = "x" * 60
    assert len(protocol.device_text(long).encode()) == 47


if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn()
            print("ok ", name)
