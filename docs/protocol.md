# USB serial protocol (v1)

The Windows companion and the ESP32 talk over the CH340 USB-serial link at
**115200 baud, 8N1**. The same link also carries the firmware's human-readable
debug log, so protocol messages are framed so that either side can tell them apart.

## Opening the port without resetting the board

On the CYD, the USB bridge's RTS line drives the ESP32 reset (EN), and DTR drives
the boot pin. Most serial libraries assert both when opening a port, which **reboots
the device**. The companion must set `dtr = False` and `rts = False` **before**
opening the port (pyserial: configure the `Serial()` object, then call `open()`).

## Framing

One message per line:

```
@<json>|<crc>\n
```

| Part | Meaning |
|---|---|
| `@` | Start marker. Lines without it are log output and are ignored by the receiver. |
| `<json>` | A compact JSON object (no newlines), UTF-8. |
| `|` | Separator (the **last** `|` on the line, so `|` may appear inside JSON strings). |
| `<crc>` | CRC-16/CCITT-FALSE of the JSON bytes, 4 upper-case hex digits. Poly 0x1021, init 0xFFFF, no reflection, no final XOR. Check value: `"123456789"` → `29B1`. |

- Maximum line length: **6144 bytes**. Longer lines are discarded.
- Invalid CRC, invalid JSON or an unknown `t` → the message is dropped (and NAK'd if it had a sequence number).

## Common fields

| Field | Type | Meaning |
|---|---|---|
| `t` | string | Message type (required). |
| `s` | integer | Sequence number. Optional; if present the receiver replies `ack`/`nak` with the same `s`. |

## PC → device

### `hello`
```json
{"t":"hello","v":1,"app":"windows-companion","ver":"0.1.0"}
```
Sent after opening the port. The device answers with its own `hello`.

### `ping`
```json
{"t":"ping","s":17}
```
Heartbeat every 5 s. The device treats the PC as **connected** while any valid
message arrived in the last 15 s.

### `cal` (calendar snapshot)
```json
{"t":"cal","s":18,"tz":"America/Denver",
 "cals":[{"i":0,"n":"Parker"},{"i":1,"n":"Family"}],
 "ev":[{"ti":"Dinner with Sam's parents","st":1794100000,"en":1794107200,"ad":0,"c":0},
       {"ti":"Pay rent","st":1794182400,"en":1794268800,"ad":1,"c":1}]}
```
A complete replacement of the device's calendar (not a delta).

| Field | Meaning |
|---|---|
| `cals[].i` | Calendar index used by events; the device colours index 0..3 with its own palette. |
| `cals[].n` | Display name (informational). |
| `ev[].ti` | Title. Truncated by the device to 47 bytes (UTF-8 safe). |
| `ev[].st`, `ev[].en` | Start/end as Unix seconds. For all-day events: local midnight of the first day / of the day after the last day. |
| `ev[].ad` | 1 = all-day. |
| `ev[].c` | Calendar index. |

Window: today 00:00 local → +14 days, recurring events already expanded, sorted by
start. At most 24 events (the device keeps the first 24).

### `media` (now playing + volume)
```json
{"t":"media","app":"Spotify","ti":"Song title","ar":"Artist","st":1,"vol":38,"mu":0}
```
Sent when anything changes (polled every 1 s) and re-sent every 15 s. `st`: 0 = no
session, 1 = playing, 2 = paused. `vol` = master volume 0-100 (-1 unknown), `mu` = muted.
Source: Windows' media-session API and Core Audio, with no simulated key presses.

### `stats` (PC load)
```json
{"t":"stats","cpu":21,"ram":49,"ru":30.3,"rt":62,"dn":813,"up":13695,"gpu":32,"gt":44}
```
Every 2 s. Percentages 0-100; `ru`/`rt` = RAM used/total GB; `dn`/`up` = network
bytes/s; `gpu` = GPU load %, `gt` = GPU °C (`-1` = unavailable, e.g. no NVIDIA GPU).
Source: Windows performance APIs (psutil) and NVIDIA NVML. No kernel drivers.

## Device → PC

### `cmd` (button pressed on the device)
```json
{"t":"cmd","s":5,"a":"playpause"}
```
`a` = `playpause` | `next` | `prev` | `vol_up` | `vol_down` | `mute`. The PC replies
`ack`/`nak` with the same `s`, then pushes a fresh `media` message. The device treats
incoming `ack`/`nak` as replies and never answers them.

### `hello`
```json
{"t":"hello","v":1,"dev":"desk-companion","fw":"Nov  7 2026 21:30:00"}
```
Sent at boot and in reply to a PC `hello`.

### `alert`
```json
{"t":"alert","k":"timer","ti":"Time's up","tx":"Your timer has finished."}
```
Something needs the user's attention (`k` = `timer` or `focus`). The device has no
speaker, so the companion plays the Windows notification sound and, in tray mode, shows
a Windows notification. No ack is expected; it's ignored if no companion is listening.

### `ack` / `nak`
```json
{"t":"ack","s":18}
{"t":"nak","s":18,"err":"bad json"}
```

## Versioning

`v` in `hello` is the protocol version. A side that sees a higher major version than
it supports logs a warning and keeps sending only messages it knows. New message types
must be ignorable by older receivers.
