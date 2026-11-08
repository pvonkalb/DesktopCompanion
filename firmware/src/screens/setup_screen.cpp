#include "screens/setup_screen.h"

#include <Arduino.h>
#include <WiFi.h>
#include <qrcode.h>   // ESP-IDF esp_qrcode

#include "device_config.h"
#include "net/network.h"

namespace {

using namespace theme;
using ui::Font;
using Phase = SetupScreen::Phase;

constexpr ui::Rect QR_AREA = {8, 50, 148, 148};
constexpr ui::Rect TEXT_AREA = {162, 48, 152, 188};
constexpr ui::Rect RETRY = {90, 176, 140, 44};
constexpr ui::Rect BODY = {0, ui::HEADER_H, 320, 240 - ui::HEADER_H};
constexpr uint32_t DONE_HOLD_MS = 3000;
constexpr uint32_t CONNECT_GIVE_UP_MS = 60000;   // after the network layer's 30 s rollback

// esp_qrcode reports the finished code through a plain callback.
TFT_eSPI* qrTarget = nullptr;

void drawQr(esp_qrcode_handle_t qr) {
    const int n = esp_qrcode_get_size(qr);
    const int scale = QR_AREA.w / (n + 4);            // 2-module quiet zone each side
    const int side = scale * (n + 4);
    const int x0 = QR_AREA.x + (QR_AREA.w - side) / 2, y0 = QR_AREA.y + (QR_AREA.h - side) / 2;
    qrTarget->fillRoundRect(x0, y0, side, side, 6, TFT_WHITE);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (esp_qrcode_get_module(qr, x, y))
                qrTarget->fillRect(x0 + (x + 2) * scale, y0 + (y + 2) * scale, scale, scale, TFT_BLACK);
}

}  // namespace

SetupScreen::SetupScreen() : buttons_(rects_, 2) {
    rects_[0] = ui::HEADER_BACK;
    rects_[1] = {0, 0, 0, 0};   // retry button only exists in the NotFound phase
}

// Only finished states may time out to Home; anything needing action stays on screen.
bool SetupScreen::idleReturnsHome() const { return phase_ == Phase::Done || phase_ == Phase::Closed; }

Phase SetupScreen::currentPhase() const {
    const bool online = net::link() == net::Link::Online;
    if (net::portalActive()) {
        if (!net::portalSubmitted()) return Phase::Portal;
        if (!online) return Phase::Connecting;
        return net::locationState() == net::LocationState::NotFound ? Phase::NotFound : Phase::Finding;
    }
    if (online) {
        switch (net::locationState()) {
            case net::LocationState::Resolving: return Phase::Finding;
            case net::LocationState::Ok: return Phase::Done;
            default: return Phase::NotFound;
        }
    }
    return submittedOnce_ ? Phase::Connecting : Phase::Closed;
}

void SetupScreen::enter(TFT_eSPI& tft, uint32_t nowMs) {
    if (!net::portalActive()) net::startPortal(false);
    submittedOnce_ = false;
    buttons_.reset();
    phase_ = currentPhase();
    phaseSinceMs_ = nowMs;
    tft.fillScreen(BG);
    ui::drawHeader(tft, "Wi-Fi setup", false);
    drawBody(tft);
}

void SetupScreen::drawMessage(TFT_eSPI& tft, const char* title, const char* line1, const char* line2,
                              uint16_t accent) {
    tft.fillRect(BODY.x, BODY.y, BODY.w, BODY.h, BG);
    ui::region(tft, 0, 70, 320, 100, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        g.fillSmoothRoundRect(w / 2 - 30, 0, 60, 4, 2, accent, BG);
        ui::setFont(g, Font::Title);
        ui::text(g, title, w / 2, 32, MC_DATUM, TEXT, BG);
        char buf[64];
        ui::setFont(g, Font::Body);
        ui::fitText(g, line1, w - 32, buf, sizeof(buf));
        ui::text(g, buf, w / 2, 62, MC_DATUM, TEXT_MUTED, BG);
        ui::setFont(g, Font::Label);
        ui::fitText(g, line2, w - 32, buf, sizeof(buf));
        ui::text(g, buf, w / 2, 86, MC_DATUM, TEXT_MUTED, BG);
    });
}

void SetupScreen::drawRetry(TFT_eSPI& tft, bool pressed) {
    const uint16_t fill = pressed ? ACCENT_ALT : theme::CARD;
    const char* label = device_config::location().query[0] ? "Try again" : "Enter city";
    ui::region(tft, RETRY.x, RETRY.y, RETRY.w, RETRY.h, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        g.fillSmoothRoundRect(0, 0, w, h, 10, fill, BG);
        ui::setFont(g, Font::Body);
        ui::text(g, label, w / 2, h / 2 + 1, MC_DATUM, TEXT, fill);
    });
}

