#include "pc_link.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <string.h>
#include <time.h>

#include "model.h"
#include "version.h"

namespace pc_link {
namespace {

constexpr size_t   MAX_LINE            = 6144;
constexpr uint32_t CONNECTED_WINDOW_MS = 15000;
constexpr int      PROTOCOL_VERSION    = 1;

constexpr const char* NVS_NAMESPACE = "cal";
constexpr const char* NVS_KEY       = "snap";
constexpr uint32_t    SNAP_MAGIC    = 0x43414C31;   // "CAL1" - bump if Event layout changes

char line[MAX_LINE + 1];
size_t lineLen = 0;
bool overflow = false;

uint32_t lastRxMs = 0;
bool rxEver = false;

struct Snapshot {
    uint32_t magic;
    uint8_t  count;
    time_t   receivedAt;
    model::Event events[model::MAX_EVENTS];
};
uint32_t savedHash = 0;

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). "123456789" -> 0x29B1.
uint16_t crc16(const uint8_t* data, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : crc << 1;
    }
    return crc;
}

uint32_t fnv1a(const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
    return h;
}

void send(const JsonDocument& doc) {
    char buf[256];
    const size_t n = serializeJson(doc, buf, sizeof(buf));
    Serial.printf("@%s|%04X\n", buf, crc16(reinterpret_cast<const uint8_t*>(buf), n));
}

void sendHello() {
    JsonDocument d;
    d["t"] = "hello";
    d["v"] = PROTOCOL_VERSION;
    d["dev"] = "desk-companion";
    d["fw"] = version::full();
    send(d);
}

void sendAck(long s) {
    JsonDocument d;
    d["t"] = "ack";
    d["s"] = s;
    send(d);
}

void sendNak(long s, const char* err) {
    JsonDocument d;
    d["t"] = "nak";
    d["s"] = s;
    d["err"] = err;
    send(d);
}

