#pragma once
// Vector weather icons drawn with TFT_eSPI anti-aliased primitives.
// Draw them into a sprite (ui::region): overlapping shapes blend against
// whatever is already underneath, which needs a readable frame buffer.

#include <TFT_eSPI.h>

namespace ui {

// cx, cy = icon centre; size = overall width in px (~24..64 looks good)
// code = WMO weather code as used by Open-Meteo
void drawWeatherIcon(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint8_t code, bool isDay,
                     uint16_t bg);

const char* weatherLabel(uint8_t code);   // "Partly cloudy", "Rain", ...

enum class AppIcon : uint8_t { Calendar, Weather, Focus, Timer, Music, Pc, Settings };

// ~32 px launcher glyphs centred on (cx, cy); bg = tile colour underneath.
void drawAppIcon(TFT_eSPI& g, int16_t cx, int16_t cy, AppIcon icon, uint16_t bg);

}  // namespace ui