void SetupScreen::drawPortal(TFT_eSPI& tft) {
    tft.fillRect(BODY.x, BODY.y, BODY.w, BODY.h, BG);

    char wifiQr[96];   // standard Wi-Fi QR payload understood by phone cameras
    snprintf(wifiQr, sizeof(wifiQr), "WIFI:T:WPA;S:%s;P:%s;;", net::portalSsid(), net::portalPassword());
    qrTarget = &tft;
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    cfg.display_func = drawQr;
    cfg.max_qrcode_version = 6;
    cfg.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
    esp_qrcode_generate(&cfg, wifiQr);

    ui::region(tft, TEXT_AREA.x, TEXT_AREA.y, TEXT_AREA.w, TEXT_AREA.h, BG, [&](TFT_eSPI& g, int16_t, int16_t) {
        ui::setFont(g, Font::Label);
        ui::text(g, "Scan with your phone,", 0, 16, L_BASELINE, TEXT_MUTED, BG);
        ui::text(g, "or join this Wi-Fi:", 0, 32, L_BASELINE, TEXT_MUTED, BG);
        ui::text(g, net::portalSsid(), 0, 52, L_BASELINE, TEXT, BG);
        ui::text(g, "Password", 0, 74, L_BASELINE, TEXT_MUTED, BG);
        ui::setFont(g, Font::Title);
        ui::text(g, net::portalPassword(), 0, 100, L_BASELINE, ACCENT, BG);
        ui::setFont(g, Font::Label);
        ui::text(g, "Then choose your Wi-Fi", 0, 128, L_BASELINE, TEXT_MUTED, BG);
        ui::text(g, "and enter your city.", 0, 144, L_BASELINE, TEXT_MUTED, BG);
        if (submittedOnce_) {
            ui::text(g, "Couldn't connect.", 0, 168, L_BASELINE, STATUS_WARN, BG);
            ui::text(g, "Check the password.", 0, 184, L_BASELINE, STATUS_WARN, BG);
        } else {
            ui::text(g, "No page? Open", 0, 168, L_BASELINE, TEXT_MUTED, BG);
            ui::text(g, WiFi.softAPIP().toString().c_str(), 0, 184, L_BASELINE, TEXT, BG);
        }
    });
}

void SetupScreen::drawBody(TFT_eSPI& tft) {
    rects_[1] = {0, 0, 0, 0};
    char line[64];
    switch (phase_) {
        case Phase::Portal:
            drawPortal(tft);
            break;
        case Phase::Connecting:
            drawMessage(tft, "Connecting\xE2\x80\xA6", "Joining your Wi-Fi network", "This takes a few seconds",
                        ACCENT);
            break;
        case Phase::Finding:
            snprintf(line, sizeof(line), "Looking up \"%s\"", device_config::location().query);
            drawMessage(tft, "Connected", line, net::ssid(), STATUS_OK);
            break;
        case Phase::Done:
            if (net::restoredPreviousWifi()) {
                snprintf(line, sizeof(line), "Kept your previous Wi-Fi: %s", net::ssid());
                drawMessage(tft, "Couldn't join", line, "Check the new network's password and try again",
                            STATUS_WARN);
            } else {
                drawMessage(tft, "All set", net::locationName(), "Time and weather will update shortly", STATUS_OK);
            }
            break;
        case Phase::NotFound:
            if (device_config::location().query[0])
                snprintf(line, sizeof(line), "Couldn't find \"%s\"", device_config::location().query);
            else
                snprintf(line, sizeof(line), "No city or ZIP code entered");
            drawMessage(tft, "Location needed", line, "Wi-Fi is connected - just the city is missing", STATUS_WARN);
            rects_[1] = RETRY;
            drawRetry(tft, false);
            break;
        case Phase::Closed:
            drawMessage(tft, "Setup closed", "Still trying the saved Wi-Fi",
                        "Open Settings > Wi-Fi & location to retry", TEXT_MUTED);
            break;
    }
}

void SetupScreen::handle(TFT_eSPI& tft, const touch_input::Event& ev, uint32_t) {
    const int hit = buttons_.handle(ev, [&](int i, bool pressed) {
        if (i == 0) ui::drawChevronButton(tft, ui::HEADER_BACK, true, pressed);
        else drawRetry(tft, pressed);
    });
    if (hit == 0) {
        net::stopPortal();
        ui::nav::back();
    } else if (hit == 1) {
        net::startPortal(false);
    }
}

void SetupScreen::update(TFT_eSPI& tft, uint32_t nowMs) {
    if (net::portalSubmitted()) submittedOnce_ = true;
    const Phase p = currentPhase();
    if (p != phase_) {
        Serial.printf("[setup] phase %u -> %u\n", static_cast<unsigned>(phase_), static_cast<unsigned>(p));
        phase_ = p;
        phaseSinceMs_ = nowMs;
        buttons_.reset();
        drawBody(tft);
    }
    if ((phase_ == Phase::Done || phase_ == Phase::Closed) && nowMs - phaseSinceMs_ > DONE_HOLD_MS)
        ui::nav::back();
    // Submitted credentials that never connect (wrong password): reopen the portal with a hint.
    if (phase_ == Phase::Connecting && !net::portalActive() && nowMs - phaseSinceMs_ > CONNECT_GIVE_UP_MS)
        net::startPortal(false);
}