// Copy at most dstLen-1 bytes without splitting a UTF-8 sequence.
void copyUtf8(char* dst, size_t dstLen, const char* src) {
    size_t n = strnlen(src, dstLen - 1);
    if (n == dstLen - 1)
        while (n > 0 && (static_cast<uint8_t>(src[n]) & 0xC0) == 0x80) --n;   // don't cut mid-character
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void saveSnapshot(const model::Calendar& c) {
    static Snapshot snap;   // ~1.5 KB, kept off the stack
    memset(&snap, 0, sizeof(snap));
    snap.magic = SNAP_MAGIC;
    snap.count = c.count;
    memcpy(snap.events, c.events, sizeof(model::Event) * c.count);
    const uint32_t h = fnv1a(snap.events, sizeof(snap.events)) ^ c.count;
    if (h == savedHash) return;   // unchanged: avoid needless flash writes every sync
    snap.receivedAt = c.receivedAt;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    prefs.putBytes(NVS_KEY, &snap, sizeof(snap));
    prefs.end();
    savedHash = h;
    Serial.printf("[pc] calendar snapshot saved (%u events)\n", c.count);
}

void loadSnapshot() {
    static Snapshot snap;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return;
    const bool ok = prefs.isKey(NVS_KEY) && prefs.getBytesLength(NVS_KEY) == sizeof(snap) &&
                    prefs.getBytes(NVS_KEY, &snap, sizeof(snap)) == sizeof(snap) && snap.magic == SNAP_MAGIC &&
                    snap.count <= model::MAX_EVENTS;
    prefs.end();
    if (!ok) return;
    model::Calendar& c = model::state().calendar;
    c.count = snap.count;
    memcpy(c.events, snap.events, sizeof(model::Event) * snap.count);
    for (int i = 0; i < c.count; ++i) c.events[i].title[sizeof(c.events[i].title) - 1] = '\0';
    c.receivedAt = snap.receivedAt;
    c.source = model::Source::Live;
    ++c.version;
    savedHash = fnv1a(snap.events, sizeof(snap.events)) ^ snap.count;
    Serial.printf("[pc] restored calendar snapshot (%u events)\n", c.count);
}

bool applyCalendar(JsonObjectConst msg) {
    JsonArrayConst ev = msg["ev"];
    if (ev.isNull()) return false;
    model::Calendar& c = model::state().calendar;
    c.count = 0;
    for (JsonObjectConst e : ev) {
        if (c.count >= model::MAX_EVENTS) break;
        const long long st = e["st"] | 0LL, en = e["en"] | 0LL;
        if (st <= 0 || en < st) continue;   // skip malformed entries
        model::Event& d = c.events[c.count++];
        copyUtf8(d.title, sizeof(d.title), e["ti"] | "");
        d.start = static_cast<time_t>(st);
        d.end = static_cast<time_t>(en);
        d.allDay = (e["ad"] | 0) == 1;
        d.calendar = e["c"] | 0;
    }
    model::sortCalendar(c);
    c.source = model::Source::Live;
    c.receivedAt = time(nullptr);
    ++c.version;
    Serial.printf("[pc] calendar: %u events\n", c.count);
    saveSnapshot(c);
    return true;
}

void applyMedia(JsonObjectConst m) {
    model::Media& d = model::state().media;
    copyUtf8(d.app, sizeof(d.app), m["app"] | "");
    copyUtf8(d.title, sizeof(d.title), m["ti"] | "");
    copyUtf8(d.artist, sizeof(d.artist), m["ar"] | "");
    d.state = m["st"] | 0;
    d.volume = static_cast<int8_t>(constrain(m["vol"] | -1, -1, 100));
    d.muted = (m["mu"] | 0) == 1;
    d.valid = true;
    ++d.version;
}

void applyStats(JsonObjectConst m) {
    model::PcStats& d = model::state().pc;
    d.cpu = static_cast<int8_t>(constrain(m["cpu"] | -1, -1, 100));
    d.ram = static_cast<int8_t>(constrain(m["ram"] | -1, -1, 100));
    d.gpu = static_cast<int8_t>(constrain(m["gpu"] | -1, -1, 100));
    d.gpuTemp = static_cast<int8_t>(constrain(m["gt"] | -1, -1, 127));
    d.ramUsedGb = m["ru"] | 0.0f;
    d.ramTotalGb = m["rt"] | 0;
    d.downBps = m["dn"] | 0UL;
    d.upBps = m["up"] | 0UL;
    d.valid = true;
    ++d.version;
}

uint32_t commandSeq = 0;

void handleLine() {
    if (lineLen < 7 || line[0] != '@') return;   // log line or noise
    char* bar = strrchr(line, '|');
    if (!bar || (line + lineLen) - bar != 5) return;
    const uint16_t want = static_cast<uint16_t>(strtoul(bar + 1, nullptr, 16));
    const size_t jsonLen = bar - (line + 1);
    if (crc16(reinterpret_cast<const uint8_t*>(line + 1), jsonLen) != want) {
        Serial.println(F("[pc] dropped frame: bad CRC"));
        return;
    }

    JsonDocument doc;
    if (const DeserializationError e = deserializeJson(doc, line + 1, jsonLen)) {
        Serial.printf("[pc] dropped frame: %s\n", e.c_str());
        return;
    }
    lastRxMs = millis();
    if (!rxEver) Serial.println(F("[pc] companion connected"));
    rxEver = true;

    const char* t = doc["t"] | "";
    const long s = doc["s"] | -1L;
    bool ok = true;
    if (strcmp(t, "hello") == 0) {
        const int v = doc["v"] | 0;
        Serial.printf("[pc] hello from %s %s (protocol v%d)\n", doc["app"] | "?", doc["ver"] | "?", v);
        if (v > PROTOCOL_VERSION) Serial.println(F("[pc] warning: companion speaks a newer protocol"));
        sendHello();
    } else if (strcmp(t, "ping") == 0) {
        // heartbeat: the ack below is the reply
    } else if (strcmp(t, "cal") == 0) {
        ok = applyCalendar(doc.as<JsonObjectConst>());
    } else if (strcmp(t, "media") == 0) {
        applyMedia(doc.as<JsonObjectConst>());
    } else if (strcmp(t, "stats") == 0) {
        applyStats(doc.as<JsonObjectConst>());
    } else if (strcmp(t, "ack") == 0 || strcmp(t, "nak") == 0) {
        if (strcmp(t, "nak") == 0) Serial.printf("[pc] command %ld rejected: %s\n", s, doc["err"] | "?");
        return;   // replies to our commands: never answer an answer
    } else {
        if (s >= 0) sendNak(s, "unknown type");
        return;
    }
    if (s >= 0) {
        if (ok) sendAck(s);
        else sendNak(s, "invalid payload");
    }
}

}  // namespace

void begin() {
    loadSnapshot();
    sendHello();
}

void loop(uint32_t) {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c < 0) break;
        if (c == '\n') {
            line[lineLen] = '\0';
            if (!overflow) handleLine();
            lineLen = 0;
            overflow = false;
        } else if (c == '\r') {
            continue;
        } else if (lineLen < MAX_LINE) {
            line[lineLen++] = static_cast<char>(c);
        } else {
            overflow = true;   // discard the rest of this line
        }
    }
}

void command(const char* action) {
    JsonDocument d;
    d["t"] = "cmd";
    d["s"] = static_cast<long>(++commandSeq);
    d["a"] = action;
    send(d);
}

void notify(const char* kind, const char* title, const char* text) {
    JsonDocument d;
    d["t"] = "alert";
    d["k"] = kind;
    d["ti"] = title;
    d["tx"] = text;
    send(d);
}

bool connected() { return rxEver && millis() - lastRxMs < CONNECTED_WINDOW_MS; }
bool everConnected() { return rxEver; }

}  // namespace pc_link
