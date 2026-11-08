#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "backlight.h"
#include "settings.h"
#include "theme.h"
#include "ui/screen.h"
#include "ui/ui.h"
#include "version.h"

#ifndef OTA_PASSWORD
#error "OTA_PASSWORD missing: copy firmware/secrets.example.ini to secrets.ini"
#endif

namespace ota {
namespace {

using namespace theme;

TFT_eSPI* screen = nullptr;
bool started = false;
int lastPercent = -1;

constexpr int16_t BAR_X = 40, BAR_Y = 150, BAR_W = 240, BAR_H = 8;

void drawFrame(const char* title, const char* detail) {
    screen->fillScreen(BG);
    ui::region(*screen, 0, 70, 320, 70, BG, [&](TFT_eSPI& g, int16_t w, int16_t) {
        ui::setFont(g, ui::Font::Title);
        ui::text(g, title, w / 2, 22, MC_DATUM, TEXT, BG);
        ui::setFont(g, ui::Font::Label);
        ui::text(g, detail, w / 2, 50, MC_DATUM, TEXT_MUTED, BG);
    });
}

void drawProgress(int pct) {
    screen->fillSmoothRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 4, CARD_EDGE, BG);
    if (pct > 2) screen->fillSmoothRoundRect(BAR_X, BAR_Y, BAR_W * pct / 100, BAR_H, 4, ACCENT, CARD_EDGE);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    ui::region(*screen, 120, 166, 80, 24, BG, [&](TFT_eSPI& g, int16_t w, int16_t h) {
        ui::setFont(g, ui::Font::Body);
        ui::text(g, buf, w / 2, h / 2, MC_DATUM, TEXT, BG);
    });
}

void start() {
    ArduinoOTA.setHostname(settings::HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([] {
        Serial.println(F("[ota] update started"));
        backlight::suspend(true);
        lastPercent = -1;
        drawFrame("Updating firmware", "Keep the device powered - this takes about 30 s");
        drawProgress(0);
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        const int pct = total ? static_cast<int>(done * 100ULL / total) : 0;
        if (pct != lastPercent && (pct - lastPercent >= 2 || pct == 100)) {
            lastPercent = pct;
            drawProgress(pct);
        }
    });
    ArduinoOTA.onEnd([] {
        Serial.println(F("[ota] update complete - restarting"));
        drawFrame("Update installed", "Restarting...");
    });
    ArduinoOTA.onError([](ota_error_t e) {
        Serial.printf("[ota] update failed (error %u)\n", static_cast<unsigned>(e));
        drawFrame("Update failed", e == OTA_AUTH_ERROR ? "Wrong password" : "The previous firmware is still running");
        delay(2000);   // only reached on the error path, which already blocked the loop
        backlight::suspend(false);
        ui::nav::goHomeAndRedraw();   // plain go(Home) is a no-op when Home was already active
    });
    ArduinoOTA.begin();
    started = true;
    Serial.printf("[ota] ready: %s.local (%s), firmware %s\n", settings::HOSTNAME,
                  WiFi.localIP().toString().c_str(), version::full());
}

}  // namespace

void loop(TFT_eSPI& tft) {
    screen = &tft;
    if (!started) {
        if (WiFi.status() == WL_CONNECTED) start();
        return;
    }
    ArduinoOTA.handle();
}

}  // namespace ota
